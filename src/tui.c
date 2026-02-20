#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "client_cxt.h"
#include "helping_hand.h"
#include "mb_base.h"
#include "tui.h"
#include "types.h"
#include "uplink.h"

pthread_mutex_t mutex;
global_t       *pglobals;

log_t logd = {0};

// ======================================================================================
// CSV LOGS
// ======================================================================================

// open csv file with
rc_t
open_csv_log(u8 ind) {
    char fname[16] = {0};
    sprintf(fname, "bmblog_%d.csv", ind);

    FILE *f = fopen(fname, "w");
    if (!f) {
        log_linef("! failed to create csv log file: %s", strerror(errno));
        return RC_FAIL;
    }

    fprintf(f, "Time,Endpoint,Direction,Payload,Error\n");
    logd.csv  = f;
    logd.find = ind;
    logd.lind = 1;

    log_linef("> csv log started %s", fname);
    return RC_SUCCESS;
}

// write to csv
void
write_csv_log(const char *time, const char *endp, const char *dir, const char *payload) {
    if (!logd.csv) {
        open_csv_log(logd.find);
    }

    fprintf(logd.csv, "%s,%s,%s,%s,%s\n", time, endp, dir, payload, logd.last_err);

    memset(logd.last_err, MAX_LINE_LEN, '\0');

    logd.lind++;
    if (logd.lind >= CSV_LINES_CAP) {
        fclose(logd.csv);

        PIND_CLAMP(logd.find, 0, CSV_IND_CAP);
        open_csv_log(logd.find);
    }
}

// ======================================================================================
// /CSV LOGS
// ======================================================================================

/* TO STRING SECTION */

// Because ncurses fields doesn't like numbers

const char *
str_baud(int baudrare) {
    // clang-format off
    switch (baudrare) {
    case 0:      return "0";
    case 50:     return "50";
    case 75:     return "75";
    case 110:    return "110";
    case 134:    return "134";
    case 150:    return "150";
    case 200:    return "200";
    case 300:    return "300";
    case 600:    return "600";
    case 1200:   return "1200";
    case 1800:   return "1800";
    case 2400:   return "2400";
    case 4800:   return "4800";
    case 9600:   return "9600";
    case 19200:  return "19200";
    case 38400:  return "38400";
    case 115200: return "115200";
    default:     return "19200";
    }
    // clang-format on

    return "19200";
}

const char *
str_dbits(int db) {
    switch (db) {
    case 5: return "5";
    case 6: return "6";
    case 7: return "7";
    case 8: return "8";

    default: return "8";
    }

    return "8";
}

const char *
str_sbits(int sp) {
    if (sp == 2) {
        return "2";
    } else {
        return "1";
    }
}

const char *
str_parity(int parity) {
    switch (parity) {
    case 'N': return "N";
    case 'O': return "O";
    case 'E': return "E";

    default: return "N";
    }

    return "N";
}

/* TO STRING SECTION */

WINDOW *wheader;
WINDOW *wlog;

static const char *
str_dirstat(dirstat_t ds) {
    // clang-format off
    switch (ds) {
    case DS_IN_OK:    return "->";
    case DS_IN_FAIL:  return "x>";
    case DS_OUT_OK:   return "<-";
    case DS_OUT_FAIL: return "<x";
    }
    // clang-format on
}

void
print_wdata() {
    int fflags = fc_flags(pglobals->cxt.fc);

    const u8 startx = 20; // based on "6 | Write Data   : " len + 1
    const u8 starty = 10;

    if (!(fflags & FCF_WRITE)) {
        mvwprintw(wheader, starty, startx, "N/A for this function");
        return;
    }

    const u8 is_bits         = fflags & FCF_BITS;
    const u8 notaion_len     = is_bits ? 1 : 4;   // reg of coil
    const u8 available_space = COLS - startx - 1; // header + right border line
    const u8 write_count     = CLAMP(pglobals->cxt.wcount, 0, WD_MAX_LEN);
    const u8 elem_per_line   = available_space / (notaion_len + 1); // + space
    const u8 nlines          = write_count / elem_per_line + 1;

    int not_finished = write_count;
    for (int y = 0; y < nlines; y++) {
        int   buff_len             = available_space * nlines + 1;
        char *buff                 = calloc(buff_len, sizeof(u8));
        int   elem_to_current_line = CLAMP(not_finished, 0, elem_per_line);
        for (int i = 0; i < elem_to_current_line; i++) {
            if (is_bits) {
                sprintf(&buff[i * (notaion_len + 1)], "%d ", pglobals->cxt.wdata[i] > 0);
            } else {
                sprintf(&buff[i * (notaion_len + 1)], "%04X ", pglobals->cxt.wdata[i]);
            }
        }
        buff[buff_len - 1] = '\0';
        mvwprintw(wheader, starty + y, startx, "%s", buff);
        free(buff);
        not_finished -= elem_per_line;
    }
}

