# Feature: Bi-directional communication

Currently central only implements `tincan_speak`, and peripheral only implements
`tincan_listen`. This is often awkward when bi-directional communication would be
more natural than polling. This feature aims to implement support for full
implementation on both sides.

## Implementation notes

  1. _Don't incur overhead, or keep it minimal._
     `zmk-tincan` does not add valuable features on its own, so keep scarce
     resources available for the code that does; optimize away at compilation if
     unused.
  2. Make sure the communication patterns remain clear for users.
     Check if this introduces N ways to achieve the same thing - if it does, come
     up with a solution that makes the module easy to use downstream.

## Resolution

The underlying GATT characteristic already carries both directions
(`WRITE` + `NOTIFY`); only the glue code was one-sided. Both `tincan_gatt.c`
and `tincan_central.c` now fully implement the single `tincan.h` API
(`tincan_speak`/`tincan_listen`/`tincan_mtu_size`) instead of each
exposing half of it under a different name:

- Peripheral's old `tincan_send(conn, data, len)` (never even declared in
  a header) became `tincan_speak(payload, length)`, using a newly tracked
  `_tincan.conn` (set/cleared via `BT_CONN_CB_DEFINE`, same pattern
  central already used) instead of a parameter.
- Central gained `tincan_listen()`; `tincan_on_notify()` now forwards to
  the registered callback instead of discarding the data.

No queue or workqueue was added for peripheral's send path — it stays a
plain synchronous call, so there's no new overhead or Kconfig toggle for
either direction.
