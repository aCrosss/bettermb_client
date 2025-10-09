#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client_cxt.h"
#include "helping_hand.h"
#include "mb_base.h"
#include "tui.h"
#include "uplink.h"

global_t *pglobals;

/* TO STRING SECTION */

// Because ncurses fields doesn't like numbers

const char *
str_baud(int baudrare) {
    switch (baudrare) {
    case 0     : return "0";
    case 50    : return "50";
    case 75    : return "75";
    case 110   : return "110";
    case 134   : return "134";
    case 150   : return "150";
    case 200   : return "200";
    case 300   : return "300";
    case 600   : return "600";
    case 1200  : return "1200";
    case 1800  : return "1800";
    case 2400  : return "2400";
    case 4800  : return "4800";
    case 9600  : return "9600";
    case 19200 : return "19200";
    case 38400 : return "38400";
    case 115200: return "115200";
    default    : return "19200";
    }

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

log_t logd = {0};

static const char *
str_dirstat(dirstat_t ds) {
    switch (ds) {
    case DS_IN_OK   : return "->";
    case DS_IN_FAIL : return "x>";
    case DS_OUT_OK  : return "<-";
    case DS_OUT_FAIL: return "<x";
    }
}

void
print_wdata(global_t *global) {
    int fflags = fc_flags(global->cxt.fc);
    if (!(fflags & FCF_WRITE)) {
        return;
    }

    const u8 startx          = 20; // based on "6 | Write Data   : " len + 1
    const u8 starty          = 10;
    const u8 is_bits         = fflags & FCF_BITS;
    const u8 notaion_len     = is_bits ? 1 : 4;   // reg of coil
    const u8 available_space = COLS - startx - 1; // header + right border line
    const u8 write_count     = CLAMP(global->cxt.wcount, 0, WD_MAX_LEN);
    const u8 elem_per_line   = available_space / (notaion_len + 1); // + space
    const u8 nlines          = write_count / elem_per_line + 1;

    int not_finished = write_count;
    for (int y = 0; y < nlines; y++) {
        int   buff_len             = available_space * nlines + 1;
        char *buff                 = calloc(buff_len, sizeof(u8));
        int   elem_to_current_line = CLAMP(not_finished, 0, elem_per_line);
        for (int i = 0; i < elem_to_current_line; i++) {
            if (is_bits) {
                sprintf(&buff[i * (notaion_len + 1)], "%d ", global->cxt.wdata[i] > 0);
            } else {
                sprintf(&buff[i * (notaion_len + 1)], "%04X ", global->cxt.wdata[i]);
            }
        }
        buff[buff_len - 1] = '\0';
        mvwprintw(wheader, starty + y, startx, "%s", buff);
        free(buff);
        not_finished -= elem_per_line;
    }
}

void
redraw_header(global_t *global) {
    wclear(wheader);
    box(wheader, 0, 0);

    s8 endp[32];
    str_curr_endpoint(endp, global);

    mvwprintw(wheader, 1, 1, "1 | Protocol: %s", str_protocol(global->cxt.protocol));
    mvwprintw(wheader, 2, 1, "2 | Endpoint: %s", endp);
    if (global->slave_id_start == global->slave_id_end) {
        mvwprintw(wheader, 3, 1, "3 | Unit IDs: %d", global->slave_id_start);
    } else {
        mvwprintw(wheader, 3, 1, "3 | Unit IDs: %d-%d", global->slave_id_start, global->slave_id_end);
    }
    mvwprintw(wheader, 4, 1, "4 | Function: %s", str_fc(global->cxt.fc));

    mvwprintw(wheader, 6, 1, "5 | Read address : 0x%04X", global->cxt.raddress);
    mvwprintw(wheader, 7, 1, "  | Read count   : %d", global->cxt.rcount);
    mvwprintw(wheader, 8, 1, "  | Write address: 0x%04X", global->cxt.waddress);
    mvwprintw(wheader, 9, 1, "  | Write count  : %d", global->cxt.wcount);
    mvwprintw(wheader, 10, 1, "6 | Write Data   : ");
    print_wdata(global);

    mvwprintw(wheader, 1, 64, "F5 | Running: %s", global->running ? "On" : "Off");
    mvwprintw(wheader, 2, 64, "F6 | Random:  %s", global->random ? "On" : "Off");
    mvwprintw(wheader, 4, 64, "F8 | Reset statistics");

    mvwprintw(wheader, 6, 64, "F9 | Response timeout: %d ms", global->response_timeout);
    mvwprintw(wheader, 7, 64, "   | Send timeout    : %d ms", global->timeout);

    int reqs      = global->stats.requests;
    int successes = global->stats.success;
    int fails     = global->stats.fails;
    int timeouts  = global->stats.timeouts;
    mvwprintw(wheader, 1, 96, "Requests:  %04d", reqs);

    // won't affect real statistic output, but make crude 0 div safeguard
    if (!reqs) {
        reqs = 1;
    }

    mvwprintw(wheader, 2, 96, "Successed: %04d  %.2f%%", successes, (float)successes / reqs * 100);
    mvwprintw(wheader, 3, 96, "Failed:    %04d  %.2f%%", fails, (float)fails / reqs * 100);
    mvwprintw(wheader, 4, 96, "Timedout:  %04d  %.2f%%", timeouts, (float)timeouts / reqs * 100);

    wrefresh(wheader);
}

static void
redraw_log() {
    wclear(wlog);

    for (int i = 0; i < logd.linec; i++) {
        mvwprintw(wlog, i, 0, "%s", logd.lines[i]);
    }

    wrefresh(wlog);
}

void
init_screen(global_t *global) {
    pglobals = global;

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    refresh();

    // init header
    u8 header_bottom = 16;
    wheader          = newwin(header_bottom, COLS, 0, 0);
    redraw_header(global);

    // init logd
    int rows      = LINES - header_bottom;
    logd.max_rows = rows;
    logd.linel    = COLS;
    wlog          = newwin(rows, COLS, header_bottom, 0);
    box(wlog, 0, 0);
    redraw_log();
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
log_traffic_str(global_t *g, const char *str, dirstat_t ds) {
    u8   buff[MAX_LINE_LEN] = {0};
    char endp[32]           = {0};
    str_curr_endpoint(endp, g);

    log_linef("%s %s <!%s>", endp, str_dirstat(ds), str);
}

void
log_adu(global_t *g, u8 adu[MB_MAX_ADU_LEN], int adu_len, mb_protocol_t protocol, dirstat_t ds) {
    u8   buff[MAX_LINE_LEN] = {0};
    char endp[32]           = {0};
    str_curr_endpoint(endp, g);

    u8 left_side[MAX_LINE_LEN] = {0};
    sprintf(left_side, "%s %s", endp, str_dirstat(ds));

    u8 payload[1024];
    format_payload(payload, adu, adu_len, protocol);
    int payload_len = strlen(payload);

    snprintf(buff, MAX_LINE_LEN - 1, "%s %s", left_side, payload);
    buff[MAX_LINE_LEN - 1] = '\0';

    int lines = strlen(buff) / COLS + 1;
    for (int i = 0; i < lines; i++) {
        u8 line_buff[MAX_LINE_LEN]  = {0};
        // TODO: clamp between COLS and MAX LINE LEN
        line_buff[MAX_LINE_LEN - 1] = '\0';
        memcpy(line_buff, &buff[i * COLS], MAX_LINE_LEN - (i * COLS));
        printf("%d\n", COLS);
        log_line(line_buff);
    }
}

void
log_line(const char *line) {
    strncpy(logd.lines[logd.linec], line, MAX_LINE_LEN - 1);
    logd.lines[logd.linec][MAX_LINE_LEN - 1] = '\0';

    // shift all lines one up if too much, drop top one
    if (logd.linec == logd.max_rows - 1) {
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
tui_endpoint(global_t *global) {
    FIELD *field[6];
    FORM  *form;
    u8     nfields = 5; // Magic number but fine; it's maximum filed for both Serial and TCP
    // s8     pos     = 0;

    //                     h   w   y              x
    WINDOW *win = newwin(11, 32, LINES / 2 - 5, COLS / 2 - 16);
    keypad(win, TRUE);

    if (global->cxt.protocol == MB_PROTOCOL_TCP) {
        nfields  = 2;
        // host field
        field[0] = new_field(1, 14, 0, 0, 0, 0);
        set_field_back(field[0], A_UNDERLINE);
        set_field_buffer(field[0], 0, global->tcp_endp.host);
        // port field
        field[1] = new_field(1, 14, 1, 0, 0, 0);
        set_field_back(field[1], A_UNDERLINE);
        set_field_type(field[1], TYPE_INTEGER, 0, 0, 65535);
        char buff[16] = {0};
        snprintf(buff, 16, "%d", global->tcp_endp.tcp_port);
        set_field_buffer(field[1], 0, buff);

        field[2] = NULL;
    } else {
        for (u8 i = 0; i < nfields; i++) {
            field[i] = new_field(1, 14, i, 0, 0, 0);
        }
        // device field
        set_field_back(field[0], A_UNDERLINE);
        set_field_buffer(field[0], 0, global->sconf.device);
        // baud field
        set_field_type(field[1], TYPE_ENUM, field_enum_baud, 0, 0);
        set_field_buffer(field[1], 0, str_baud(global->sconf.baud));
        // data bits field
        set_field_type(field[2], TYPE_ENUM, field_enum_dbits, 0, 0);
        set_field_buffer(field[2], 0, str_dbits(global->sconf.data_bits));
        // stop bits field
        set_field_type(field[3], TYPE_ENUM, field_enum_sbits, 0, 0);
        set_field_buffer(field[3], 0, str_sbits(global->sconf.stop_bits));
        // parity field
        set_field_type(field[4], TYPE_ENUM, field_enum_parity, 0, 0);
        set_field_buffer(field[4], 0, str_parity(global->sconf.parity));

        field[5] = NULL;
    }

    form = new_form(field);
    set_form_win(form, win); //    h  w   y  x
    set_form_sub(form, derwin(win, 5, 16, 1, 13));
    post_form(form);

    // TODO: TMP
    box(win, 0, 0);
    if (global->cxt.protocol == MB_PROTOCOL_TCP) {
        mvwprintw(win, 0, 1, "TCP Endpoint");
        mvwprintw(win, 1, 1, "Host:   ");
        mvwprintw(win, 2, 1, "Port:   ");

        mvwprintw(win, 8, 1, "F1 - Submit");
        mvwprintw(win, 9, 1, "F2 - Cancel");
    } else {
        mvwprintw(win, 0, 1, "Serial Endpoint");
        mvwprintw(win, 1, 1, "Device:   ");
        mvwprintw(win, 2, 1, "Baudrate: ");
        mvwprintw(win, 3, 1, "Data bits:");
        mvwprintw(win, 4, 1, "Stop bits:");
        mvwprintw(win, 5, 1, "Parity:   ");

        mvwprintw(win, 8, 1, "F1 - Submit");
        mvwprintw(win, 9, 1, "F2 - Cancel");
    }
    wrefresh(win);
    pos_form_cursor(form);

    while (1) {
        int ch = wgetch(win);

        switch (ch) {
        case KEY_DOWN     : form_driver(form, REQ_NEXT_FIELD); break;
        case KEY_UP       : form_driver(form, REQ_PREV_FIELD); break;
        case KEY_LEFT     : form_driver(form, REQ_PREV_CHOICE); break;
        case KEY_RIGHT    : form_driver(form, REQ_NEXT_CHOICE); break;
        case KEY_BACKSPACE: form_driver(form, REQ_DEL_PREV); break;

        // cancel
        case KEY_F(2): close_dialog(win, form, field, nfields); return;

        // try submit
        case KEY_F(1):
            // make sure all buffer available and valid
            if (form_driver(form, REQ_VALIDATION) != E_OK) {
                break;
            }

            if (global->cxt.protocol == MB_PROTOCOL_TCP) {
                tcp_endp tcp = {0};
                if (tcp_ednp_from_str(&tcp, field_buffer(field[0], 0), field_buffer(field[1], 0))) {
                    memcpy(&global->tcp_endp, &tcp, sizeof(tcp));

                    relink(global);
                    close_dialog(win, form, field, nfields);
                    return;
                }
            } else {
                serial_cfg new_conf = {0};
                if (sconf_from_str(&new_conf,
                                   field_buffer(field[0], 0),
                                   field_buffer(field[1], 0),
                                   field_buffer(field[2], 0),
                                   field_buffer(field[3], 0),
                                   field_buffer(field[4], 0))) {
                    memcpy(&global->sconf, &new_conf, sizeof(new_conf));

                    relink(global);
                    close_dialog(win, form, field, nfields);
                    return;
                }
                // serial config is bad, just continue with this window
                break;
            }

        default: form_driver(form, ch); break;
        }
    }
}

// ---------------------------- UID --------------------------------------------

void
tui_uid(global_t *global) {
    FIELD *field[3];
    FORM  *form;
    u8     nfields = 3; // Magic number but fine; it's maximum filed for both Serial and TCP

    //                     h   w   y              x
    WINDOW *win = newwin(7, 32, LINES / 2 - 5, COLS / 2 - 16);
    keypad(win, TRUE);

    // end field
    field[0] = new_field(1, 14, 0, 0, 0, 0);
    set_field_back(field[0], A_UNDERLINE);
    set_field_type(field[0], TYPE_INTEGER, 0, 0, 255);
    char buff[16] = {0};
    snprintf(buff, 16, "%d", global->slave_id_start);
    set_field_buffer(field[0], 0, buff);
    // end field
    field[1] = new_field(1, 14, 1, 0, 0, 0);
    set_field_back(field[1], A_UNDERLINE);
    set_field_type(field[1], TYPE_INTEGER, 0, 0, 255);
    memset(buff, 0, 16);
    snprintf(buff, 16, "%d", global->slave_id_end);
    set_field_buffer(field[1], 0, buff);

    field[2] = NULL;

    form = new_form(field);
    set_form_win(form, win); //    h  w   y  x
    set_form_sub(form, derwin(win, 5, 16, 1, 13));
    post_form(form);

    box(win, 0, 0);
    mvwprintw(win, 0, 1, "Unit ID");
    mvwprintw(win, 1, 1, "UID Start: ");
    mvwprintw(win, 2, 1, "UID End:   ");
    // mvwprintw(win, 7, 1, "Status ok");
    mvwprintw(win, 4, 1, "F1 - Submit");
    mvwprintw(win, 5, 1, "F2 - Cancel");
    wrefresh(win);
    pos_form_cursor(form);

    while (1) {
        int ch = wgetch(win);

        switch (ch) {
        case KEY_DOWN     : form_driver(form, REQ_NEXT_FIELD); break;
        case KEY_UP       : form_driver(form, REQ_PREV_FIELD); break;
        case KEY_BACKSPACE: form_driver(form, REQ_DEL_PREV); break;

        // cancel
        case KEY_F(2): close_dialog(win, form, field, nfields); return;

        // sumbit
        case KEY_F(1): {
            // make sure all buffer available and valid
            if (form_driver(form, REQ_VALIDATION) != E_OK) {
                break;
            }

            int start = 0;
            int end   = 0;
            if (uid_from_str(&start, &end, field_buffer(field[0], 0), field_buffer(field[1], 0))) {
                global->slave_id_start = start;
                global->slave_id_end   = end;
                close_dialog(win, form, field, nfields);
                return;
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
tui_fc(global_t *global) {
    //                   h   w   y              x
    WINDOW *wfc = newwin(13, 48, LINES / 2 - 7, COLS / 2 - 24);
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
            global->cxt.fc = ind_to_fc(ch - KEY_1);
            redraw_header(global);
            break;
        }
    }

    delwin(wfc);
    redraw_log();
}

// ---------------------------- Qantities and addresses ------------------------

void
tui_qty_addr(global_t *global) {
    FIELD *field[5];
    FORM  *form;
    u8     nfields = 4; // Magic number but fine; it's maximum filed for both Serial and TCP

    //                   h   w   y              x
    WINDOW *win = newwin(9, 26, LINES / 2 - 5, COLS / 2 - 16);
    keypad(win, TRUE);

    for (int i = 0; i < nfields; i++) {
        field[i] = new_field(1, 6, i, 0, 0, 0);
        set_field_back(field[i], A_UNDERLINE);
    }

    // read address field
    set_field_type(field[0], TYPE_INTEGER, 0, 0, 0xFFFF);
    char buff[16] = {0};
    snprintf(buff, 16, "%d", global->cxt.raddress);
    set_field_buffer(field[0], 0, buff);

    // read count field
    set_field_type(field[1], TYPE_INTEGER, 0, 0, 2000);
    memset(buff, 0, 16);
    snprintf(buff, 16, "%d", global->cxt.rcount);
    set_field_buffer(field[1], 0, buff);

    // write address field
    set_field_type(field[2], TYPE_INTEGER, 0, 0, 0xFFFF);
    memset(buff, 0, 16);
    snprintf(buff, 16, "%d", global->cxt.waddress);
    set_field_buffer(field[2], 0, buff);

    // write count field
    set_field_type(field[3], TYPE_INTEGER, 0, 0, 0x07B0);
    memset(buff, 0, 16);
    snprintf(buff, 16, "%d", global->cxt.wcount);
    set_field_buffer(field[3], 0, buff);

    field[4] = NULL;

    form = new_form(field);
    set_form_win(form, win); //    h  w   y  x
    set_form_sub(form, derwin(win, 5, 6, 1, 18));
    post_form(form);

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

        switch (ch) {
        case KEY_DOWN     : form_driver(form, REQ_NEXT_FIELD); break;
        case KEY_UP       : form_driver(form, REQ_PREV_FIELD); break;
        case KEY_BACKSPACE: form_driver(form, REQ_DEL_PREV); break;

        // cancel
        case KEY_F(2): close_dialog(win, form, field, nfields); return;

        // sumbit
        case KEY_F(1): {
            // make sure all buffer available and valid
            if (form_driver(form, REQ_VALIDATION) != E_OK) {
                break;
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
                global->cxt.raddress = raddr;
                global->cxt.rcount   = rcount;
                global->cxt.waddress = waddr;
                global->cxt.wcount   = wcount;
                close_dialog(win, form, field, nfields);
                return;
            }
            break;
        }

        default: form_driver(form, ch); break;
        }
    }
}

// ---------------------------- Timeouts ---------------------------------------

void
tui_timeouts(global_t *global) {
    FIELD *field[3];
    FORM  *form;
    u8     nfields = 3; // Magic number but fine; it's maximum filed for both Serial and TCP

    //                   h   w   y              x
    WINDOW *win = newwin(7, 27, LINES / 2 - 5, COLS / 2 - 16);
    keypad(win, TRUE);

    // response timeout field
    field[0] = new_field(1, 6, 0, 0, 0, 0);
    set_field_back(field[0], A_UNDERLINE);
    set_field_type(field[0], TYPE_INTEGER, 0, 0, 10000);
    char buff[16] = {0};
    snprintf(buff, 16, "%d", global->response_timeout);
    set_field_buffer(field[0], 0, buff);
    // send timeout field
    field[1] = new_field(1, 6, 1, 0, 0, 0);
    set_field_back(field[1], A_UNDERLINE);
    set_field_type(field[1], TYPE_INTEGER, 0, 0, 10000);
    memset(buff, 0, 16);
    snprintf(buff, 16, "%d", global->timeout);
    set_field_buffer(field[1], 0, buff);

    field[2] = NULL;

    form = new_form(field);
    set_form_win(form, win); //    h  w   y  x
    set_form_sub(form, derwin(win, 5, 6, 1, 19));
    post_form(form);

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

        switch (ch) {
        case KEY_DOWN     : form_driver(form, REQ_NEXT_FIELD); break;
        case KEY_UP       : form_driver(form, REQ_PREV_FIELD); break;
        case KEY_BACKSPACE: form_driver(form, REQ_DEL_PREV); break;

        // cancel
        case KEY_F(2): close_dialog(win, form, field, nfields); return;

        // sumbit
        case KEY_F(1): {
            // make sure all buffer available and valid
            if (form_driver(form, REQ_VALIDATION) != E_OK) {
                break;
            }

            int rtimeout = 0;
            int stimeout = 0;
            if (timeouts_from_str(&rtimeout, &stimeout, field_buffer(field[0], 0), field_buffer(field[1], 0))) {
                global->response_timeout = rtimeout;
                global->timeout          = stimeout;
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
tui_wdata(global_t *global) {
    FIELD *field[2];
    FORM  *form;
    u8     nfields = 1; // Magic number but fine; it's maximum filed for both Serial and TCP

    const u8 input_line_len = 40;
    const u8 reg_str_len    = 4; // 4 bytes for register presentation; choosed as bigger of coils/regs

    // 4 chars for one reg + space for all of them exept last one on the line
    const u8 regs_on_line = input_line_len / (reg_str_len + 1);
    const u8 nlines       = WD_MAX_LEN / regs_on_line + !!(WD_MAX_LEN % regs_on_line);

    // len of "Data: " + len of input line + 2 border lines
    const u8 win_width  = 8 + input_line_len + 2;
    // rows for nlines, division line, submit, cancel AND two border lines
    const u8 win_height = nlines + 3 + 2;

    const u8 current_reg_count = CLAMP(global->cxt.wcount, 0, WD_MAX_LEN);

    //                   h           w          y                           x
    WINDOW *win = newwin(win_height, win_width, LINES / 2 - win_height / 2, COLS / 2 - win_width / 2);
    keypad(win, TRUE);

    // response timeout field
    field[0] = new_field(nlines, input_line_len, 0, 0, 0, 0);
    set_field_back(field[0], A_UNDERLINE);

    // write data to buffer
    int   buff_len = input_line_len * nlines + 1;
    char *buff     = calloc(buff_len, sizeof(u8));
    for (u8 i = 0; i < current_reg_count; i++) {
        sprintf(&buff[i * 5], "%04X ", global->cxt.wdata[i]);
    }
    buff[buff_len - 1] = '\0';
    set_field_buffer(field[0], 0, buff);
    free(buff);

    field[1] = NULL;

    form = new_form(field);
    set_form_win(form, win); //    h       w               y  x
    set_form_sub(form, derwin(win, nlines, input_line_len, 1, 8));
    post_form(form);

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
                wdata_from_str(global, field_buffer(field[0], 0));
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
input_thread(void *global) {
    global_t *g = (global_t *)global;

    while (1) {
        int key = getch();
        if (key == ERR) {
            continue;
        }

        // TUI shouldn't change anything while client actualy running requests
        if (g->running && key != KEY_F(5)) {
            continue;
        }

        switch (key) {
        case KEY_1: g->cxt.protocol = (g->cxt.protocol + 1) % MB_PROTOCOL_MAX; break;
        case KEY_2: tui_endpoint(g); break;
        case KEY_3: tui_uid(g); break;
        case KEY_4: tui_fc(g); break;
        case KEY_5: tui_qty_addr(g); break;
        case KEY_6: tui_wdata(g); break;

        case KEY_F(5): g->running = ~g->running; break;
        case KEY_F(6): g->random = ~g->random; break;
        case KEY_F(8):
            g->stats.requests = 0;
            g->stats.success  = 0;
            g->stats.timeouts = 0;
            g->stats.fails    = 0;
            redraw_header(g);
            break;

        case KEY_F(9): tui_timeouts(g); break;
        }

        redraw_header(g);
    }
}
