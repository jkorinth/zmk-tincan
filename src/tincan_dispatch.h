#pragma once
#include <stddef.h>
#include <stdint.h>
#include <tincan/tincan.h>

// Registers `cb` for `channel` (0-7), overwriting whatever was registered
// there before. Returns -EINVAL for an out-of-range channel, else 0.
int tincan_listen(uint8_t channel, tincan_cb_t cb);

// Calls the callback registered for `channel`, if any (no-op otherwise).
void tincan_dispatch(uint8_t channel, const void *payload, size_t length);
