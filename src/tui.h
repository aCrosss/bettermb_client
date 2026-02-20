#ifndef SCREEN_H
#define SCREEN_H

#include <form.h>
#include <ncurses.h>

#include "client_cxt.h"

#define HEADER_BOTTOM 16 // header bottom position in y coords

#define CSV_IND_CAP   8
#define CSV_LINES_CAP 1023

#define NEW_WIN(h, w, y, x)     (newwin(h, w, y, x))
#define NEW_FIELD(h, w, y, x)   (new_field(h, w, y, x, 0, 0))
#define DERWIN(win, h, w, y, x) (derwin(win, h, w, y, x))

// ======= BOILERPLATE ======= //

// will produce
// clang-format off
#define TUIDW_HEAD(nf, wh, ww, wy, wx)    \
    FIELD *field[nf + 1];                 \
    FORM  *form;                          \
    u8     nfields = nf;                  \
    WINDOW *win = NEW_WIN(wh, ww, wy, wx);\
    keypad(win, TRUE);                    \
    field[nf] = NULL;


#define TUIDW_SUBFORM(dwh, dww, dwy, dwx)               \
    form = new_form(field);                             \
    set_form_win(form, win);                            \
    set_form_sub(form, DERWIN(win, dwh, dww, dwy, dwx));\
    post_form(form);


#define TUIDW_STDDRIVER                                          \
    case KEY_DOWN     : form_driver(form, REQ_NEXT_FIELD); break;\
    case KEY_UP       : form_driver(form, REQ_PREV_FIELD); break;\
    case KEY_BACKSPACE: form_driver(form, REQ_DEL_PREV);   break;\
    case KEY_F(2): close_dialog(win, form, field, nfields); return;

// clang-format on

// ======= BOILERPLATE ======= //

#define KEY_1 49
#define KEY_2 50
#define KEY_3 51
#define KEY_4 52
#define KEY_5 53
#define KEY_6 54
#define KEY_7 55
#define KEY_8 56
#define KEY_9 57
#define KEY_0 48

#define MAX_LINES    64
#define MAX_LINE_LEN 1024

typedef struct log {
    u8 linel;
    u8 linec;
    u8 max_rows;
    s8 lines[MAX_LINES][MAX_LINE_LEN];

    // csv log
    u8    last_err[MAX_LINE_LEN];
    u8    find;
    u32   lind;
    FILE *csv;
} log_t;

typedef enum {
    DS_IN_OK,
    DS_IN_FAIL,
    DS_OUT_OK,
    DS_OUT_FAIL,
} dirstat_t;

void  init_tui(global_t *global);
void  destroy_tui();
void  redraw_header();
void *input_thread();
void  log_line(const char *line);
void  log_linef(const char *format, ...);
void  log_traffic_str(const char *str, dirstat_t ds);
void  log_adu(u8 adu[MB_MAX_ADU_LEN], int adu_len, mb_protocol_t protocol, dirstat_t ds);
void  log_req_errf(const char *format, ...);

#endif
