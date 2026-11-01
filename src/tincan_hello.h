#pragma once
#include <stddef.h>
#include <stdint.h>

#define TINCAN_PROTO_VERSION 1u
#define TINCAN_HELLO_LEN 4 // ver(1) + flags(1) + max_msg(2), little-endian

struct tincan_hello {
    uint8_t version;
    uint8_t flags;
    uint16_t max_msg;
};

// Encodes this side's HELLO payload into `out` (capacity `out_cap`).
// Returns TINCAN_HELLO_LEN, or -1 if out_cap is too small.
int tincan_hello_encode(uint16_t max_msg, uint8_t *out, size_t out_cap);

// Decodes a received HELLO payload into `*hello`. Returns 0 on success,
// or -1 if `len` isn't exactly TINCAN_HELLO_LEN.
int tincan_hello_decode(const uint8_t *payload, size_t len, struct tincan_hello *hello);
