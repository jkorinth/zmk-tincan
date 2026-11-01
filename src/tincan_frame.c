#include "tincan_frame.h"
#include <string.h>

int tincan_frame_write_chunk(const uint8_t *msg, size_t msg_len, size_t *offset,
                             size_t max_chunk_payload, uint8_t *out, size_t out_cap,
                             bool *more_out) {
    size_t remaining = msg_len - *offset;
    size_t chunk = remaining < max_chunk_payload ? remaining : max_chunk_payload;
    bool more = chunk < remaining;

    if (out_cap < TINCAN_FRAME_HDR_LEN + chunk) {
        return -1;
    }

    out[0] = more ? TINCAN_FRAME_MORE_FLAG : 0;
    memcpy(out + TINCAN_FRAME_HDR_LEN, msg + *offset, chunk);
    *offset += chunk;
    *more_out = more;
    return (int)(TINCAN_FRAME_HDR_LEN + chunk);
}

int tincan_frame_reassemble(const uint8_t *frame, uint16_t frame_len, uint8_t *asm_buf,
                            size_t asm_cap, size_t *asm_len) {
    if (frame_len < TINCAN_FRAME_HDR_LEN) {
        *asm_len = 0;
        return -2;
    }

    bool more = frame[0] & TINCAN_FRAME_MORE_FLAG;
    size_t payload_len = frame_len - TINCAN_FRAME_HDR_LEN;

    if (*asm_len + payload_len > asm_cap) {
        *asm_len = 0;
        return -2;
    }

    memcpy(asm_buf + *asm_len, frame + TINCAN_FRAME_HDR_LEN, payload_len);
    *asm_len += payload_len;

    if (more) {
        return -1;
    }

    int complete_len = (int)*asm_len;
    *asm_len = 0;
    return complete_len;
}
