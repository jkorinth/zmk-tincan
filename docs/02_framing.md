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

### TODO Testing

So far, `zmk-tincan` was so small that unit tests were too much effort.
With this change I think I'll have to figure out how to test this in
isolation. Good learning opportunity.