void
redraw_header() {
    // y positions of tui header columns
    const int col_1 = 1;
    const int col_2 = 58;
    const int col_3 = 92;

    pthread_mutex_lock(&mutex);

    wclear(wheader);
    box(wheader, 0, 0);

    s8 endp[32];
    str_curr_endpoint(endp, pglobals);

    mvwprintw(wheader, 1, col_1, "1 | Protocol: %s", str_protocol(pglobals->cxt.protocol));
    mvwprintw(wheader, 2, col_1, "2 | Endpoint: %s", endp);
    if (pglobals->slave_id_start == pglobals->slave_id_end) {
        mvwprintw(wheader, 3, col_1, "3 | Unit IDs: %d", pglobals->slave_id_start);
    } else {
        mvwprintw(wheader, 3, col_1, "3 | Unit IDs: %d-%d", pglobals->slave_id_start, pglobals->slave_id_end);
    }
    mvwprintw(wheader, 4, col_1, "4 | Function: %s", str_fc(pglobals->cxt.fc));

    mvwprintw(wheader, 6, col_1, "5 | Read address : 0x%04X", pglobals->cxt.raddress);
    mvwprintw(wheader, 7, col_1, "  | Read count   : %d", pglobals->cxt.rcount);
    mvwprintw(wheader, 8, col_1, "  | Write address: 0x%04X", pglobals->cxt.waddress);
    mvwprintw(wheader, 9, col_1, "  | Write count  : %d", pglobals->cxt.wcount);
    mvwprintw(wheader, 10, col_1, "6 | Write Data   : ");
    print_wdata(pglobals);

    mvwprintw(wheader, 1, col_2, "F5 | Running: %s", pglobals->running ? "On" : "Off");
    mvwprintw(wheader, 2, col_2, "F6 | Random:  %s", pglobals->random ? "On" : "Off");

    mvwprintw(wheader, 4, col_2, "F7 | Fire request sequence:");
    mvwprintw(wheader, 5, col_2, "     %06d / %06d", pglobals->rfire_current, pglobals->rfire_count);

    mvwprintw(wheader, 7, col_2, "F9 | Response timeout: %d ms", pglobals->response_timeout);
    mvwprintw(wheader, 8, col_2, "   | Send timeout    : %d ms", pglobals->timeout);

    int reqs      = pglobals->stats.requests;
    int successes = pglobals->stats.success;
    int fails     = pglobals->stats.fails;
    int timeouts  = pglobals->stats.timeouts;

    mvwprintw(wheader, 1, col_3, "Requests:  %05d", reqs);

    // won't affect real statistic output, but make crude 0 div safeguard
    if (!reqs) {
        reqs = 1;
    }

    mvwprintw(wheader, 2, col_3, "Successed: %05d  %.2f%%", successes, (float)successes / reqs * 100);
    mvwprintw(wheader, 3, col_3, "Failed:    %05d  %.2f%%", fails, (float)fails / reqs * 100);
    mvwprintw(wheader, 4, col_3, "Timedout:  %05d  %.2f%%", timeouts, (float)timeouts / reqs * 100);

    mvwprintw(wheader, 6, col_3, "F8 | Reset statistics");

    wrefresh(wheader);
    pthread_mutex_unlock(&mutex);
}

static void
redraw_log() {
    pthread_mutex_lock(&mutex);
    wclear(wlog);

    for (int i = 0; i < logd.linec; i++) {
        mvwprintw(wlog, i, 0, "%s", logd.lines[i]);
    }

    wrefresh(wlog);
    pthread_mutex_unlock(&mutex);
}

void
init_tui(global_t *globals) {
    pthread_mutex_init(&mutex, NULL);
    pglobals = globals;

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    refresh();

    // init header
    wheader = NEW_WIN(HEADER_BOTTOM, COLS, 0, 0);
    redraw_header();

    // init logd
    int rows      = LINES - HEADER_BOTTOM;
    logd.max_rows = rows;
    logd.linel    = COLS;
    wlog          = NEW_WIN(rows, COLS, HEADER_BOTTOM, 0);
    box(wlog, 0, 0);
    redraw_log();
}

void
destroy_tui() {
    pthread_mutex_destroy(&mutex);

    delwin(wheader);
    delwin(wlog);

    endwin();

    fclose(logd.csv);
}

