# zmk-tincan - basic communication between estranged splits

ZMK/Zephyr module that implements a generic GATT server on the peripheral side
of a split keyboard to facilitate generic communication of nanoPB protocols.
Useful in cases where extending the ZMK split communication protocol is not
straightforward or matching the usecase.

## Usage

`zmk-tincan` only moves opaque bytes between the two split halves, tagged
with a channel (0-7, see `docs/03_channels.md`). It never inspects the
payload, so each channel can carry its own protobuf schema independent of
any other channel.

Nothing in `zmk-tincan` keeps two modules from picking the same channel.
Each module should expose its channel number as a Kconfig option (rather
than hardcoding it), so the final keyboard's app config is what actually
assigns channels and keeps modules from colliding:

```kconfig
config MY_MODULE_TINCAN_CHANNEL
    int "tincan channel for my_module"
    default 1
```

```c
#include <tincan/tincan.h>

static void my_module_on_message(const void *payload, size_t length) {
    MyProtoMsg msg;
    my_proto_msg_decode(payload, length, &msg);
    // ...
}

void my_module_init(void) {
    tincan_listen(CONFIG_MY_MODULE_TINCAN_CHANNEL, my_module_on_message);
}

void my_module_send(const MyProtoMsg *msg) {
    uint8_t buf[64];
    size_t len = my_proto_msg_encode(msg, buf, sizeof(buf));
    tincan_speak(CONFIG_MY_MODULE_TINCAN_CHANNEL, buf, len);
}
```
