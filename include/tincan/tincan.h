#pragma once
#include <stddef.h>
#include <stdint.h>

typedef void (*tincan_cb_t)(const void *payload, size_t length);

#define TINCAN_CHANNEL_CONTROL 0 // reserved for the HELLO handshake, never dispatched to a listener
#define TINCAN_CHANNEL_MAX 7

size_t tincan_mtu_size(void);
int tincan_speak(uint8_t channel, const void *payload, size_t length);
int tincan_listen(uint8_t channel, tincan_cb_t cb);
