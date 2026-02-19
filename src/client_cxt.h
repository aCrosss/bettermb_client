#ifndef CLIENT_CXT_H
#define CLIENT_CXT_H

#include "types.h"

#define WD_MAX_LEN 125 // maximum ammount of custom coils/regs data to write

typedef struct {
    char device[32];
    int  baud;
    char parity;
    int  data_bits;
    int  stop_bits;
} serial_cfg;

typedef struct {
    char host[16];
    int  tcp_port;
} tcp_endp;

typedef struct {
    mb_protocol_t protocol;
    mb_protocol_t last_run_was_on;

    fc_t fc;
    int  waddress;
    int  wcount;
    int  raddress;
    int  rcount;

    u16 wdata[WD_MAX_LEN];

    u16 tid;
    int fd;
} client_cxt_t;

typedef struct statistic {
    u32 requests;
    u32 success;
    u32 timeouts;
    u32 fails;
} statistic_t;

typedef struct global {
    serial_cfg sconf;
    tcp_endp   tcp_endp;

    client_cxt_t cxt;

    statistic_t stats;

    int slave_id_start;
    int slave_id_end;
    int response_timeout; // ms

    u8  running;
    int timeout; // ms
    int random;

    u64 time_start;
} global_t;

int init_client(int argc, char **argv, global_t *global);

#endif
