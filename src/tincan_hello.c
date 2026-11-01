#include "tincan_hello.h"

int tincan_hello_encode(uint16_t max_msg, uint8_t *out, size_t out_cap) {
    if (out_cap < TINCAN_HELLO_LEN) {
        return -1;
    }
    out[0] = TINCAN_PROTO_VERSION;
    out[1] = 0; // flags: reserved
    out[2] = (uint8_t)(max_msg & 0xff);
    out[3] = (uint8_t)(max_msg >> 8);
    return TINCAN_HELLO_LEN;
}

int tincan_hello_decode(const uint8_t *payload, size_t len, struct tincan_hello *hello) {
    if (len != TINCAN_HELLO_LEN) {
        return -1;
    }
    hello->version = payload[0];
    hello->flags = payload[1];
    hello->max_msg = (uint16_t)(payload[2] | (payload[3] << 8));
    return 0;
}