static void
format_payload(u8 out[MAX_LINE_LEN], u8 adu[MB_MAX_ADU_LEN], int adu_len, mb_protocol_t protocol) {
    // so we can not to worry about null termination
    memset(out, 0, 1024);

    for (int i = 0; i < adu_len; i++) {
        out[i * 3]     = nibble_to_hex((adu[i] >> 4) & 0x0F);
        out[i * 3 + 1] = nibble_to_hex((adu[i] >> 0) & 0x0F);
        out[i * 3 + 2] = ' ';
    }
}

void
log_traffic_str(const char *str, dirstat_t ds) {
    u8   buff[MAX_LINE_LEN] = {0};
    char endp[32]           = {0};
    str_curr_endpoint(endp, pglobals);

    struct timeval tv;
    gettimeofday(&tv, NULL); // Get current time with microseconds
    u64 ms = tv.tv_usec / 1000;

    u8 time[8] = {0};
    sprintf(time, ".%03lu", ms);

    // log csv
    if (pglobals->use_csv_log) {
        strcpy(logd.last_err, str);
        write_csv_log(time, endp, str_dirstat(ds), "<NONE>");
    }

    log_linef("%s %s %s <!%s>", time, endp, str_dirstat(ds), str);
}

void
log_adu(u8 adu[MB_MAX_ADU_LEN], int adu_len, mb_protocol_t protocol, dirstat_t ds) {
    u8   buff[MAX_LINE_LEN] = {0};
    char endp[32]           = {0};
    str_curr_endpoint(endp, pglobals);

    struct timeval tv;
    gettimeofday(&tv, NULL); // Get current time with microseconds
    u64 ms = tv.tv_usec / 1000;

    u8 time[8] = {0};
    sprintf(time, ".%03lu", ms);

    u8 left_side[MAX_LINE_LEN] = {0};
    sprintf(left_side, ".%03luZ %s %s", ms, endp, str_dirstat(ds));

    u8 payload[1024];
    format_payload(payload, adu, adu_len, protocol);
    int payload_len = strlen(payload);

    // log csv
    if (pglobals->use_csv_log) {
        write_csv_log(time, endp, str_dirstat(ds), payload);
    }

    snprintf(buff, MAX_LINE_LEN - 1, "%s %s", left_side, payload);
    buff[MAX_LINE_LEN - 1] = '\0';

    int lines = strlen(buff) / COLS + 1;
    for (int i = 0; i < lines; i++) {
        u8 line_buff[MAX_LINE_LEN]  = {0};
        // TODO: clamp between COLS and MAX LINE LEN
        line_buff[MAX_LINE_LEN - 1] = '\0';
        memcpy(line_buff, &buff[i * COLS], MAX_LINE_LEN - (i * COLS));
        log_line(line_buff);
    }
}

void
log_line(const char *line) {
    strncpy(logd.lines[logd.linec], line, MAX_LINE_LEN - 1);
    logd.lines[logd.linec][MAX_LINE_LEN - 1] = '\0';

    // shift all lines one up if too much, drop top one
    if (logd.linec == logd.max_rows) {
        for (int i = 0; i < logd.max_rows; i++) {
            strncpy(logd.lines[i], logd.lines[i + 1], MAX_LINE_LEN);
        }
        logd.linec--;
    }

    logd.linec++;

    redraw_log();
}

void
log_linef(const char *format, ...) {
    u8 buff[MAX_LINE_LEN] = {0};

    va_list va;
    va_start(va, format);
    vsnprintf(buff, MAX_LINE_LEN, format, va);
    va_end(va);

    log_line(buff);
}

void
log_req_errf(const char *format, ...) {
    va_list va;
    va_start(va, format);
    vsnprintf(logd.last_err, MAX_LINE_LEN, format, va);
    va_end(va);

    log_line(logd.last_err);
}

// =============================================================================
// TUI
// =============================================================================

void
close_dialog(WINDOW *win, FORM *form, FIELD **fields, int nfields) {
    unpost_form(form);
    free_form(form);
    for (int i = 0; i < nfields; i++) {
        free_field(fields[i]);
    }
    delwin(win);
    redraw_log();
}

// ---------------------------- Endpoint ---------------------------------------

char *field_enum_baud[] = {
  "0",      //
  "50",     //
  "75",     //
  "110",    //
  "134",    //
  "150",    //
  "200",    //
  "300",    //
  "600",    //
  "1200",   //
  "1800",   //
  "2400",   //
  "4800",   //
  "9600",   //
  "19200",  //
  "38400",  //
  "57600",  //
  "115200", //
  NULL,
};

char *field_enum_dbits[] = {
  "5",
  "6",
  "7",
  "8",
  NULL,
};

char *field_enum_sbits[] = {
  "1",
  "2",
  NULL,
};

char *field_enum_parity[] = {
  "N",
  "O",
  "E",
  NULL,
};

