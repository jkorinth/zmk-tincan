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

```text
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

```text
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

## Resolution

- **Single shared buffer, not per-channel.** TX stays serialized across
  channels (central's existing single-flight queue+work-handler;
  peripheral's `tincan_speak` is already one synchronous call), so the wire
  never interleaves two messages from different channels. RX only needs
  one reassembly buffer per role, tagged with whichever channel is
  currently mid-message — not one per channel. Matches the "one buffer
  per peer" sizing above (we only ever have one peer); no ring buffer was
  introduced, the existing queue/buffers just gained a channel tag.
- **1:1 instead of `TINCAN_LISTENER` fan-out.** `tincan_listen(channel, cb)`
  stores one callback in a fixed 8-slot array (`src/tincan_dispatch.c`),
  multiplying the original single-callback design via channels rather than
  the sketched iterable-section macros. This is simpler, and it dissolves
  the collision `TODO` above rather than merely deferring it: registering
  on a channel is just an array-slot write, so a second registration on
  the same channel deterministically overwrites the first — there's no
  ambiguous multi-listener state to collide in the first place.
- **`HELLO` is implemented** (`src/tincan_hello.c`): version check, and
  `max_msg` is enforced on sends once the peer's `HELLO` is seen. On the
  central role (which has a tx queue), non-control messages sent before
  the peer's `HELLO` arrives are genuinely queued (peeked, not dequeued,
  until ready). The peripheral role has no tx queue (kept deliberately
  synchronous since feature 05) — its `tincan_speak` returns `-EAGAIN` for
  non-control channels until the peer is ready, and the caller retries.
- Channel-collision avoidance between downstream users otherwise remains
  open, as originally flagged — the array design above just makes the
  failure mode ("last registration wins") well-defined instead of
  undefined.
