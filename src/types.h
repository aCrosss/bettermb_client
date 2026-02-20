#ifndef TYPES_H
#define TYPES_H

#include <time.h>

// -------- macroses --------------------------------------------------------------------

#define s8  char
#define s16 short int
#define s32 int
#define s64 long

#define u8  unsigned char
#define u16 unsigned short int
#define u32 unsigned int
#define u64 unsigned long

#define MAX_R_FIREED 999999 // how much requset we can order to fire in sequence

// -------- macroses --------------------------------------------------------------------

#define FLAG(n)          (1 << (n))
#define HI_NIBBLE(fbyte) ((fbyte >> 8) & 0xFF)
#define LO_NIBBLE(fbyte) (fbyte & 0xFF)

#define MAX_VAL(a, b)      (((a) > (b)) ? (a) : (b))
#define MIN_VAL(a, b)      (((a) < (b)) ? (a) : (b))
#define CLAMP(v, min, max) (((v) > (max)) ? (max) : (((v) < (min)) ? (min) : (v)))

// ind++ and clamp it between min and max
#define PIND_CLAMP(val, min, max) ((((val + 1) % (max + 1)) == 0) ? (val = min) : (val++))

// -------- typedefs --------------------------------------------------------------------

typedef enum opt_arg_req {
    OPT_ARG_NONE,
    OPT_ARG_REQUIRED,
    OPT_ARG_OPTIONAL,
} opt_arg_req_t;

typedef enum {
    RC_ERROR = -1,
    RC_FAIL,
    RC_SUCCESS,
} rc_t;

enum {
    REQ_SUCCEDED,
    REQ_FAILED,
    REQ_TIMED_OUT,
};

typedef struct ip_addr {
    union {
        u32 val_be; // big endian
        u8  data[4];
    };
} ip_addr_t;

// -------- modbus ----------------------------------------------------------------------

#define MB_MAX_PDU_LEN 253
#define MB_MIN_PDU_LEN 2 // function code + exception code

#define MB_RTU_HDR_LEN          1 // Unit ID (1)
#define MB_RTU_CRC_LEN          2 // CRC16 (2)
#define MB_RTU_MIN_PDU_LEN      (MB_MIN_PDU_LEN)
#define MB_RTU_MAX_PDU_LEN      (MB_MAX_PDU_LEN)
#define MB_RTU_MIN_ADU_LEN      (MB_RTU_HDR_LEN + MB_RTU_MIN_PDU_LEN + MB_RTU_CRC_LEN)
#define MB_RTU_MAX_ADU_LEN      (MB_RTU_HDR_LEN + MB_RTU_MAX_PDU_LEN + MB_RTU_CRC_LEN)
#define MB_RTU_ADU_LEN(pdu_len) (MB_RTU_HDR_LEN + (pdu_len) + MB_RTU_CRC_LEN)

#define MB_ASCII_HDR_LEN          3 // ':' (1) + Unit ID (2)
#define MB_ASCII_CRC_LEN          4 // LRC8 (2) + '\r' (1) + '\n' (1)
#define MB_ASCII_MIN_PDU_LEN      (2 * MB_MIN_PDU_LEN)
#define MB_ASCII_MAX_PDU_LEN      (2 * MB_MAX_PDU_LEN)
#define MB_ASCII_MIN_ADU_LEN      (MB_ASCII_HDR_LEN + MB_ASCII_MIN_PDU_LEN + MB_ASCII_CRC_LEN)
#define MB_ASCII_MAX_ADU_LEN      (MB_ASCII_HDR_LEN + MB_ASCII_MAX_PDU_LEN + MB_ASCII_CRC_LEN)
#define MB_ASCII_ADU_LEN(pdu_len) (MB_ASCII_HDR_LEN + (2 * pdu_len) + MB_ASCII_CRC_LEN)

#define MB_TCP_HDR_LEN          7 // MBAP: Transaction ID (2) + Protocol ID (2) + Length (2) + Unit ID (1))
#define MB_TCP_MIN_PDU_LEN      (MB_MIN_PDU_LEN)
#define MB_TCP_MAX_PDU_LEN      (MB_MAX_PDU_LEN)
#define MB_TCP_MIN_ADU_LEN      (MB_TCP_HDR_LEN + MB_TCP_MIN_PDU_LEN)
#define MB_TCP_MAX_ADU_LEN      (MB_TCP_HDR_LEN + MB_TCP_MAX_PDU_LEN)
#define MB_TCP_ADU_LEN(pdu_len) (MB_TCP_HDR_LEN + (pdu_len))

#define MB_MAX_ADU_LEN (MB_ASCII_MAX_ADU_LEN)

#define MB_MAX_READ_BITS     2000
#define MB_MAX_WRITE_BITS    1968
#define MB_MAX_READ_REGS     125
#define MB_MAX_WRITE_REGS    123
#define MB_MAX_WR_WRITE_REGS 121
#define MB_MAX_WR_READ_REGS  125

#define FCF_WRITE FLAG(0) /*if this comand write data*/
#define FCF_READ  FLAG(1) /*if this comand read data*/
#define FCF_BITS  FLAG(2) /*if this comand operates single bits*/

typedef enum {
    MB_FC_READ_COILS               = 0x01, // 01
    MB_FC_READ_DISCRETE_INPUTS     = 0x02, // 02
    MB_FC_READ_HOLDING_REGISTERS   = 0x03, // 03
    MB_FC_READ_INPUT_REGISTERS     = 0x04, // 04
    MB_FC_WRITE_SINGLE_COIL        = 0x05, // 05
    MB_FC_WRITE_SINGLE_REGISTER    = 0x06, // 06
    MB_FC_WRITE_MULTIPLE_COILS     = 0x0F, // 15
    MB_FC_WRITE_MULTIPLE_REGISTERS = 0x10, // 16
    MB_FC_WRITE_AND_READ_REGISTERS = 0x17, // 23
} fc_t;

typedef enum {
    MB_PROTOCOL_RTU,
    MB_PROTOCOL_ASCII,
    MB_PROTOCOL_TCP,
    MB_PROTOCOL_MAX,
} mb_protocol_t;

// Exception codes
typedef enum mb_ex {
    MB_EX_ILLEGAL_FUNCTION        = 0x01,
    MB_EX_ILLEGAL_DATA_ADDRESS    = 0x02,
    MB_EX_ILLEGAL_DATA_VALUE      = 0x03,
    MB_EX_SLAVE_OR_SERVER_FAILURE = 0x04,
    MB_EX_ACKNOWLEDGE             = 0x05,
    MB_EX_SLAVE_OR_SERVER_BUSY    = 0x06,
    MB_EX_MEMORY_PARITY           = 0x08,
    MB_EX_GATEWAY_PATH            = 0x0A,
    MB_EX_GATEWAY_TARGET          = 0x0B,
    MB_EX_MAX
} mb_ex_t;

#endif
