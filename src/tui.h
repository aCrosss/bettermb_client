#ifndef SCREEN_H
#define SCREEN_H

#include <form.h>
#include <ncurses.h>

#include "client_cxt.h"

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
} log_t;

typedef enum {
    DS_IN_OK,
    DS_IN_FAIL,
    DS_OUT_OK,
    DS_OUT_FAIL,
} dirstat_t;

void
init_tui(global_t *global);
void
destroy_tui();
void
redraw_header();
void *
input_thread();
void
log_line(const char *line);
void
log_linef(const char *format, ...);
void
log_traffic_str(const char *str, dirstat_t ds);
void
log_adu(u8 adu[MB_MAX_ADU_LEN], int adu_len, mb_protocol_t protocol, dirstat_t ds);

#endif