void
tui_endpoint_draw(WINDOW *win, FORM *form) {
    box(win, 0, 0);
    if (pglobals->cxt.protocol == MB_PROTOCOL_TCP) {
        mvwprintw(win, 0, 1, "TCP Endpoint");
        mvwprintw(win, 1, 1, "Host:   ");
        mvwprintw(win, 2, 1, "Port:   ");

        mvwprintw(win, 8, 1, "F1 - Submit");
        mvwprintw(win, 9, 1, "F2 - Cancel");
    } else {
        mvwprintw(win, 0, 1, "Serial Endpoint");
        mvwprintw(win, 1, 1, "Device:    ");
        mvwprintw(win, 2, 1, "Baudrate:  ");
        mvwprintw(win, 3, 1, "Data bits: ");
        mvwprintw(win, 4, 1, "Stop bits: ");
        mvwprintw(win, 5, 1, "Parity:    ");

        mvwprintw(win, 8, 1, "F1 - Submit");
        mvwprintw(win, 9, 1, "F2 - Cancel");
    }
    wrefresh(win);
    pos_form_cursor(form);
}

void
tui_endpoint() {
    const u8 input_field_lane = 16;
    const u8 legend_len       = 12;

    const u8 win_width  = input_field_lane + legend_len + 2;
    const u8 win_height = 11;

    //   nfields  h           w          y              x
    TUIDW_HEAD(5, win_height, win_width, LINES / 2 - 5, COLS / 2 - 16)

    if (pglobals->cxt.protocol == MB_PROTOCOL_TCP) {
        // host field    i  h  w                 y  x  val
        TUIDW_FIELD_TEXT(0, 1, input_field_lane, 0, 0, pglobals->tcp_endp.host)
        // port field   i  h  w                 y  x  z  mn mx     val
        TUIDW_FIELD_INT(1, 1, input_field_lane, 1, 0, 0, 0, 65535, pglobals->tcp_endp.tcp_port)

        field[2] = NULL;
    } else {
        // device field  i  h  w                 y  x  val
        TUIDW_FIELD_TEXT(0, 1, input_field_lane, 0, 0, pglobals->sconf.device)
        // baud field    i  h  w                 y  x  enum             val
        TUIDW_FIELD_ENUM(1, 1, input_field_lane, 1, 0, field_enum_baud, str_baud(pglobals->sconf.baud))
        // data bits field
        TUIDW_FIELD_ENUM(2, 1, input_field_lane, 2, 0, field_enum_dbits, str_dbits(pglobals->sconf.data_bits))
        // stop bits field
        TUIDW_FIELD_ENUM(3, 1, input_field_lane, 3, 0, field_enum_sbits, str_sbits(pglobals->sconf.stop_bits))
        // parity field
        TUIDW_FIELD_ENUM(4, 1, input_field_lane, 4, 0, field_enum_parity, str_parity(pglobals->sconf.parity))
    }

    //            h  w                 y  x
    TUIDW_SUBFORM(5, input_field_lane, 1, legend_len)

    tui_endpoint_draw(win, form);

    while (1) {
        int ch = wgetch(win);

        // clang-format off
        switch (ch) {
        TUIDW_STDDRIVER
        case KEY_LEFT     : form_driver(form, REQ_PREV_CHOICE); break;
        case KEY_RIGHT    : form_driver(form, REQ_NEXT_CHOICE); break;
        // clang-format on

        // try submit
        case KEY_F(1):
            // make sure all buffer available and valid
            if (form_driver(form, REQ_VALIDATION) != E_OK) {
                continue;
            }

            if (pglobals->cxt.protocol == MB_PROTOCOL_TCP) {
                tcp_endp tcp = {0};
                if (tcp_ednp_from_str(&tcp, field_buffer(field[0], 0), field_buffer(field[1], 0))) {
                    memcpy(&pglobals->tcp_endp, &tcp, sizeof(tcp));

                    relink(pglobals);
                    close_dialog(win, form, field, nfields);
                    return;
                } else {
                    // tcp config is bad, just continue with this window
                    // Note: have to redraw after writing in log
                    tui_endpoint_draw(win, form);
                    break;
                }
            } else {
                serial_cfg new_conf = {0};

                char *b0 = field_buffer(field[0], 0);
                char *b1 = field_buffer(field[1], 0);
                char *b2 = field_buffer(field[2], 0);
                char *b3 = field_buffer(field[3], 0);
                char *b4 = field_buffer(field[4], 0);
                if (sconf_from_str(&new_conf, b0, b1, b2, b3, b4)) {
                    memcpy(&pglobals->sconf, &new_conf, sizeof(new_conf));

                    relink(pglobals);
                    close_dialog(win, form, field, nfields);
                    return;
                }
                // serial config is bad, just continue with this window
                // Note: have to redraw after writing in log
                tui_endpoint_draw(win, form);
                break;
            }

        default: form_driver(form, ch); break;
        }
    }
}

