# Feature: Encryption

`zmk-tincan`'s first goal was to reach PoC stage and actually work
on target hardware. Now that additional use cases are considered
(in particular: over-the-air updates), it needs to move to encrypted
transfers via `BT_GATT_PERM_WRITE_ENCRYPT`.

## Simulation approach

Testing encrypted transfers on real hardware for every change is slow.
`native_sim`/`native_posix` alone doesn't help here — it runs a single
Zephyr instance, so there's no second peer to pair/encrypt with.

`docs/06_simulation-testing.md` builds the BabbleSim harness this feature
needs — two simulated peers exchanging real BLE traffic, including
pairing/encryption — ahead of time, so the permission-check logic in
`tincan_gatt.c` (unreachable from the existing host-side `theft` tests,
which only cover the Zephyr-free `tincan_frame.c`/`tincan_dispatch.c`/
`tincan_hello.c`) has somewhere to run other than real hardware.

## Resolution

- [x] Implement `BT_GATT_PERM_WRITE_ENCRYPT` on the relevant
      characteristic(s) in `tincan_gatt.c`
- [x] Verify via the harness from `docs/06_simulation-testing.md`: write
      rejected before pairing, accepted after

Scoped to just the tincan characteristic's write permission, not its CCC —
encrypting the CCC too would also gate `bt_gatt_subscribe()` itself, which is
a separate concern (can the peer subscribe at all) from the one asked for
here (can the peer write data).

Verifying "rejected before pairing" needed a new test case
(`sim/encryption-rejected-before-pairing/`), not just confirming
`sim/queue-block-repro/` still passes. ZMK's own split characteristics
(`app/src/split/bluetooth/service.c`) already require encryption, so the
link reaches security level 2 within ~1.4s of connecting — ZMK's split
pairing runs regardless of tincan, and Zephyr's `BT_ATT_RETRY_ON_SEC_ERR`
(default `y`) transparently elevates security and retries any ATT request
that fails for insufficient encryption. The first attempt at racing tincan's
own discovery+write ahead of that 1.4s window (via a lowered discovery
delay) didn't actually demonstrate a rejection — the write just silently
waited for the retry-triggered security elevation and then succeeded,
indistinguishable in the log from an already-encrypted write. The real
lever is `CONFIG_BT_ATT_RETRY_ON_SEC_ERR=n` (set in the new test case's
`nrf52_bsim.conf`, same flag ZMK's own `app/run-ble-test.sh` uses for its
`no_auto_sec_central` build): with retries disabled, nothing ever triggers
pairing, so every write attempt fails for the lifetime of the test —
`sim/encryption-rejected-before-pairing/snapshot.log` shows the central's
writes consistently getting `tincan_on_response: write failed:  (5)`.

A side effect of building this test: `tincan_on_response` had a pre-existing
logging bug, unconditionally logging "write successful" even after logging
a write failure (missing an `else`) — harmless until a test needed to tell
the two apart in a snapshot. Fixed in `tincan_central.c` alongside this
feature.
