#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#include "client_cxt.h"
#include "helping_hand.h"
#include "mb_base.h"
#include "tui.h"
#include "types.h"
#include "uplink.h"

global_t globals = {0};

// -------------------- Request section ---------------------------------------------------

int
build_wdata_bits(func_cxt_t *fcxt, u8 data[MB_MAX_WRITE_BITS]) {
    int to_write = CLAMP(globals.cxt.wcount, 0, MB_MAX_WRITE_BITS);
    fcxt->wcount = to_write;

    if (globals.random) {
        for (int i = 0; i < to_write; i++) {
            data[i] = rand() % 2;
        }
    } else {
        to_write = CLAMP(to_write, 0, WD_MAX_LEN);
        for (int i = 0; i < to_write; i++) {
            // write what we have, everything else will be 0
            data[i] = globals.cxt.wdata[i];
        }
    }
}

int
build_wdata_regs(func_cxt_t *fcxt, u8 data[MB_MAX_WRITE_BITS]) {
    int  fflags    = fc_flags(fcxt->fc);
    int  max_write = fflags & FCF_READ ? MB_MAX_WR_WRITE_REGS : MB_MAX_WRITE_REGS;
    u16 *write     = (void *)data;

    int to_write = CLAMP(globals.cxt.wcount, 0, max_write);
    fcxt->wcount = to_write;

    if (globals.random) {
        for (int i = 0; i < to_write; i++) {
            write[i] = rand() % 0xFFFF;
        }
    } else {
        for (int i = 0; i < to_write; i++) {
            // write what we have, everything else will be 0
            write[i] = globals.cxt.wdata[i];
        }
    }
}

int
send_request(frame_t *frame) {
    func_cxt_t fcxt = {
      .fc = globals.cxt.fc,
    };
    // can't write anything more than that anyway
    u8 wdata[MB_MAX_WRITE_BITS] = {0};

    // build data for request
    int fflag = fc_flags(fcxt.fc);
    if (fflag & FCF_READ) {
        fcxt.raddress = globals.cxt.raddress;
        fcxt.rcount   = globals.cxt.rcount;
    }
    if (fflag & FCF_WRITE) {
        fcxt.waddress = globals.cxt.waddress;
        if (fflag & FCF_BITS) {
            build_wdata_bits(&fcxt, wdata);
        } else {
            build_wdata_regs(&fcxt, wdata);
        }
    }

    // build pdu
    int pdu_len = build_pdu(frame->pdu, wdata, fcxt);
    if (pdu_len > 0) {
        frame->pdu_len = pdu_len;
    } else {
        return RC_FAIL;
    }

    // build adu
    u8 adu[MB_MAX_ADU_LEN] = {0};

    int adu_len = build_adu(adu, frame);
    if (adu_len <= 0) {
        return RC_FAIL;
    }

    // try write to fd
    globals.stats.requests++;
    int bytes_send = write(globals.cxt.fd, adu, adu_len);
    /* it's my homie, mr. write*/

    // clear all pesky leftovers
    tcflush(globals.cxt.fd, TCIFLUSH);

    if (bytes_send > 0) {
        log_adu(adu, adu_len, frame->protocol, DS_OUT_OK);
        return RC_SUCCESS;
    } else if (errno == EPIPE || errno == ENOTTY) {
        log_linef("! bad fd: %s", strerror(errno));
        log_linef("! client request handling stopped, fd closed");

        globals.running = FALSE;
        close(globals.cxt.fd);
        globals.cxt.fd = -1;
        redraw_header();
        return RC_FAIL;
    } else {
        log_traffic_str("failed to send", DS_OUT_FAIL);
        return RC_FAIL;
    }
}

// -------------------- Response section --------------------------------------------------

