#pragma once
#include <zephyr/bluetooth/uuid.h>

#define BT_UUID_TINCAN_SERVICE_VAL                                                                 \
    BT_UUID_128_ENCODE(0x0d7b344c, 0x9979, 0x11f1, 0xad10, 0xb89a2a6e9420)

#define BT_UUID_TINCAN_CHAR_VAL                                                                    \
    BT_UUID_128_ENCODE(0x17bd9238, 0x9979, 0x11f1, 0x91c1, 0xb89a2a6e9420)

#define BT_UUID_TINCAN_SERVICE BT_UUID_DECLARE_128(BT_UUID_TINCAN_SERVICE_VAL)
#define BT_UUID_TINCAN_CHAR BT_UUID_DECLARE_128(BT_UUID_TINCAN_CHAR_VAL)
