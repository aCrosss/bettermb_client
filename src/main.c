#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client_cxt.h"
#include "mb_base.h"
#include "screen.h"
#include "types.h"
#include "uplink.h"

global_t globals = {0};

int
send_request(frame_t *frame) {
    func_cxt_t fcxt = {
      .fc       = globals.cxt.fc,
      .raddress = globals.cxt.raddress,
      .rcount   = globals.cxt.rcount,
    };
    u8 data[1] = {0};

    int pdu_len = build_pdu(frame->pdu, data, fcxt);
    if (pdu_len > 0) {
        frame->pdu_len = pdu_len;
    } else {
        log_line("! failed to build pdu");
        return RC_FAIL;
    }

    u8 adu[MB_MAX_ADU_LEN] = {0};

    int adu_len = build_adu(adu, frame);
    if (adu_len <= 0) {
        log_line("! failed to build adu");
        return RC_FAIL;
    }

    globals.stats.requests++;
    int bytes_send = write(globals.cxt.fd, adu, adu_len);
    // int bytes_send = send(globals.cxt.fd, adu, adu_len, 0);
    if (bytes_send > 0) {
        char endp[32] = {0};
        str_curr_endpoint(endp, &globals);
        log_adu(endp, adu, adu_len, frame->protocol, DS_OUT_OK);
        return RC_SUCCESS;
    } else {
        // TOOD: make more informative error message
        log_line("! failed to send request");
        return RC_FAIL;
    }
}

int
recv_response(frame_t *req_frame) {
    u8 adu[MB_MAX_ADU_LEN] = {0};

    int adu_len = read(globals.cxt.fd, adu, MB_MAX_ADU_LEN);
    // int adu_len = recv(globals.cxt.fd, adu, MB_MAX_ADU_LEN, 0);
    if (adu_len < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            // timed out
            log_line("! response timed out");
            globals.stats.timeouts++;
            return RC_FAIL;
        } else {
            // realy screwed
            globals.stats.fails++;
            log_line("! response error");
        }
    }

    if (!mb_is_adu_valid(req_frame->protocol, adu, adu_len)) {
        frame_t rsp_frame = {0};
        mb_extract_frame(req_frame->protocol, adu, adu_len, &rsp_frame);

        if (check_req_rsp_pdu(req_frame->pdu, req_frame->pdu_len, rsp_frame.pdu, rsp_frame.pdu_len)) {
            char endp[32] = {0};
            str_curr_endpoint(endp, &globals);
            log_adu(endp, adu, adu_len, rsp_frame.protocol, DS_IN_OK);
            globals.stats.success++;
            return RC_SUCCESS;
        } else {
            // 192.168.5.225:502 <x failed to send requesto
            char endp[32] = {0};
            str_curr_endpoint(endp, &globals);
            log_adu(endp, adu, adu_len, rsp_frame.protocol, DS_IN_FAIL);
            globals.stats.fails++;
            log_line("! recieved response is invalid");
            return RC_FAIL;
        }
    }
}

int
make_request() {
    u16 tid = 0;
    if (globals.cxt.protocol == MB_PROTOCOL_TCP) {
        tid = globals.cxt.tid++;
    }
    frame_t frame = {
      .protocol = globals.cxt.protocol,
      .fc       = globals.cxt.fc,
      .uid      = 1,
      .tid      = tid,
    };

    if (!send_request(&frame)) {
        return RC_FAIL;
    }

    recv_response(&frame);

    // update statistic output
    redraw_header(&globals);
}

int
main(int argc, char *argv[]) {
    if (init_client(argc, argv, &globals) < 0) {
        return -1;
    }

    init_screen(&globals);
    log_line("> screen initialized...");

    open_uplink(&globals);

    pthread_t tinput;
    pthread_create(&tinput, NULL, input_thread, (void *)&globals);

    while (1) {
        if (globals.running) {
            make_request();
        }

        // msleep(globals.timeout);
        // getchar();
        sleep(1);
    }

    // pthread_exit(NULL);
    getchar();

    endwin();
    return 0;
}
