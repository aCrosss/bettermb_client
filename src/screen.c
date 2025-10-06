#include <string.h>
#include <unistd.h>

#include "mb_base.h"
#include "screen.h"

global_t *pglobals;

// WINDOW *win = newwin(10, 32, y, x);

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
redraw_header(global_t *global) {
    wclear(wheader);
    box(wheader, 0, 0);

    s8 endp[32];
    str_curr_endpoint(endp, global);

    mvwprintw(wheader, 1, 1, "1 | Protocol: %s", str_protocol(global->cxt.protocol));
    mvwprintw(wheader, 2, 1, "2 | Endpoint: %s", endp);
    mvwprintw(wheader, 3, 1, "3 | Unit IDs: %d", global->slave_id_start);
    mvwprintw(wheader, 4, 1, "4 | Function: %s", str_fc(global->cxt.fc));
    //
    mvwprintw(wheader, 6, 1, "5 | Read address : 0x%04X", global->cxt.raddress);
    mvwprintw(wheader, 7, 1, "6 | Read count   : %d", global->cxt.rcount);
    mvwprintw(wheader, 8, 1, "7 | Write address: 0x%04X", global->cxt.waddress);
    mvwprintw(wheader, 9, 1, "8 | Write count  : %d", global->cxt.wcount);
    mvwprintw(wheader, 10, 1, "9 | Write Data  : ");

    mvwprintw(wheader, 1, 64, "F5 | Running: %s", global->running ? "On" : "Off");
    mvwprintw(wheader, 2, 64, "F6 | Random:  %s", global->random ? "On" : "Off");

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
    // box(wheader, 0, 0);
    redraw_header(global);
    // refresh();

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
log_adu(const char *endp, u8 adu[MB_MAX_ADU_LEN], int adu_len, mb_protocol_t protocol, dirstat_t ds) {
    u8 buff[MAX_LINE_LEN] = {0};

    u8 left_side[MAX_LINE_LEN] = {0};
    sprintf(left_side, "%s %s", endp, str_dirstat(ds));

    u8 payload[1024];
    format_payload(payload, adu, adu_len, protocol);
    int payload_len = strlen(payload);

    snprintf(buff, MAX_LINE_LEN - 1, "%s %s", left_side, payload);
    buff[MAX_LINE_LEN - 1] = '\0';
    // int cursor              = strlen(buff) - strlen(left_side);
    // payload_len            -= cursor;

    // log_line(buff);
    // return;

    int lines = strlen(buff) / COLS + 1;
    for (int i = 0; i < lines; i++) {
        u8 line_buff[MAX_LINE_LEN]  = {0};
        // TODO: clamp between COLS and MAX LINE LEN
        // snprintf(line_buff, COLS - 1, "%s", buff[i * COLS]);
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

void
handle_user_input() {
    // ?
}

void *
input_thread(void *global) {
    global_t *g = (global_t *)global;

    while (1) {
        int key = getch();
        if (key == ERR) {
            continue;
        }

        log_linef("> %d pressed", key);
        // TUI shouldn't change anything while client actualy running requests
        if (g->running && key != KEY_F(5)) {
            continue;
        }

        switch (key) {
        case KEY_1:
            log_line("1 pressed");
            g->cxt.protocol = (g->cxt.protocol + 1) % MB_PROTOCOL_MAX;
            break;
        case KEY_2: break;
        case KEY_3: break;

        case KEY_F(5):
            log_line("F5 pressed");
            g->running = ~g->running;
            break;

        case KEY_F(6):
            log_line("F6 pressed");
            g->random = ~g->random;
            break;
        }

        redraw_header(g);
    }
}
