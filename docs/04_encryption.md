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

## Next steps

- [ ] Implement `BT_GATT_PERM_WRITE_ENCRYPT` on the relevant
      characteristic(s) in `tincan_gatt.c`
- [ ] Verify via the harness from `docs/06_simulation-testing.md`: write
      rejected before pairing, accepted after
