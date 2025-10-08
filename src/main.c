#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client_cxt.h"
#include "helping_hand.h"
#include "mb_base.h"
#include "tui.h"
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

    // clear all pesky leftovers
    u8 garbage[MB_MAX_ADU_LEN];
    read(globals.cxt.fd, &garbage, MB_MAX_ADU_LEN);

    globals.stats.requests++;
    int bytes_send = write(globals.cxt.fd, adu, adu_len);
    if (bytes_send > 0) {
        log_adu(&globals, adu, adu_len, frame->protocol, DS_OUT_OK);
        return RC_SUCCESS;
    } else {
        // TOOD: make more informative error message
        log_traffic_str(&globals, "failed to send", DS_OUT_FAIL);
        return RC_FAIL;
    }
}

int
read_nonblock(u8 out[MB_MAX_ADU_LEN], int *out_len, u64 tstart) {
    u32 pos      = 0;
    u32 expected = 0;

    while (1) {
        expected = mb_get_expected_adu_len(globals.cxt.protocol, out, pos, MB_DIR_RESPONSE);
        if (expected < 0) {
            return RC_FAIL;
        } else if (expected == 0) {
            expected = MB_MAX_ADU_LEN;
        }

        int add = read(globals.cxt.fd, &out[pos], expected);
        if (add > 0) {
            pos += add;
        }

        if (now_ms() - tstart > globals.response_timeout) {
            log_traffic_str(&globals, "timed out", DS_IN_FAIL);
            globals.stats.timeouts++;
            return RC_FAIL;
        }

        if (pos >= expected) {
            *out_len = pos;
            return RC_SUCCESS;
        }
    }
}

int
recv_response(frame_t *req_frame) {
    // (almost) immediately after message sent, can count time for response arrival
    u64 start = now_ms();

    u8  adu[MB_MAX_ADU_LEN] = {0};
    int adu_len             = 0;

    if (!read_nonblock(adu, &adu_len, start)) {
        return RC_FAIL;
    }

    if (!mb_is_adu_valid(req_frame->protocol, adu, adu_len)) {
        frame_t rsp_frame = {0};
        mb_extract_frame(req_frame->protocol, adu, adu_len, &rsp_frame);

        // can be response that we already count as 'timed out, skip it
        // NN after add of pre send cleanup?
        if (req_frame->tid != rsp_frame.tid) {
            log_adu(&globals, adu, adu_len, rsp_frame.protocol, DS_IN_FAIL);
            log_linef("get %d tid, expected %d", rsp_frame.tid, req_frame->tid);
            return RC_FAIL;
        }

        if (check_req_rsp_pdu(req_frame->pdu, req_frame->pdu_len, rsp_frame.pdu, rsp_frame.pdu_len)) {
            log_adu(&globals, adu, adu_len, rsp_frame.protocol, DS_IN_OK);
            globals.stats.success++;
            return RC_SUCCESS;
        } else {
            // 192.168.5.225:502 <x failed to send requesto
            log_adu(&globals, adu, adu_len, rsp_frame.protocol, DS_IN_FAIL);
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
      .uid      = globals.slave_id_start,
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
    log_line("> tui started");

    open_uplink(&globals);

    pthread_t tinput;
    pthread_create(&tinput, NULL, input_thread, (void *)&globals);

    while (1) {
        if (globals.running) {
            make_request();
        }

        msleep(globals.timeout);
    }

    // pthread_exit(NULL);
    getchar();

    endwin();
    return 0;
}
