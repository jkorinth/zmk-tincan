# Feature: Channels

In addition to [framing](framing.md), more complex use cases of `zmk-tincan`
require that multiple downstream users can exist in parallel. This is not
possible at the moment and could be realised in two different ways:

  1. Shared envelope - if all users share the same message protocol, no
     changes are required for `zmk-tincan`; but this requires sharing the
     protobuf defs somehow.
  2. Channels - since _framing_ already added a 1B header to the wire
     format, we can use additional bits to select a _channel_.

We want to implement 2. in this feature.

## Concept

### Extension of framing wire format

The framing header has reserved bits that we can use:

```
bit 7   bit 6..4               3..0
More?   Channel index (0-7)    reserved
[hdr u8][payload ...]
```

In theory we could support up to 128 channels, but given the BLE
throughput constraints, it's unlikely that more than 5-7 users can
exist in the same app (also consider space constraints on typical
targets).

### Protocol

Current idea is to use channel 0 as _control channel_: When a link
comes up, each side sends a `HELLO` packet on channel 0:

```
[ver u8 = 1][flags u8][max_msg u16]
```

This allows to queue messages until the peer's `HELLO` arrives,
and also detect protocol version mismatches.

### Listeners

Current single callback needs to be replaced by a chain or list of
callbacks. Most zephyr-like approach: `TINCAN_LISTENER` macros.

### Implementation notes

- tx queue: ring buffer, ~1-2KiB? (configurable)
- rx queue: one buffer per peer, 256B per peer (configurable)
- listeners: stored in flash
- downstream users: need to pick their channel; global config must
  somehow guarantee that they don't collide? TODO!


