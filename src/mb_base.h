#ifndef MB_BASE_H
#define MB_BASE_H

#include "client_cxt.h"
#include "types.h"

typedef enum {
    MB_VALIDATION_ERROR_OK           = 0,
    MB_VALIDATION_ERROR_SHORT        = -1,
    MB_VALIDATION_ERROR_BAD_CRC      = -2,
    MB_VALIDATION_ERROR_INVALID_DATA = -3,
    MB_VALIDATION_ERROR_BAD_LEN      = -4,
    MB_VALIDATION_ERROR_BAD_LRC      = -5,
    MB_VALIDATION_ERROR_BAD_PROTO_ID = -6,
} mb_validation_err_t;

typedef enum {
    MB_DIR_REQUEST  = 0, // master->slave (indication)
    MB_DIR_RESPONSE = 1, // slave->master (confirmation)
} mb_dir_t;

typedef struct func_cxt {
    fc_t fc;
    int  waddress;
    int  wcount;
    int  raddress;
    int  rcount;
} func_cxt_t;

typedef struct frame {
    mb_protocol_t protocol;
    fc_t          fc;

    u8  uid;
    u16 tid;
    u8  pdu_len;
    u16 expected_rsp_adu_len;

    u8 pdu[MB_MAX_PDU_LEN];
} frame_t;

void                bit_data_to_bytes(u8 *data, int data_len, u8 *out);
int                 build_pdu(u8 pdu[MB_MAX_PDU_LEN], u8 *data, func_cxt_t fdata);
int                 build_adu(u8 *adu, frame_t *frame);
int                 mb_get_expected_adu_len(mb_protocol_t proto, u8 *adu, int adu_len, mb_dir_t dir);
int                 client_get_expected_rsp_adu_len(mb_protocol_t protocol, func_cxt_t *fcxt);
void                mb_extract_frame(mb_protocol_t proto, u8 *adu, int adu_len, frame_t *out);
mb_validation_err_t mb_is_adu_valid(mb_protocol_t proto, u8 *adu, int adu_len);
int                 check_req_rsp_pdu(u8 *req, u8 req_len, u8 *rsp, u8 rsp_len);
char                nibble_to_hex(u8 d);
const char         *str_protocol(mb_protocol_t protocol);
void                str_curr_endpoint(char out[32], global_t *global);
const char         *str_fc(fc_t fc);
const char         *str_valid_err(mb_validation_err_t err);

#endif