// ---------------------------- UID --------------------------------------------

char *use_uid_seq[] = {
  "single",
  "sequence",
  NULL,
};

// clang-format off
void static inline
tui_uid_draw(WINDOW *win, FORM *form, u8 sequence) {
    // clang-format on
    box(win, 0, 0);
    mvwprintw(win, 0, 1, "Unit ID");

    if (sequence) {
        mvwprintw(win, 1, 1, "Sequence:  ");
        mvwprintw(win, 2, 1, "UID Start: ");
        mvwprintw(win, 3, 1, "UID End:   ");
        mvwprintw(win, 5, 1, "F1 - Submit");
        mvwprintw(win, 6, 1, "F2 - Cancel");
    } else {
        mvwprintw(win, 1, 1, "Sequence:   ");
        mvwprintw(win, 2, 1, "Single UID: ");
        mvwprintw(win, 3, 1, "            ");

        mvwprintw(win, 5, 1, "F1 - Submit");
        mvwprintw(win, 6, 1, "F2 - Cancel");
    }

    wrefresh(win);
    pos_form_cursor(form);
}

void
tui_uid() {
    u8 sequence = pglobals->sequence_uid; // 0 - single uid, 1 - uid start, uid end

    //   nfields  h  w   y              x
    TUIDW_HEAD(3, 8, 32, LINES / 2 - 5, COLS / 2 - 16)

    // select        i  h  w   y  x
    TUIDW_FIELD_ENUM(0, 1, 14, 0, 0, use_uid_seq, use_uid_seq[sequence])

    // start uid    i  h  w   y  x  z  mn mx   val
    TUIDW_FIELD_INT(1, 1, 14, 1, 0, 0, 0, 255, pglobals->slave_id_start)
    // end uid
    TUIDW_FIELD_INT(2, 1, 14, 2, 0, 0, 0, 255, pglobals->slave_id_end)

    // disable uid end field if sequence is set to single
    if (!sequence) {
        field_opts_off(field[2], O_VISIBLE | O_EDIT | O_ACTIVE);
    }

    //            h  w   y  x
    TUIDW_SUBFORM(5, 16, 1, 13)
    tui_uid_draw(win, form, sequence);

    while (1) {
        int ch = wgetch(win);

        // clang-format off
        switch (ch) {
        TUIDW_STDDRIVER
            // clang-format on

        case KEY_LEFT: {
            form_driver(form, REQ_PREV_CHOICE);

            FIELD *cur = current_field(form);
            // 0 - index of sequence/single select field
            if (field_index(cur) == 0) {
                sequence = !sequence;

                if (!sequence) {
                    field_opts_off(field[2], O_VISIBLE | O_EDIT | O_ACTIVE);
                } else {
                    field_opts_on(field[2], O_VISIBLE | O_EDIT | O_ACTIVE);
                }
                tui_uid_draw(win, form, sequence);
            }

            break;
        }

        case KEY_RIGHT: {
            form_driver(form, REQ_NEXT_CHOICE);

            FIELD *cur = current_field(form);
            // 0 - index of sequence/single select field
            if (field_index(cur) == 0) {
                sequence = !sequence;

                if (!sequence) {
                    field_opts_off(field[2], O_VISIBLE | O_EDIT | O_ACTIVE);
                } else {
                    field_opts_on(field[2], O_VISIBLE | O_EDIT | O_ACTIVE);
                }
                tui_uid_draw(win, form, sequence);
            }

            break;
        }

        // sumbit
        case KEY_F(1): {
            // make sure all buffer available and valid
            if (form_driver(form, REQ_VALIDATION) != E_OK) {
                break;
            }

            int   start = 0;
            int   end   = 0;
            char *buff1 = field_buffer(field[1], 0);
            char *buff2 = field_buffer(field[2], 0);
            if (uid_from_str(&start, &end, buff1, buff2, sequence)) {
                pglobals->slave_id_start = start;
                pglobals->slave_id_end   = end;
                pglobals->sequence_uid   = sequence;
                pglobals->current_uid    = start;
                close_dialog(win, form, field, nfields);
                return;
            } else {
                // Note: have to redraw after writing in log
                tui_uid_draw(win, form, sequence);

                break;
            }
            break;
        }

        default: form_driver(form, ch); break;
        }
    }
}

// ---------------------------- Function codes ---------------------------------

