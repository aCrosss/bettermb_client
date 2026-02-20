#define _POSIX_C_SOURCE 199309L
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "helping_hand.h"
#include "tui.h"

// ================================================================================
// Basic Conversions
// ================================================================================

int
parse_int(const char *string, int *value) {
    int   tmp_value;
    char *endptr;

    errno     = 0;
    tmp_value = strtol(string, &endptr, 0 /* base 16 or 10 */);
    if (errno != 0) {
        log_linef("'%s': %s\n", string, strerror(errno));
        return RC_FAIL;
    }

    if (endptr == string) {
        log_linef("'%s': No digits were found\n", string);
        return RC_FAIL;
    }

    if (*endptr != '\0') {
        log_linef("'%s': Not a valid number representation\n", string);
        return RC_FAIL;
    }

    *value = tmp_value;

    return RC_SUCCESS;
}

void
trim_spaces(char *str) {
    int len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

char
nibble_to_hex(u8 d) {
    char c = 0;

    if (d < 10) {
        c = d + '0';
    } else {
        c = d - 10 + 'A';
    }

    return c;
}

int
hex_to_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

u8
hex_to_digit(char ch1, char ch2) {
    u8 hi = hex_to_nibble(ch1);
    u8 lo = hex_to_nibble(ch2);
    return (hi << 4) | (lo << 0);
}

// ================================================================================
// Client Conversions
// ================================================================================

int
sconf_from_str(serial_cfg *sconf, char *device, char *baud, char *dbits, char *sbits, const char *parity) {
    trim_spaces(device);
    trim_spaces(baud);
    trim_spaces(dbits);
    trim_spaces(sbits);

    if (access(device, F_OK) != 0) {
        log_linef("! device doesn't exist: '%s'\n", device);
        return RC_FAIL;
    }
    strncpy(sconf->device, device, 32);

    if (parse_int(baud, &sconf->baud) < 0) {
        return RC_FAIL;
    } else if (sconf->baud < 0 || sconf->baud > 921600) {
        log_line("! baudrate must be between 0 and 921600");
        return RC_FAIL;
    }

    if (parse_int(dbits, &sconf->data_bits) < 0) {
        return RC_FAIL;
    } else if (sconf->data_bits < 5 || sconf->data_bits > 8) {
        log_line("! data bits can be 5, 6, 7 or 8");
        return RC_FAIL;
    }

    if (parse_int(sbits, &sconf->stop_bits) < 0) {
        return RC_FAIL;
    } else if (sconf->stop_bits < 1 || sconf->stop_bits > 2) {
        log_linef("! stop bits can be 1 or 2, but got %d", sconf->stop_bits);
        return RC_FAIL;
    }

    if (strcmp(parity, "N")) {
        sconf->parity = 'N';
    } else if (strcmp(parity, "O")) {
        sconf->parity = 'O';
    } else if (strcmp(parity, "E")) {
        sconf->parity = 'V';
    } else {
        log_line("! invalid parity, can be N|O|E");
        return RC_FAIL;
    }

    log_linef("");
    log_linef("New serial config:");
    log_linef("> device   : %s", sconf->device);
    log_linef("> baud     : %d", sconf->baud);
    log_linef("> data bits: %d", sconf->data_bits);
    log_linef("> stop bits: %d", sconf->stop_bits);
    log_linef("> parity   : %c", sconf->parity);
    log_linef("");

    return RC_SUCCESS;
}

int
tcp_ednp_from_str(tcp_endp *tcp, char *host, char *port) {
    trim_spaces(host);
    trim_spaces(port);

    // ip validation
    if (!validate_ip(host)) {
        log_linef("! invalid ip address: %s", host);
        return RC_FAIL;
    }
    strncpy(tcp->host, host, 16);

    if (parse_int(port, &tcp->tcp_port) < 0) {
        log_linef("! invalid tcp port: %d", tcp->tcp_port);
        return RC_FAIL;
    }

    return RC_SUCCESS;
}

int
uid_from_str(int *start, int *end, char *s_start, char *s_end, u8 sequence) {
    trim_spaces(s_start);

    if (parse_int(s_start, start) < 0) {
        log_linef("! invalid uid start: %d", *start);
        return RC_FAIL;
    } else if (*start < 0 || *start > 255) {
        log_linef("! uid start must be between 0 and 255: %d", *start);
        return RC_FAIL;
    }

    if (sequence) {
        trim_spaces(s_end);

        if (parse_int(s_end, end) < 0) {
            log_linef("! invalid uid end: %d", *end);
            return RC_FAIL;
        } else if (*end < 0 || *end > 255) {
            log_linef("! uid end must be between 0 and 255: %d", *end);
            return RC_FAIL;
        }

        if (*start > *end) {
            log_linef("! start must be less or equal to end");
            return RC_FAIL;
        }
    } else {
        *end = *start;
    }

    return RC_SUCCESS;
}

int
qnt_addr_from_str(int *ra, int *rc, int *wa, int *wc, char *sra, char *src, char *swa, char *swc) {
    trim_spaces(sra);
    trim_spaces(src);
    trim_spaces(swa);
    trim_spaces(swc);

    if (parse_int(sra, ra) < 0) {
        log_linef("! invalid read address: %d", *sra);
        return RC_FAIL;
    }

    if (parse_int(src, rc) < 0) {
        log_linef("! invalid read address: %d", *src);
        return RC_FAIL;
    }

    if (parse_int(swa, wa) < 0) {
        log_linef("! invalid read address: %d", *swa);
        return RC_FAIL;
    }

    if (parse_int(swc, wc) < 0) {
        log_linef("! invalid read address: %d", *swc);
        return RC_FAIL;
    }
    // Note: upper and above 0 boundaries should be handled by tui, make other checks
    // is pointless and better to lay it on pdu and adu building functions and endpoint's
    // request validation. worst case scenarion should be: user fails with request
    // building or response and change this settings

    return RC_SUCCESS;
}

int
timeouts_from_str(int *rtout, int *stout, char *s_rtout, char *s_stout) {
    trim_spaces(s_rtout);
    trim_spaces(s_stout);

    if (parse_int(s_rtout, rtout) < 0) {
        log_linef("! invalid response tiomeout: %d", *rtout);
        return RC_FAIL;
    } else if (*rtout < 0 || *rtout > 10000) {
        log_linef("! uid start must be between 0 and 10 000 ms: %d", *rtout);
        return RC_FAIL;
    }

    if (parse_int(s_stout, stout) < 0) {
        log_linef("! invalid send timeout: %d", *stout);
        return RC_FAIL;
    } else if (*stout < 0 || *stout > 10000) {
        log_linef("! uid end must be between 0 and 10 000 ms: %d", *stout);
        return RC_FAIL;
    }

    return RC_SUCCESS;
}

int
wdata_from_str(global_t *global, char *str) {
    trim_spaces(str);

    memset(global->cxt.wdata, 0, sizeof(global->cxt.wdata));

    u16   temp_data[WD_MAX_LEN] = {0};
    int   ind                   = 0;
    char *endpt;

    char *pch = strtok(str, " ");
    while (pch != NULL && ind < WD_MAX_LEN) {
        int tmp = strtol(pch, &endpt, 16);

        temp_data[ind] = tmp;

        ind++;
        pch = strtok(NULL, " ");
    }

    for (int i = 0; i < ind; i++) {
        global->cxt.wdata[i] = temp_data[i];
    }

    return RC_SUCCESS;
}

int
int_from_str(int *i, char *str) {
    trim_spaces(str);

    return parse_int(str, i);
}

void
str_curr_endpoint(char out[32], global_t *global) {
    memset(out, 0, 32);

    switch (global->cxt.protocol) {
    case MB_PROTOCOL_RTU:
    case MB_PROTOCOL_ASCII:
        if (!global->sconf.device) {
            log_line("! serial endpoint undefined");
            snprintf(out, 32, "<null>");
            break;
        }

        strncpy(out, global->sconf.device, 32);
        break;

    case MB_PROTOCOL_TCP:
        if (!global->tcp_endp.host || !global->tcp_endp.tcp_port) {
            log_line("! TCP endpoint undefined");
            snprintf(out, 32, "<null>");
            break;
        }

        snprintf(out, 32, "%s:%d", global->tcp_endp.host, global->tcp_endp.tcp_port);
        break;
    }
}

// ================================================================================
// Helping Functions
// ================================================================================

u64
now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000ull + (u64)ts.tv_nsec / 1000000ull;
}

void
msleep(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

// WRITE, READ, BITS
int
fc_flags(int function_code) {
    switch (function_code) {
    case MB_FC_READ_COILS:
    case MB_FC_READ_DISCRETE_INPUTS: return FCF_READ | FCF_BITS;
    case MB_FC_READ_HOLDING_REGISTERS:
    case MB_FC_READ_INPUT_REGISTERS: return FCF_READ;
    case MB_FC_WRITE_SINGLE_COIL: return FCF_WRITE | FCF_BITS;
    case MB_FC_WRITE_SINGLE_REGISTER: return FCF_WRITE;
    case MB_FC_WRITE_MULTIPLE_COILS: return FCF_WRITE | FCF_BITS;
    case MB_FC_WRITE_MULTIPLE_REGISTERS: return FCF_WRITE;
    case MB_FC_WRITE_AND_READ_REGISTERS: return FCF_WRITE | FCF_READ;
    }

    return -1;
}

rc_t
validate_ip(const char *ip) {
    ip_addr_t addr;
    return (rc_t)inet_pton(AF_INET, ip, &addr);
}