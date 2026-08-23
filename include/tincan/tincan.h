#pragma once
#include <stddef.h>
#include <stdint.h>

typedef void (*tincan_cb_t)(const void *payload, size_t length);

size_t tincan_mtu_size(void);
int tincan_speak(const void *payload, size_t length);
void tincan_listen(tincan_cb_t cb);
