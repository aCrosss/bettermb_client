#ifndef HElPING_HAND_H
#    define HELPING_HAND_H

#    include "client_cxt.h"
#    include "types.h"

char nibble_to_hex(u8 d);
int  hex_to_nibble(char c);
u8   hex_to_digit(char ch1, char ch2);
int  tcp_ednp_from_str(tcp_endp *tcp, char *host, char *port);
int  uid_from_str(int *start, int *end, char *s_start, char *s_end);
int  sconf_from_str(serial_cfg *sconf, char *device, char *baud, char *dbits, char *sbits, const char *parity);
int  qnt_addr_from_str(int *ra, int *rc, int *wa, int *wc, char *sra, char *src, char *swa, char *swc);
int  timeouts_from_str(int *rtout, int *stout, char *s_rtout, char *s_stout);
int  wdata_from_str(global_t *global, char *str);
void str_curr_endpoint(char out[32], global_t *global);
u64  now_ms(void);
void msleep(int ms);
int  fc_flags(int function_code);

#endif
