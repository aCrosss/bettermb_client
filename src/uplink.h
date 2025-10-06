#ifndef UPLINK_H
#define UPLINK_H

#include "client_cxt.h"

int
open_uplink(global_t *global);
int
set_socket_timeout(int fd, u32 ms);
int
relink(global_t *global);

#endif