int
ind_to_fc(int ind) {
    switch (ind) {
    case 0: return MB_FC_READ_COILS;
    case 1: return MB_FC_READ_DISCRETE_INPUTS;
    case 2: return MB_FC_READ_HOLDING_REGISTERS;
    case 3: return MB_FC_READ_INPUT_REGISTERS;
    case 4: return MB_FC_WRITE_SINGLE_COIL;
    case 5: return MB_FC_WRITE_SINGLE_REGISTER;
    case 6: return MB_FC_WRITE_MULTIPLE_COILS;
    case 7: return MB_FC_WRITE_MULTIPLE_REGISTERS;
    case 8: return MB_FC_WRITE_AND_READ_REGISTERS;
    }

    // just in case
    return MB_FC_READ_COILS;
}

void
tui_fc() {
    //                    h   w   y              x
    WINDOW *wfc = NEW_WIN(13, 48, LINES / 2 - 7, COLS / 2 - 24);
    // YEAH! MAGIC AROUND!
    keypad(wfc, TRUE);
    box(wfc, 0, 0);

    mvwprintw(wfc, 0, 1, "Function Code");
    mvwprintw(wfc, 1, 1, "1 | 01 - Read Coils (0x01)");
    mvwprintw(wfc, 2, 1, "2 | 02 - Read Discrete Inputs  (0x02)");
    mvwprintw(wfc, 3, 1, "3 | 03 - Read Holding Registers (0x03)");
    mvwprintw(wfc, 4, 1, "4 | 04 - Read Input Registers (0x04)");
    mvwprintw(wfc, 5, 1, "5 | 05 - Write Single Coil (0x05)");
    mvwprintw(wfc, 6, 1, "6 | 06 - Write Single Register (0x06)");
    mvwprintw(wfc, 7, 1, "7 | 15 - Write Multiple Coils (0x0F)");
    mvwprintw(wfc, 8, 1, "8 | 16 - Write Multiple registers (0x10)");
    mvwprintw(wfc, 9, 1, "9 | 23 - Read/Write Multiple registers (0x17)");

    mvwprintw(wfc, 11, 1, "F2 - Cancel");

    wrefresh(wfc);

    while (1) {
        int ch = wgetch(wfc);

        if (ch == KEY_F(2)) {
            // i changed my mind, get me out
            break;
        } else if (ch >= KEY_1 && ch <= KEY_9) {
            // ladies and gentelmens, we got him
            pglobals->cxt.fc = ind_to_fc(ch - KEY_1);
            redraw_header();
            break;
        }
    }

    delwin(wfc);
    redraw_log();
}

// ---------------------------- Qantities and addresses ------------------------

void
tui_qty_addr() {
    //   nfields  h  w   y              x
    TUIDW_HEAD(4, 9, 26, LINES / 2 - 5, COLS / 2 - 16)

    // read address i  h  w  y  x  z  mn mx      val
    TUIDW_FIELD_INT(0, 1, 6, 0, 0, 0, 0, 0xFFFF, pglobals->cxt.raddress)
    // read count
    TUIDW_FIELD_INT(1, 1, 6, 1, 0, 0, 0, 2000, pglobals->cxt.rcount)
    // write address
    TUIDW_FIELD_INT(2, 1, 6, 2, 0, 0, 0, 0xFFFF, pglobals->cxt.waddress)
    // write count field
    TUIDW_FIELD_INT(3, 1, 6, 3, 0, 0, 0, 0x07B0, pglobals->cxt.rcount)

    //            h  w  y  x
    TUIDW_SUBFORM(5, 6, 1, 18)

    box(win, 0, 0);
    mvwprintw(win, 0, 1, "Qanity and Addresses");
    mvwprintw(win, 1, 1, "Read  address: ");
    mvwprintw(win, 2, 1, "Read  count:   ");
    mvwprintw(win, 3, 1, "Write address: ");
    mvwprintw(win, 4, 1, "Write count:   ");
    mvwprintw(win, 6, 1, "F1 - Submit");
    mvwprintw(win, 7, 1, "F2 - Cancel");
    wrefresh(win);
    pos_form_cursor(form);

    while (1) {
        int ch = wgetch(win);

        // clang-format off
        switch (ch) {
        TUIDW_STDDRIVER
        // clang-format on

        // sumbit
        case KEY_F(1): {
            // make sure all buffer available and valid
            if (form_driver(form, REQ_VALIDATION) != E_OK) {
                continue;
            }

            char *fb1 = field_buffer(field[0], 0);
            char *fb2 = field_buffer(field[1], 0);
            char *fb3 = field_buffer(field[2], 0);
            char *fb4 = field_buffer(field[3], 0);

            int raddr  = 0;
            int rcount = 0;
            int waddr  = 0;
            int wcount = 0;
            if (qnt_addr_from_str(&raddr, &rcount, &waddr, &wcount, fb1, fb2, fb3, fb4)) {
                pglobals->cxt.raddress = raddr;
                pglobals->cxt.rcount   = rcount;
                pglobals->cxt.waddress = waddr;
                pglobals->cxt.wcount   = wcount;
                close_dialog(win, form, field, nfields);
                return;
            } else {
                // Note: have to redraw after writing in log
                box(win, 0, 0);
                mvwprintw(win, 0, 1, "Qanity and Addresses");
                mvwprintw(win, 1, 1, "Read  address: ");
                mvwprintw(win, 2, 1, "Read  count:   ");
                mvwprintw(win, 3, 1, "Write address: ");
                mvwprintw(win, 4, 1, "Write count:   ");
                mvwprintw(win, 6, 1, "F1 - Submit");
                mvwprintw(win, 7, 1, "F2 - Cancel");
                wrefresh(win);
                pos_form_cursor(form);

                break;
            }
            break;
        }

        default: form_driver(form, ch); break;
        }
    }
}

