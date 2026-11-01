# Feature: Framing

To enable more interesting use cases on `zmk-tincan` it needs to support
larger messager than a single ATT packet. This feature introduces a lean
implementation of _frames_ to support that.

## Concept

### Principle: transport only

`zmk-tincan` moves opaque messages and never inspects or parses payload.
There is no shared envelope, so every user can freely define their own
protocols.

### Wire format

KISS - Header byte per ATT packet:

```text
bit 7    bit 6..0
More?    reserved
[hdr u8][payload ...]       # payload <= ATT_MTU - 3 - 1
```

- bit 7 set: next packet carries more of the same message
- no checksum, no sequence number, no length field

### Why no checksum, sequence numbers, retries?

BLE link layer covers this already; users are free to implement
additional measures or validations downstream.

### Testing

The chunk-split/reassembly logic lives in `src/tincan_frame.c`/`.h`, kept
free of Zephyr/BT headers specifically so it can be tested in isolation
with a plain host compiler — no `west` build needed (same reasoning that
deferred `clang-tidy` in `docs/00_linting.md`). `tests/test_tincan_frame.c`
uses `theft` (C property-based testing) to check the round-trip law
(split then reassemble reconstructs the original message) across randomized
messages and chunk sizes, plus a few explicit edge cases (empty message,
reassembly overflow, writer out-of-space). Wired into `nix flake check` as
the `frame-test` check, and required on PRs via a dedicated `test` CI job.

### Resolution

`tincan_gatt.c` and `tincan_central.c` each wire the shared frame module
into their existing send/receive paths — no new queue/workqueue, reusing
the same callback-driven send loop central already had and a plain
synchronous loop on the peripheral's notify path. One accepted limitation
without a sequence number: if a sender's write/notify fails partway
through a multi-fragment message, the receiver's reassembly buffer is left
waiting for a fragment that never arrives, until the next message or a
disconnect resets it — a direct consequence of the "no checksum, no
sequence number" choice above, not something this feature works around.
