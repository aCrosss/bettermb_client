#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include "tui.h"
#include "types.h"
#include "uplink.h"

static int
get_baud(int baud) {
    switch (baud) {
    case 0: return B0;
    case 50: return B50;
    case 75: return B75;
    case 110: return B110;
    case 134: return B134;
    case 150: return B150;
    case 200: return B200;
    case 300: return B300;
    case 600: return B600;
    case 1200: return B1200;
    case 1800: return B1800;
    case 2400: return B2400;
    case 4800: return B4800;
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    }

    return RC_FAIL;
}

static int
open_serial(serial_cfg *sconf) {
    int fd = open(sconf->device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        log_linef("! failed to create serial connection: %s", strerror(errno));
        return RC_ERROR;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        log_line("! failed to get termios");
        return RC_ERROR;
    }

    // baud
    int baud = get_baud(sconf->baud);
    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);

    // parity
    switch (sconf->parity) {
    case 'O':
    case 'o':
        tty.c_cflag |= PARENB;
        tty.c_cflag |= PARODD;
        break;
    case 'E':
    case 'e':
        tty.c_cflag |= PARENB;
        tty.c_cflag &= ~PARODD;
        break;
    default: tty.c_cflag &= ~PARENB; break;
    }

    // data bits
    switch (sconf->data_bits) {
    case 5: tty.c_cflag |= CS5; break;
    case 6: tty.c_cflag |= CS6; break;
    case 7: tty.c_cflag |= CS7; break;
    case 8: tty.c_cflag |= CS8; break;
    default: log_line("! invalid data bits"); return RC_ERROR;
    }

    // stop bits
    switch (sconf->stop_bits) {
    case 1: tty.c_cflag &= ~CSTOPB; break;
    case 2: tty.c_cflag |= CSTOPB; break;
    default: log_line("! invalid stop bits"); return RC_ERROR;
    }

    if (tcsetattr(fd, TCSANOW, &tty) < 0) {
        log_line("! failed to accept termios config");
        return RC_ERROR;
    }

    log_linef("> openned serial connection (fd: %d)", fd);

    return fd;
}

int
open_tcp(tcp_endp *endp) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | O_NONBLOCK, 0);
    if (fd < 0) {
        log_linef("! failed to create tcp connection: %s", strerror(errno));
        return RC_ERROR;
    }

    in_addr_t ip_addr = inet_addr(endp->host);

    struct sockaddr_in sa = {
      .sin_family      = AF_INET,
      .sin_port        = htons(endp->tcp_port),
      .sin_addr.s_addr = ip_addr,
    };

    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        if (errno != EINPROGRESS) {
            log_linef("! failed to connect socket (fd: %s)", strerror(errno));
            close(fd);
            return RC_ERROR;
        }
    }

    u8  buff[1] = {0};
    // should return 0 and errno WOULDBLOCK or EAGAIN
    int rc      = write(fd, buff, 0);
    if (rc < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            close(fd);
            return RC_ERROR;
        }
    }

    log_linef("> oppened tcp connection (fd: %d): %s:%d", fd, endp->host, endp->tcp_port);

    return fd;
}

int
open_uplink(global_t *global) {
    int fd;

    switch (global->cxt.protocol) {
    case MB_PROTOCOL_RTU:
    case MB_PROTOCOL_ASCII: fd = open_serial(&global->sconf); break;
    case MB_PROTOCOL_TCP: fd = open_tcp(&global->tcp_endp); break;
    }

    if (fd < 0) {
        return RC_FAIL;
    }

    global->cxt.fd = fd;
    return RC_SUCCESS;
}

int
relink(global_t *global) {
    // if something opened, close it
    if (global->cxt.fd > 2) { // 0 - stdin, 1 - stdout, 2 - stderr
        close(global->cxt.fd);
    }

    log_linef("> reopening connection");
    // reopen new socket
    if (open_uplink(global)) {
        global->cxt.last_run_was_on = global->cxt.protocol;
        return RC_SUCCESS;
    } else {
        return RC_FAIL;
    }
}