// ---------------------------- Timeouts ---------------------------------------

void
tui_timeouts(global_t *pglobals) {
    //   nfields  h  w   y              x
    TUIDW_HEAD(2, 7, 27, LINES / 2 - 5, COLS / 2 - 16)

    // respone t/o  i  h  w  y  x  z  mn mx     val
    TUIDW_FIELD_INT(0, 1, 6, 0, 0, 0, 0, 10000, pglobals->response_timeout)
    // send t/o
    TUIDW_FIELD_INT(1, 1, 6, 1, 0, 0, 0, 10000, pglobals->timeout)

    //            h  w  y  x
    TUIDW_SUBFORM(5, 6, 1, 19)

    box(win, 0, 0);
    mvwprintw(win, 0, 1, "Timeouts");
    mvwprintw(win, 1, 1, "Response timeout: ");
    mvwprintw(win, 2, 1, "Send timeout:     ");

    mvwprintw(win, 4, 1, "F1 - Submit");
    mvwprintw(win, 5, 1, "F2 - Cancel");
    wrefresh(win);
    pos_form_cursor(form);

    while (1) {
        int ch = wgetch(win);

        // clang-format off
        switch (ch) {
        TUIDW_STDDRIVER
        // clang-format on

        // sumbit
        case KEY_F(1): {
            // make sure all buffer available and valid
            if (form_driver(form, REQ_VALIDATION) != E_OK) {
                break;
            }

            int rtimeout = 0;
            int stimeout = 0;
            if (timeouts_from_str(&rtimeout, &stimeout, field_buffer(field[0], 0), field_buffer(field[1], 0))) {
                pglobals->response_timeout = rtimeout;
                pglobals->timeout          = stimeout;
                close_dialog(win, form, field, nfields);
                return;
            }
            break;
        }

        default: form_driver(form, ch); break;
        }
    }
}

// ---------------------------- Write data--------------------------------------

void
tui_wdata() {
    const u8 input_line_len = 40;
    const u8 reg_str_len    = 4; // 4 bytes for register presentation; choosed as bigger of coils/regs

    // 4 chars for one reg + space for all of them exept last one on the line
    const u8 regs_on_line = input_line_len / (reg_str_len + 1);
    const u8 nlines       = WD_MAX_LEN / regs_on_line + !!(WD_MAX_LEN % regs_on_line);

    // len of "Data: " + len of input line + 2 border lines
    const u8 win_width  = 8 + input_line_len + 2;
    // rows for nlines, division line, submit, cancel AND two border lines
    const u8 win_height = nlines + 3 + 2;

    const u8 current_reg_count = CLAMP(pglobals->cxt.wcount, 0, WD_MAX_LEN);

    //   nfields  h           w          y                           x
    TUIDW_HEAD(1, win_height, win_width, LINES / 2 - win_height / 2, COLS / 2 - win_width / 2)

    // write data to buffer
    int   buff_len = input_line_len * nlines + 1;
    char *buffl    = calloc(buff_len, sizeof(u8));
    for (u8 i = 0; i < current_reg_count; i++) {
        sprintf(&buffl[i * 5], "%04X ", pglobals->cxt.wdata[i]);
    }
    buffl[buff_len - 1] = '\0';

    // write values  i  h       w               y  x  val
    TUIDW_FIELD_TEXT(0, nlines, input_line_len, 0, 0, buffl);

    free(buffl);

    //            h       w               y  x
    TUIDW_SUBFORM(nlines, input_line_len, 1, 8)

    box(win, 0, 0);
    mvwprintw(win, 0, 1, "Write data");
    mvwprintw(win, 1, 1, "Data: ");

    mvwprintw(win, nlines + 2, 1, "F1 - Submit");
    mvwprintw(win, nlines + 3, 1, "F2 - Cancel");
    wrefresh(win);
    pos_form_cursor(form);

    while (1) {
        int ch = wgetch(win);

        switch (ch) {
        case KEY_BACKSPACE: form_driver(form, REQ_DEL_PREV); break;

        // cancel
        case KEY_F(2): close_dialog(win, form, field, nfields); return;

        // sumbit
        case KEY_F(1): {
            if (form_driver(form, REQ_VALIDATION) == E_OK) {
                wdata_from_str(pglobals, field_buffer(field[0], 0));
                close_dialog(win, form, field, nfields);
                return;
            }
            continue;
        }

        default: form_driver(form, ch); break;
        }
    }
}

