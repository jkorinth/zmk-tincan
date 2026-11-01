#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TINCAN_FRAME_HDR_LEN 1
#define TINCAN_FRAME_MORE_FLAG 0x80u
#define TINCAN_FRAME_MAX_MSG_LEN 128

enum tincan_frame_write_res {
    TINCAN_FRAME_WRITE_ERROR = -1, // out_cap can't hold the next chunk
};

enum tincan_frame_reassemble_res {
    TINCAN_FRAME_INCOMPLETE = -1, // more fragments still expected
    TINCAN_FRAME_ERROR = -2,      // missing header byte, or would overflow asm_cap
};

// Writes the next header+payload chunk of `msg` (length `msg_len`) at
// `*offset` into `out` (capacity `out_cap`), advances `*offset`, and reports
// via `*more_out` whether another call is needed. Chunk payload is capped at
// `max_chunk_payload`. Returns the number of bytes written (header +
// payload, >= TINCAN_FRAME_HDR_LEN), or TINCAN_FRAME_WRITE_ERROR if
// `out_cap` can't hold the next chunk. Call in a do-while loop (keyed on
// `*more_out`) so a zero-length message still emits one (header-only)
// frame.
int tincan_frame_write_chunk(const uint8_t *msg, size_t msg_len, size_t *offset,
                             size_t max_chunk_payload, uint8_t *out, size_t out_cap,
                             bool *more_out);

// Feeds one received frame (header + payload) into the reassembly buffer
// `asm_buf` (capacity `asm_cap`), tracked across calls via `*asm_len`
// (caller-owned, persists between calls for the same peer). Returns
// TINCAN_FRAME_ERROR on error (missing header byte, or the payload would
// overflow `asm_cap` -- `*asm_len` is reset to 0 either way),
// TINCAN_FRAME_INCOMPLETE while more fragments are still expected, or the
// completed message length (>= 0, data left in `asm_buf`) once the final
// fragment (More flag clear) arrives -- also resetting `*asm_len` to 0 for
// the next message.
int tincan_frame_reassemble(const uint8_t *frame, uint16_t frame_len, uint8_t *asm_buf,
                            size_t asm_cap, size_t *asm_len);
