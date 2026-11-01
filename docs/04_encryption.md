# Feature: Encryption

`zmk-tincan`'s first goal was to reach PoC stage and actually work
on target hardware. Now that additional use cases are considered
(in particular: over-the-air updates), it needs to move to encrypted
transfers via `BT_GATT_PERM_WRITE_ENCRYPT`.

Since this can be challenging on real hardware, first step will be
to check if we can run a simulation somehow and reduce the effort
required for this.
