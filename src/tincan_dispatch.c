#include "tincan_dispatch.h"
#include <errno.h>

static tincan_cb_t listeners[TINCAN_CHANNEL_MAX + 1];

int tincan_listen(uint8_t channel, tincan_cb_t cb) {
    if (channel > TINCAN_CHANNEL_MAX) {
        return -EINVAL;
    }
    listeners[channel] = cb;
    return 0;
}

void tincan_dispatch(uint8_t channel, const void *payload, size_t length) {
    if (channel <= TINCAN_CHANNEL_MAX && listeners[channel]) {
        listeners[channel](payload, length);
    }
}
