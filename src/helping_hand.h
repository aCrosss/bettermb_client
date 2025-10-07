#ifndef HElPING_HAND_H
#define HELPING_HAND_H

#include "client_cxt.h"
#include "types.h"

char
nibble_to_hex(u8 d);
int
hex_to_nibble(char c);
u8
hex_to_digit(char ch1, char ch2);
int
tcp_ednp_from_str(tcp_endp *tcp, char *host, char *port);
int
uid_from_str(int *start, int *end, char *s_start, char *s_end);
int
sconf_from_str(serial_cfg *sconf, char *device, char *baud, char *dbits, char *sbits, const char *parity);
void
str_curr_endpoint(char out[32], global_t *global);
u64
mb_tw_now_ms(void);

#endif