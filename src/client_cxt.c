#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "client_cxt.h"
#include "tui.h"
#include "uplink.h"

void
msleep(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

static int
parse_int(const char *string, int *value) {
    int   tmp_value;
    char *endptr;

    errno     = 0;
    tmp_value = strtol(string, &endptr, 0 /* base 16 or 10 */);
    if (errno != 0) {
        printf("'%s': %s\n", string, strerror(errno));
        return -1;
    }

    if (endptr == string) {
        printf("'%s': No digits were found\n", string);
        return -1;
    }

    if (*endptr != '\0') {
        printf("'%s': Not a valid number representation\n", string);
        return -1;
    }

    *value = tmp_value;

    return 0;
}

static void
help(const char *progname) {
    const char *help_message =
      "Usage: %s [-h|--usage] tcp       HOST   [OPTIONS] [WRITE VALUES]\n"
      "   or: %s [-h|--usage] rtu|ascii DEVICE [OPTIONS] [WRITE VALUES]\n"
      "Send Modbus TCP|RTU|ASCII request to remote slave device.\n"
      "WRITE VALUES can be in decimal or hexidecimal, like so:\n"
      " decimal:     0 2 5 11 23 ...\n"
      " hexidecimal: 0x0 0x2 0x5 0xB 0x17 ...\n"
      "For functions 1-2, 15: WRITE VALUES are binary values (0 or 1).\n"
      "For other functions:   WRITE VALUES are 16-bit integers.\n\n"
      " TCP options:\n"
      "  -t, --tcp-port=NUM                       TCP port of Modbus TCP slave (1-65535).\n"
      "                                           Default: 502.\n\n"
      " Serial options:\n"
      "  -b, --baudrate=NUM                       Transmission speed (1200-921600 bps).\n"
      "                                           Default: 115200.\n"
      "  -p, --parity=N|O|E                       Parity Bit (None, Odd, Even).\n"
      "                                           Default: None.\n"
      "      --data-bits=5|6|7|8                  Number of data bits in frame (5-8).\n"
      "                                           Default: 8.\n"
      "      --stop-bits=1|2                      Number of stop bits in frame (1-2).\n"
      "                                           Default: 1.\n"
      " Protocol options:\n"
      "  -s, --slave-start=NUM                    Slave (Unit) ID range start (1-255).\n"
      "                                           Default: 1.\n"
      "  -e, --slave-end=NUM                      Slave (Unit) ID range end (1-255).\n"
      "                                           Default: 1.\n"
      "  -f, --function=NUM                       Modbus command function code (1-6, 15-16, 23).\n"
      "                                           Default: 1.\n"
      "  -R, --read-addr=NUM                      Read address (in remote slave device) (0-65535).\n"
      "                                           Default: 0.\n"
      "  -r, --read-count=NUM                     Read count (max count depends on function).\n"
      "                                           Default: 1.\n"
      "  -W, --write-addr=NUM                     Write address (in remote slave device) (0-65535).\n"
      "                                           Default: 0.\n"
      "  -w, --write-count=NUM                    Write count (max count depends on function).\n"
      "                                           Default: 1.\n\n"
      " Miscellaneous options:\n"
      "  -q, --response_timeout=NUM               Response timeout for incoming modbus packet.\n"
      "                                           Default: 500.\n"
      "  -l, --random                             Use random bytes as data for write commands.\n"
      "                                           Default: No.\n"
      "  -c, --count=NUM                          How many times to send request (0 - don't stop sending)\n"
      "                                           Default: 1.\n"
      "  -T, --timeout=NUM                        Timeout between requests (ms) (0-600000).\n"
      "                                           Default: 1000.\n\n"
      "  -h, --help                               Give this help list\n"
      "      --usage                              Give a short usage message\n";

    fprintf(stdout, help_message, progname, progname);
}

static int
handle_startup_args(int argc, char **argv, global_t *global) {
    int c;
    int parsed_opts = 0;

    int fc = 0;

    while (1) {
        int                  option_index   = 0;
        static struct option long_options[] = {
          // {long arg, use arg val, flag, short}
          {"tcp-port", OPT_ARG_REQUIRED, 0, 't'},
          {"baudrate", OPT_ARG_REQUIRED, 0, 'b'},
          {"parity", OPT_ARG_REQUIRED, 0, 'p'},
          {"data-bits", OPT_ARG_REQUIRED, 0, 0},
          {"stop-bits", OPT_ARG_REQUIRED, 0, 0},
          {"slave", OPT_ARG_REQUIRED, 0, 'a'},
          {"delay", OPT_ARG_REQUIRED, 0, 'd'},
          {"random", OPT_ARG_NONE, 0, 'l'},
          // clinet part
          {"slave-start", OPT_ARG_REQUIRED, 0, 's'},
          {"slave-end", OPT_ARG_REQUIRED, 0, 'e'},
          {"function", OPT_ARG_REQUIRED, 0, 'f'},
          {"read-addr", OPT_ARG_REQUIRED, 0, 'R'},
          {"read-count", OPT_ARG_REQUIRED, 0, 'r'},
          {"write-addr", OPT_ARG_REQUIRED, 0, 'W'},
          {"write-count", OPT_ARG_REQUIRED, 0, 'w'},
          {"response-timeout", OPT_ARG_REQUIRED, 0, 'q'},
          {"count", OPT_ARG_REQUIRED, 0, 'c'},
          {"timeout", OPT_ARG_REQUIRED, 0, 'T'},
          {0},
        };

        /*
         * a   - argument w/o value
         * a:  - argument with value
         * a:: - argument with optional value, will return null w/o value
         */
        c = getopt_long(argc, argv, "t:b:p:ls:e:f:R:r:W:w:q:c:T:", long_options, &option_index);
        if (c == -1) {
            break;
        }

        parsed_opts++;
        // printf("%d\n", parsed_opts);
        switch (c) {
        case 0:
            if (strcmp(long_options[option_index].name, "data-bits") == 0) {
                if (parse_int(optarg, &global->sconf.data_bits) < 0) {
                    return -1;
                }
            } else if (strcmp(long_options[option_index].name, "stop-bits") == 0) {
                if (parse_int(optarg, &global->sconf.stop_bits) < 0) {
                    return -1;
                }
            }
            break;

        case 't':
            if (parse_int(optarg, &global->tcp_endp.tcp_port) < 0) {
                return -1;
            }
            break;

        case 'b':
            if (parse_int(optarg, &global->sconf.baud) < 0) {
                return -1;
            }
            break;

        case 'p':
            global->sconf.parity = *optarg;
            if (global->sconf.parity != 'N' && global->sconf.parity != 'O' && global->sconf.parity != 'E') {
                printf("invalid parity value: '%s', allowed: N, O or E\n", optarg);
                return -1;
            }
            break;

        case 'l': global->random = 1; break;

        case 's':
            if (parse_int(optarg, &global->slave_id_start) < 0) {
                return -1;
            }
            break;

        case 'e':
            if (parse_int(optarg, &global->slave_id_end) < 0) {
                return -1;
            }
            break;

        case 'f':
            if (parse_int(optarg, &fc) < 0) {
                return -1;
            } else {
                global->cxt.fc = fc;
            }
            break;

        case 'R':
            if (parse_int(optarg, &global->cxt.raddress) < 0) {
                return -1;
            }
            break;

        case 'r':
            if (parse_int(optarg, &global->cxt.rcount) < 0) {
                return -1;
            }
            break;

        case 'W':
            if (parse_int(optarg, &global->cxt.waddress) < 0) {
                return -1;
            }
            break;

        case 'w':
            if (parse_int(optarg, &global->cxt.wcount) < 0) {
                return -1;
            }
            break;

        case 'q':
            if (parse_int(optarg, &global->response_timeout) < 0) {
                return -1;
            }
            break;

        case 'c':
            if (parse_int(optarg, &global->request_count) < 0) {
                return -1;
            }
            break;

        case 'T':
            if (parse_int(optarg, &global->timeout) < 0) {
                return -1;
            }
            break;

        case 'h': help(""); return 0;
        default : help(""); return 0;
        }
    }

    int j = 0;
    int i = parsed_opts + 3; // progname + mode + endpoint

    // for (; (argv[i]) && (j < MB_MAX_WRITE_BITS); ++i, ++j) {
    //     if (parse_int(argv[i], &globals.write_data[j]) < 0) {
    //         printf("invalid write value\n");
    //         return -1;
    //     }
    // }

    return 1;
}

void
trim_spaces(char *str) {
    int len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

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

    // TODO: Validate ip
    strncpy(tcp->host, host, 16);

    if (parse_int(port, &tcp->tcp_port) < 0) {
        log_linef("! invalid tcp port: %d", tcp->tcp_port);
        return RC_FAIL;
    }

    return RC_SUCCESS;
}

int
init_client(int argc, char **argv, global_t *global) {
    global->sconf.baud      = 115200;
    global->sconf.parity    = 'N';
    global->sconf.data_bits = 8;
    global->sconf.stop_bits = 1;

    global->tcp_endp.tcp_port = 502;

    global->slave_id_start = 1;
    global->slave_id_end   = 1;

    global->cxt.fc       = 0x01;
    global->cxt.protocol = MB_PROTOCOL_RTU;
    global->cxt.raddress = 0;
    global->cxt.rcount   = 1;
    global->cxt.waddress = 0;
    global->cxt.wcount   = 1;

    global->response_timeout = 500;
    global->random           = 0;
    global->request_count    = 1;
    global->timeout          = 1000;

    const char *progname = argv[0];
    if (argc < 3) {
        help(progname);
        return -1;
    }

    char *mode = argv[1];
    char *endp = argv[2];

    if (strcmp(mode, "rtu") == 0) {
        global->cxt.protocol = MB_PROTOCOL_RTU;
    } else if (strcmp(mode, "ascii") == 0) {
        global->cxt.protocol = MB_PROTOCOL_ASCII;
    } else if (strcmp(mode, "tcp") == 0) {
        global->cxt.protocol = MB_PROTOCOL_TCP;
    } else {
        return RC_FAIL;
    }

    switch (global->cxt.protocol) {
    case MB_PROTOCOL_TCP: strncpy(global->tcp_endp.host, endp, 16); break;
    case MB_PROTOCOL_RTU:
    case MB_PROTOCOL_ASCII:
        if (access(endp, F_OK) != 0) {
            log_linef("%s: device doesn't exist: '%s'\n", mode, endp);
            return RC_FAIL;
        }

        strncpy(global->sconf.device, endp, 32);
        break;
    default: printf("Invalid modbus mode\n"); return -1;
    }

    return handle_startup_args(argc, argv, global);
}