int
read_nonblock(u8 out[MB_MAX_ADU_LEN], int *out_len) {
    u32 pos                  = 0;
    u8  buff[MB_MAX_ADU_LEN] = {0};
    int temp_len             = 0;
    u16 expected             = 0;

    while (1) {
        expected = mb_get_expected_adu_len(globals.cxt.protocol, out, pos, MB_DIR_RESPONSE);
        if (expected < 0) {
            return RC_FAIL;
        } else if (expected == 0) {
            expected = MB_MAX_ADU_LEN;
        }

        int add = read(globals.cxt.fd, &out[pos], expected - pos);
        if (add > 0) {
            pos += add;
        }

        if (now_ms() - globals.time_start > globals.response_timeout) {
            log_traffic_str("timed out", DS_IN_FAIL);
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
    u8  adu[MB_MAX_ADU_LEN] = {0};
    int adu_len             = 0;

    if (!read_nonblock(adu, &adu_len)) {
        return RC_FAIL;
    }

    int verr = mb_is_adu_valid(req_frame->protocol, adu, adu_len);
    if (verr == MB_VALIDATION_ERROR_OK) {
        frame_t rsp_frame = {0};
        mb_extract_frame(req_frame->protocol, adu, adu_len, &rsp_frame);

        // can be response that we already count as 'timed out, try another
        if (req_frame->tid != rsp_frame.tid) {
            return recv_response(req_frame);
        }

        if (check_req_rsp_pdu(req_frame->pdu, req_frame->pdu_len, rsp_frame.pdu, rsp_frame.pdu_len)) {
            log_adu(adu, adu_len, rsp_frame.protocol, DS_IN_OK);
            globals.stats.success++;
            return RC_SUCCESS;
        } else {
            log_adu(adu, adu_len, rsp_frame.protocol, DS_IN_FAIL);
            globals.stats.fails++;
            return RC_FAIL;
        }
    } else {
        globals.stats.fails++;
        log_traffic_str(str_valid_err(verr), DS_IN_FAIL);
    }
}

// -------------------- Base section ------------------------------------------------------

int
make_request() {
    u16 tid = 0;
    if (globals.cxt.protocol == MB_PROTOCOL_TCP) {
        tid = globals.cxt.tid++;
    }

    u8 uid = 1;
    if (globals.sequence_uid) {
        uid = globals.current_uid;
        PIND_CLAM(globals.current_uid, globals.slave_id_start, globals.slave_id_end);
    } else {
        uid = globals.slave_id_start;
    }

    frame_t frame = {
      .protocol = globals.cxt.protocol,
      .fc       = globals.cxt.fc,
      .uid      = uid,
      .tid      = tid,
    };

    if (send_request(&frame) != RC_SUCCESS) {
        return RC_FAIL;
    }

    globals.time_start = now_ms();
    recv_response(&frame);

    // update statistic output
    redraw_header(&globals);
}

void
exit_cleanup() {
    destroy_tui();
    exit(0);
}

int
main(int argc, char *argv[]) {
    if (init_client(argc, argv, &globals) != RC_SUCCESS) {
        return -1;
    }

    // stop write breacking client
    signal(SIGPIPE, SIG_IGN);

    // before client is closed by any reason we must be sure that ncurses is
    // correctly finished, otherwise it will break user console
    signal(SIGINT, exit_cleanup);
    signal(SIGTERM, exit_cleanup);
    signal(SIGSEGV, exit_cleanup);
    signal(SIGABRT, exit_cleanup);

    init_tui(&globals);
    log_line("Better Modbus Client v1.1");
    log_line("> tui started");

    open_uplink(&globals);
    globals.cxt.last_run_was_on = globals.cxt.protocol;

    // must be after init_tui because of pointer to globals
    pthread_t tinput;
    pthread_create(&tinput, NULL, input_thread, NULL);

    while (1) {
        if (!globals.running) {
            continue;
        }

        if (globals.cxt.fd == -1) {
            // try reconnect just once
            if (!relink(&globals)) {
                log_linef("! failed to reconnect: %s", strerror(errno));
                globals.running = FALSE;
                redraw_header();
                continue;
            }

            // wait small ammount of time to let new connection get ready
            msleep(10);
        }

        // make sure we run request on according connection
        if (globals.cxt.last_run_was_on == MB_PROTOCOL_TCP) {
            if (globals.cxt.protocol == MB_PROTOCOL_RTU || globals.cxt.protocol == MB_PROTOCOL_ASCII) {
                if (!relink(&globals)) {
                    globals.running = FALSE;
                }
            }
        } else {
            if (globals.cxt.protocol == MB_PROTOCOL_TCP) {
                if (!relink(&globals)) {
                    globals.running = FALSE;
                }
            }
        }

        make_request();
        globals.cxt.last_run_was_on = globals.cxt.protocol;

        msleep(globals.timeout);
    }

    getchar();

    destroy_tui();
    return 0;
}
