#include <stdio.h>
#include <string.h>

#include "client_cxt.h"
#include "mb_base.h"
#include "tui.h"

// ======================================================================================
// Conversions
// ======================================================================================

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

const char *
str_protocol(mb_protocol_t protocol) {
    switch (protocol) {
    case MB_PROTOCOL_RTU  : return "RTU";
    case MB_PROTOCOL_ASCII: return "ASCII";
    case MB_PROTOCOL_TCP  : return "TCP";
    }
}

// TODO: Magic number
void
str_curr_endpoint(char out[32], global_t *global) {
    memset(out, 0, 32);

    // if (!global->cxt.protocol) {
    //     log_line("! protocol undefined");
    //     snprintf(out, 32, "<null>");
    //     return;
    // }

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

const char *
str_fc(fc_t fc) {
    switch (fc) {
    case MB_FC_READ_COILS              : return "01 - Read Coils (0x01)";
    case MB_FC_READ_DISCRETE_INPUTS    : return "02 - Read Discrete Inputs  (0x02)";
    case MB_FC_READ_HOLDING_REGISTERS  : return "03 - Read Holding Registers (0x03)";
    case MB_FC_READ_INPUT_REGISTERS    : return "04 - Read Input Registers (0x04)";
    case MB_FC_WRITE_SINGLE_COIL       : return "05 - Write Single Coil (0x05)";
    case MB_FC_WRITE_SINGLE_REGISTER   : return "06 - Write Single Register (0x06)";
    case MB_FC_WRITE_MULTIPLE_COILS    : return "15 - Write Multiple Coils (0x0F)";
    case MB_FC_WRITE_MULTIPLE_REGISTERS: return "16 - Write Multiple registers (0x10)";
    case MB_FC_WRITE_AND_READ_REGISTERS: return "23 - Read/Write Multiple registers (0x17)";

    default: break;
    }
}

// ======================================================================================
// Misc
// ======================================================================================

void
bit_data_to_bytes(u8 *data, int data_len, u8 *out) {
    int out_size = (data_len + 7) / 8;
    memset(out, 0, out_size);

    for (int i = 0; i < data_len; i++) {
        int byte_index   = i / 8;
        u8  val          = (data[i] == 0) ? 0 : 1;
        out[byte_index] |= val << (i % 8);
    }
}

int
fc_flags(int function_code) {
    switch (function_code) {
    case MB_FC_READ_COILS:
    case MB_FC_READ_DISCRETE_INPUTS    : return FCF_READ | FCF_BITS;
    case MB_FC_READ_HOLDING_REGISTERS  :
    case MB_FC_READ_INPUT_REGISTERS    : return FCF_READ;
    case MB_FC_WRITE_SINGLE_COIL       : return FCF_WRITE | FCF_BITS;
    case MB_FC_WRITE_SINGLE_REGISTER   : return FCF_WRITE;
    case MB_FC_WRITE_MULTIPLE_COILS    : return FCF_WRITE | FCF_BITS;
    case MB_FC_WRITE_MULTIPLE_REGISTERS: return FCF_WRITE;
    case MB_FC_WRITE_AND_READ_REGISTERS: return FCF_WRITE | FCF_READ;
    }

    return -1;
}

/* Table of CRC values for high-order byte */
static const u8 table_crc_hi[] = {
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80,
  0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
  0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
  0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
  0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
  0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00,
  0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
  0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80,
  0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01,
  0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
  0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80,
  0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
};

/* Table of CRC values for low-order byte */
static const u8 table_crc_lo[] = {
  0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D,
  0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, 0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB,
  0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2,
  0x12, 0x13, 0xD3, 0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37,
  0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, 0x3B, 0xFB, 0x39, 0xF9, 0xF8,
  0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24,
  0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26, 0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63,
  0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, 0x6E, 0xAE,
  0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F,
  0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5, 0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
  0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C,
  0x5C, 0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89,
  0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C, 0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46,
  0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80, 0x40,
};

u16
crc16(u8 *data, u16 len) {
    u8  crc_hi = 0xFF; /* high CRC byte initialized  */
    u8  crc_lo = 0xFF; /* low CRC byte initialized   */
    u32 i;             /* will index into CRC lookup */

    /* pass through message buffer */
    while (len--) {
        i      = crc_lo ^ *data++; /* calculate the CRC  */
        crc_lo = crc_hi ^ table_crc_hi[i];
        crc_hi = table_crc_lo[i];
    }

    return (crc_hi << 8 | crc_lo);
}

u8
lrc8(u8 *data, u16 len) {
    u8 lrc = 0;
    while (len--) {
        lrc += *data++;
    }
    /* Return two's complementing of the result */
    return -lrc;
}

// ======================================================================================
// Payloads
// ======================================================================================

int
build_pdu(u8 pdu[MB_MAX_PDU_LEN], u8 *data, func_cxt_t fdata) {
    int fc = fdata.fc;

    u8 byte_count = 0;

    pdu[0] = fc;
    switch (fc) {
    case MB_FC_READ_COILS:
    case MB_FC_READ_DISCRETE_INPUTS:
    case MB_FC_READ_HOLDING_REGISTERS:
    case MB_FC_READ_INPUT_REGISTERS:
        pdu[1] = HI_NIBBLE(fdata.raddress);
        pdu[2] = LO_NIBBLE(fdata.raddress);
        pdu[3] = HI_NIBBLE(fdata.rcount);
        pdu[4] = LO_NIBBLE(fdata.rcount);
        return 5;

    case MB_FC_WRITE_SINGLE_COIL:
        pdu[1] = HI_NIBBLE(fdata.waddress);
        pdu[2] = LO_NIBBLE(fdata.waddress);
        pdu[3] = (data[0] == 0) ? 0x00 : 0xFF;
        pdu[4] = 0x00;
        return 5;

    case MB_FC_WRITE_SINGLE_REGISTER:
        pdu[1] = HI_NIBBLE(fdata.waddress);
        pdu[2] = LO_NIBBLE(fdata.waddress);
        pdu[3] = data[0];
        pdu[4] = data[1];
        return 5;

    case MB_FC_WRITE_MULTIPLE_COILS:
        pdu[1] = HI_NIBBLE(fdata.waddress);
        pdu[2] = LO_NIBBLE(fdata.waddress);
        pdu[3] = HI_NIBBLE(fdata.wcount);
        pdu[4] = LO_NIBBLE(fdata.wcount);

        byte_count = (fdata.wcount + 7) / 8;
        pdu[5]     = byte_count;

        u8 write_buf[MB_MAX_ADU_LEN] = {0};
        bit_data_to_bytes(data, fdata.wcount, write_buf);
        for (int i = 0; i < byte_count; ++i) {
            pdu[6 + i] = write_buf[i];
        }

        return 6 + byte_count;

    case MB_FC_WRITE_MULTIPLE_REGISTERS:
        pdu[1] = HI_NIBBLE(fdata.waddress);
        pdu[2] = LO_NIBBLE(fdata.waddress);
        pdu[3] = HI_NIBBLE(fdata.wcount);
        pdu[4] = LO_NIBBLE(fdata.wcount);

        byte_count = fdata.wcount * 2;
        pdu[5]     = byte_count;

        for (int i = 0; i < byte_count; ++i) {
            pdu[6 + i * 2] = data[i * 2];
            pdu[7 + i * 2] = data[i * 2 + 1];
        }

        return 6 + byte_count;

    case MB_FC_WRITE_AND_READ_REGISTERS:
        pdu[1] = HI_NIBBLE(fdata.raddress);
        pdu[2] = LO_NIBBLE(fdata.raddress);
        pdu[3] = HI_NIBBLE(fdata.rcount);
        pdu[4] = LO_NIBBLE(fdata.rcount);

        pdu[5] = HI_NIBBLE(fdata.waddress);
        pdu[6] = LO_NIBBLE(fdata.waddress);
        pdu[7] = HI_NIBBLE(fdata.wcount);
        pdu[8] = LO_NIBBLE(fdata.wcount);

        byte_count = fdata.wcount * 2;
        pdu[9]     = byte_count;

        for (int i = 0; i < byte_count; ++i) {
            pdu[10 + i * 2] = data[i * 2];
            pdu[11 + i * 2] = data[i * 2 + 1];
        }

        return 10 + byte_count;
    }

    return -1;
}

int
build_adu_rtu(u8 adu[MB_RTU_MAX_ADU_LEN], frame_t *frame) {
    adu[0] = frame->uid;
    memcpy(&adu[1], frame->pdu, frame->pdu_len); // PDU includes function code

    int out_adu_len = 1 + frame->pdu_len;
    u16 crc         = crc16(adu, out_adu_len);

    adu[out_adu_len++] = LO_NIBBLE(crc);
    adu[out_adu_len++] = HI_NIBBLE(crc);
    return out_adu_len;
}

int
build_adu_ascii(u8 out_adu[MB_ASCII_MAX_ADU_LEN], frame_t *frame) {
    // binary buffer: Unit ID + Function Code + Data
    u8 bin[1 + 1 + MB_MAX_PDU_LEN];

    bin[0] = frame->uid;
    bin[1] = frame->pdu[0];

    memcpy(&bin[2], &frame->pdu[1], frame->pdu_len - 1);

    u8  lrc         = lrc8(bin, 1 + frame->pdu_len);
    int out_adu_len = 0;

    out_adu[out_adu_len++] = ':';
    for (int i = 0; i < (1 + frame->pdu_len); i++) {
        out_adu[out_adu_len++] = nibble_to_hex((bin[i] >> 4) & 0x0F);
        out_adu[out_adu_len++] = nibble_to_hex((bin[i] >> 0) & 0x0F);
    }

    // LRC
    out_adu[out_adu_len++] = nibble_to_hex((lrc >> 4) & 0x0F);
    out_adu[out_adu_len++] = nibble_to_hex((lrc >> 0) & 0x0F);
    // CRLF
    out_adu[out_adu_len++] = '\r';
    out_adu[out_adu_len++] = '\n';
    return out_adu_len;
}

int
build_adu_tcp(u8 out_adu[MB_TCP_MAX_ADU_LEN], frame_t *frame) {
    out_adu[0] = frame->tid >> 8;
    out_adu[1] = frame->tid >> 0;
    out_adu[2] = 0;
    out_adu[3] = 0;
    // [4] & [5] : MBAP length, will be computed later
    out_adu[6] = frame->uid;

    int out_adu_len = 7;
    memcpy(out_adu + out_adu_len, frame->pdu, frame->pdu_len);
    out_adu_len += frame->pdu_len;

    u16 mbap_len = (u16)(out_adu_len - 6);

    out_adu[4] = (mbap_len >> 8) & 0xFF;
    out_adu[5] = (mbap_len >> 0) & 0xFF;
    return out_adu_len;
}

int
build_adu(u8 *adu, frame_t *frame) {
    switch (frame->protocol) {
    case MB_PROTOCOL_RTU  : return build_adu_rtu(adu, frame);
    case MB_PROTOCOL_ASCII: return build_adu_ascii(adu, frame);
    case MB_PROTOCOL_TCP  : return build_adu_tcp(adu, frame);
    }

    return RC_FAIL;
}

int
check_req_rsp_pdu(u8 *req, u8 req_len, u8 *rsp, u8 rsp_len) {
    u16 qty    = 0;
    u8  nbytes = 0;
    u8  fcode  = req[0];

    // function codes must be equal
    if (req[0] != rsp[0]) {
        return 0;
    }

    switch (fcode) {
    case MB_FC_READ_COILS:
    case MB_FC_READ_DISCRETE_INPUTS:
        qty    = req[3] << 8 | req[4];
        nbytes = (qty + 7) / 8;

        if (rsp[1] != nbytes || rsp_len != nbytes + 2) {
            return 0;
        }

        break;
    case MB_FC_READ_HOLDING_REGISTERS:
    case MB_FC_READ_INPUT_REGISTERS:
        qty    = req[3] << 8 | req[4];
        nbytes = qty * 2;

        if (rsp[1] != nbytes || rsp_len != nbytes + 2) {
            return 0;
        }

        break;

    // for those functions normal rsp is echo of req
    case MB_FC_WRITE_SINGLE_COIL:
    case MB_FC_WRITE_SINGLE_REGISTER:
        if (memcmp(req, rsp, req_len) != 0) {
            return 0;
        }

        break;

    // normal rsp return function code, starting address and qaunity of outputs,
    // so first 5 bytes have to be equal
    case MB_FC_WRITE_MULTIPLE_COILS:
    case MB_FC_WRITE_MULTIPLE_REGISTERS:
        if (memcmp(req, rsp, 5) != 0) {
            return 0;
        }

        break;
    case MB_FC_WRITE_AND_READ_REGISTERS:
        qty    = req[3] << 8 | req[4];
        nbytes = qty * 2;

        if (rsp[1] != nbytes || rsp_len != nbytes + 2) {
            return 0;
        }

        break;
    }

    return 1;
}

// =============================================================================
// Extract frame from ADU
// =============================================================================

void
mb_rtu_extract_frame(u8 *adu, int adu_len, frame_t *out) {
    out->protocol = MB_PROTOCOL_RTU;
    out->uid      = adu[MB_RTU_HDR_LEN - 1];
    out->fc       = adu[MB_RTU_HDR_LEN];
    out->pdu_len  = adu_len - (MB_RTU_HDR_LEN + MB_RTU_CRC_LEN);
    memcpy(out->pdu, adu + MB_RTU_HDR_LEN, out->pdu_len);
}

void
mb_ascii_extract_frame(u8 *adu, int adu_len, frame_t *out) {
    out->protocol = MB_PROTOCOL_ASCII;
    out->uid      = hex_to_digit(adu[MB_ASCII_HDR_LEN - 2], adu[MB_ASCII_HDR_LEN - 1]);
    out->fc       = hex_to_digit(adu[MB_ASCII_HDR_LEN + 0], adu[MB_ASCII_HDR_LEN + 1]);
    out->pdu_len  = (adu_len - (MB_ASCII_HDR_LEN + MB_ASCII_CRC_LEN)) / 2;

    for (int i = 0; i < out->pdu_len; i++) {
        out->pdu[i] = hex_to_digit(adu[MB_ASCII_HDR_LEN + 2 * i + 0], adu[MB_ASCII_HDR_LEN + 2 * i + 1]);
    }
}

void
mb_tcp_extract_frame(u8 *adu, int adu_len, frame_t *out) {
    out->tid      = (adu[0] << 8) | (adu[1] << 0);
    out->protocol = MB_PROTOCOL_TCP;
    out->uid      = adu[MB_TCP_HDR_LEN - 1];
    out->fc       = adu[MB_TCP_HDR_LEN];
    out->pdu_len  = adu_len - MB_TCP_HDR_LEN;
    memcpy(out->pdu, adu + MB_TCP_HDR_LEN, out->pdu_len);
}

void
mb_extract_frame(mb_protocol_t proto, u8 *adu, int adu_len, frame_t *out) {
    switch (proto) {
    case MB_PROTOCOL_RTU  : mb_rtu_extract_frame(adu, adu_len, out); break;
    case MB_PROTOCOL_ASCII: mb_ascii_extract_frame(adu, adu_len, out); break;
    case MB_PROTOCOL_TCP  : mb_tcp_extract_frame(adu, adu_len, out); break;
    }
}

// =============================================================================
// Validate ADU
// =============================================================================

mb_validation_err_t
mb_rtu_is_adu_valid(u8 *adu, int len) {
    if (len < MB_RTU_MIN_ADU_LEN) {
        return MB_VALIDATION_ERROR_SHORT;
    }

    u16 calculated = crc16(adu, len - MB_RTU_CRC_LEN);
    // little-endian
    u8  lo         = adu[len - MB_RTU_CRC_LEN + 0];
    u8  hi         = adu[len - MB_RTU_CRC_LEN + 1];
    u16 expected   = (hi << 8) | (lo << 0);

    if (calculated != expected) {
        return MB_VALIDATION_ERROR_BAD_CRC;
    }

    return MB_VALIDATION_ERROR_OK;
}

mb_validation_err_t
mb_ascii_is_adu_valid(u8 *adu, int len) {
    if (len < MB_ASCII_MIN_ADU_LEN) {
        return MB_VALIDATION_ERROR_SHORT;
    }

    if (adu[0] != ':' || adu[len - 2] != '\r' || adu[len - 1] != '\n') {
        // invalid start character
        return MB_VALIDATION_ERROR_INVALID_DATA;
    }

    int hex_bytes = len - 1 /*:*/ - 2 /*LRC hex*/ - 2 /*CRLF*/;
    if (hex_bytes < 2 || (hex_bytes % 2) != 0) {
        return MB_VALIDATION_ERROR_BAD_LEN;
    }

    // decode payload to binary (Unit ID + Function Code + Data)
    int bin_len = hex_bytes / 2;
    u8  bin[1 + 1 + MB_MAX_PDU_LEN];

    for (int i = 0; i < bin_len; ++i) {
        int val = hex_to_digit(adu[1 + 2 * i + 0], adu[1 + 2 * i + 1]);
        if (val < 0) {
            return MB_VALIDATION_ERROR_INVALID_DATA;
        }
        bin[i] = (u8)val;
    }

    int lrc_val = hex_to_digit(adu[1 + hex_bytes + 0], adu[1 + hex_bytes + 1]);
    if (lrc_val < 0) {
        return MB_VALIDATION_ERROR_INVALID_DATA;
    }

    u8 got      = (u8)lrc_val;
    u8 expected = lrc8(bin, bin_len);
    if (got != expected) {
        return MB_VALIDATION_ERROR_BAD_LRC;
    }
    return MB_VALIDATION_ERROR_OK;
}

mb_validation_err_t
mb_tcp_is_adu_valid(u8 *adu, int len) {
    if (len < MB_TCP_HDR_LEN) {
        return MB_VALIDATION_ERROR_SHORT;
    }

    u16 pid = adu[2] << 8 | adu[3];
    if (pid != 0x0000) {
        // invalid protocol ID
        return MB_VALIDATION_ERROR_BAD_PROTO_ID;
    }

    // MBAP length = length of (unit id + data)
    u16 mbap_len = adu[4] << 8 | adu[5];
    if (mbap_len != len - (MB_TCP_HDR_LEN - 1)) {
        // invalid MBAP length
        return MB_VALIDATION_ERROR_BAD_LEN;
    }

    if (mbap_len < 1 + MB_TCP_MIN_PDU_LEN) {
        return MB_VALIDATION_ERROR_INVALID_DATA;
    }

    return MB_VALIDATION_ERROR_OK;
}

mb_validation_err_t
mb_is_adu_valid(mb_protocol_t proto, u8 *adu, int adu_len) {
    switch (proto) {
    case MB_PROTOCOL_RTU  : return mb_rtu_is_adu_valid(adu, adu_len);
    case MB_PROTOCOL_ASCII: return mb_ascii_is_adu_valid(adu, adu_len);
    case MB_PROTOCOL_TCP  : return mb_tcp_is_adu_valid(adu, adu_len);
    }
    return 0;
}