// ---------------------------- Fire sequence ----------------------------------

void
tui_fsequence() {
    //   nfields  h  w   y              x
    TUIDW_HEAD(1, 7, 42, LINES / 2 - 5, COLS / 2 - 18)

    // seq count    i  h  w  y  x  zp mn max     val
    TUIDW_FIELD_INT(0, 1, 8, 0, 0, 0, 0, 999999, pglobals->rfire_count)

    //            h  w  y  x
    TUIDW_SUBFORM(1, 10, 1, 26)

    box(win, 0, 0);
    mvwprintw(win, 0, 1, "Requests count");
    mvwprintw(win, 1, 1, "Request count (⩽999999): ");

    mvwprintw(win, 4, 1, "F1 - Submit");
    mvwprintw(win, 5, 1, "F2 - Cancel");
    wrefresh(win);
    pos_form_cursor(form);

    while (1) {
        int ch = wgetch(win);

        // clang-format off
        switch (ch) {
        TUIDW_STDDRIVER
        // clang-format on

        // sumbit
        case KEY_F(1): {
            // make sure all buffer available and valid
            if (form_driver(form, REQ_VALIDATION) != E_OK) {
                break;
            }

            int count = 0;
            if (int_from_str(&count, field_buffer(field[0], 0))) {
                if (count <= 0) {
                    continue;
                }

                pglobals->rfire_count   = (u32)count;
                pglobals->rfire_current = 0;

                pglobals->running = TRUE;

                close_dialog(win, form, field, nfields);
                return;
            }

            break;
        }

        default: form_driver(form, ch); break;
        }
    }
}

// ---------------------------- Parent function --------------------------------

void *
input_thread() {
    while (1) {
        int key = getch();
        if (key == ERR) {
            continue;
        }

        // handle terminal resize, redraw everething
        if (key == KEY_RESIZE) {
            resize_term(0, 0);
            clear();

            // print warning if terminal is too small
            if (COLS < 116 || LINES < 22) {
                log_linef("! it's not recommended to run BMB CLient in terminal less than 116x22 in size");
            }

            clearok(curscr, TRUE);
            refresh();

            // resize and redraw header
            wresize(wheader, HEADER_BOTTOM, COLS);
            redraw_header(pglobals);

            // restructure log, drop older lines if they doesn't fit in new line count
            logd.max_rows = LINES - HEADER_BOTTOM;
            if (logd.linec >= logd.max_rows) {
                for (int i = 0; i < logd.max_rows; i++) {
                    strncpy(logd.lines[i], logd.lines[i + 1], MAX_LINE_LEN);
                }
                logd.linec--;
            }

            // resize and redraw header
            wresize(wlog, LINES - HEADER_BOTTOM, COLS);
            redraw_log(pglobals);

            continue;
        }

        // TUI shouldn't change anything while client actualy running requests
        if (pglobals->running && key != KEY_F(5)) {
            continue;
        }

        switch (key) {
        case KEY_1: pglobals->cxt.protocol = (pglobals->cxt.protocol + 1) % MB_PROTOCOL_MAX; break;
        case KEY_2: tui_endpoint(); break;
        case KEY_3: tui_uid(); break;
        case KEY_4: tui_fc(); break;
        case KEY_5: tui_qty_addr(); break;
        case KEY_6: tui_wdata(); break;

        case KEY_F(5):
            // if have running request sequence right now, kill it
            if (pglobals->running & pglobals->rfire_count > 0) {
                pglobals->rfire_count   = 0;
                pglobals->rfire_current = 0;
                pglobals->running       = FALSE;
                break;
            }
            pglobals->running = ~pglobals->running;
            break;

        case KEY_F(6): pglobals->random = ~pglobals->random; break;
        case KEY_F(7): tui_fsequence(); break;
        case KEY_F(8):
            pglobals->stats.requests = 0;
            pglobals->stats.success  = 0;
            pglobals->stats.timeouts = 0;
            pglobals->stats.fails    = 0;
            redraw_header(pglobals);
            break;

        case KEY_F(9): tui_timeouts(pglobals); break;
        }

        redraw_header(pglobals);
    }
}
