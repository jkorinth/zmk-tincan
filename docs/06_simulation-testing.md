# Feature: BabbleSim test harness

`docs/04_encryption.md` identified BabbleSim as the way to exercise real
BLE pairing/encryption without hardware. This feature stands up that harness
on its own, ahead of the encryption work, so it can also be used to
reproduce bugs in the existing central/peripheral link (first target: the
tx queue blocking up).

## Concept

`zmk-tincan` has no west/Zephyr workspace of its own today — it's consumed
as a module (`zephyr/module.yml`) inside a real ZMK build. Two of its
Kconfig touch points aren't even Zephyr's own: `select BLE` (`Kconfig`) and
`CONFIG_ZMK_SPLIT_ROLE_CENTRAL` (`CMakeLists.txt`) are ZMK symbols, not
Zephyr ones. Simulating it faithfully therefore means building inside a
real ZMK app, not re-deriving ZMK's BLE central-scan/peripheral-advertise
setup by hand.

### Toolchain

- `nrf52_bsim` is a stock Zephyr board: BabbleSim's simulated radio PHY
  plus Zephyr's own software BLE controller, compiled as a native Linux
  binary — no ARM SDK needed, host `gcc` is enough. Without
  `ZEPHYR_TOOLCHAIN_VARIANT=host` (`sim/run-ci.sh`), Zephyr's build
  defaults to looking for an ARM cross SDK it will never find here.
- `find_package(Zephyr)` (`zmk/app/CMakeLists.txt`) resolves via CMake's
  user package registry (`~/.cmake/packages/Zephyr/`) unless told
  otherwise — on a machine where some *other* Zephyr checkout was ever
  `west zephyr-export`-ed, it happily resolves to that unrelated one
  instead of this workspace's, and on a fresh machine/CI runner with no
  registry entry at all it just fails outright. `sim/run-ci.sh` sets
  `CMAKE_PREFIX_PATH` to this workspace's own
  `zephyr/share/zephyr-package/cmake` to sidestep the registry
  entirely. When verifying a change to this harness locally, hide
  `~/.cmake/packages/Zephyr*` first (`mv` it aside) — otherwise a local
  "PASS" can pass by accident on registry state that CI doesn't have.
- [`zephyr-nix`](https://github.com/nix-community/zephyr-nix) supplies
  `west` + its Python dependencies (`pythonEnv`) through Nix, the same
  approach `zmk-nix` uses to build real ZMK firmware without Docker.
- BabbleSim itself isn't part of `zephyr-nix` or nixpkgs; its component
  repos come in through `west update` (the `babblesim_*` projects in
  Zephyr's manifest) and build with a plain `make`, independent of
  west/CMake.
- ZMK already has exactly this working internally: `app/boards/nrf52_bsim.{conf,overlay}`,
  `app/run-ble-test.sh`, and a dedicated CI workflow (added 2023 by
  `petejohanson`/`caksoylar`), including `app/tests/ble/security/` —
  encryption-gated GATT access over a real simulated link, the scenario
  `docs/04_encryption.md` needs. It's undocumented/internal to ZMK, so
  this reuses their pattern rather than asking them to expose it.

### Reproducing the queue-block bug

`tincan_tx_work_handler` (`src/tincan_central.c`) drains its tx queue
strictly head-first: `k_msgq_peek` the head, and if it's a non-control
message queued before `tincan_peer_ready`, return without touching the
queue at all — including anything queued *behind* it. Since the central's
own `HELLO` is only enqueued once discovery/subscribe completes (4s after
`connected()`), an app-level `tincan_speak()` call fired early can land
ahead of `HELLO` in the queue and head-of-line-block it. It's designed to
self-heal once the peripheral's unprompted `HELLO` notify arrives and
decodes cleanly — the open question this harness exists to answer is
whether it reliably does, or whether the queue can fill to
`CONFIG_ZMK_TINCAN_TX_QUEUE_DEPTH` and start permanently dropping messages.

## Implementation notes

- West workspace (zmk + zephyr + modules, including the `babblesim_*`
  projects) lives outside this repo, at `WORKSPACE_DIR` (default: a
  sibling directory) — it's a build dependency, not something to
  vendor/commit. ZMK is pinned to a specific commit (`sim/run-ci.sh`'s
  `ZMK_REV`) for reproducibility; bump it deliberately. `west init --mr`
  only accepts a branch/tag (it's a plain `git clone --branch`), so
  pinning to an exact commit means cloning the default branch first, then
  `git fetch`+`checkout`-ing `ZMK_REV` separately. Setup (`west
  init`+`update`) is gated on a `.west-update-ok` marker file, not just
  `.west` existing — a `west update` that fails partway (network blip,
  runner timeout) would otherwise leave CI caching, and silently reusing,
  a half-updated workspace with no hint anything's wrong.
- `sim/run-bsim-test.sh` is ZMK's own `app/run-ble-test.sh`, copied in and
  patched to add `-DZMK_EXTRA_MODULES=<path to this repo>` (so
  `CONFIG_ZMK_TINCAN` is visible) and a `TINCAN_EXTRA_CFLAGS` escape hatch
  for build-time `#define`s like `ZMK_TINCAN_DEBUG_SEND` — ZMK's script
  has no reason to know about out-of-tree modules, so that's on us, not
  something to upstream.
- `sim/run-ci.sh` orchestrates the rest: stands up the west workspace
  (skipping `west update` entirely on a cache hit — see below), builds
  BabbleSim, symlinks a test case from `sim/<name>/` into
  `zmk/app/tests/ble/<name>/`, and runs it through `run-bsim-test.sh`.
- Test case layout follows ZMK's own `app/tests/ble/` convention:
  `nrf52_bsim.keymap`/`.conf` for the central side, plus a
  `peripheral.overlay` (its mere presence is what makes the script build
  *both* sides) and `siblings.txt` naming the peripheral executable to
  launch alongside it. Verification is log-based, same as ZMK's own
  tests: `events.patterns` (sed filter) + `snapshot.log` (expected
  filtered output), diffed by the script.
- Toolchain is a `sim` devShell (`flake.nix`), entered via
  `nix develop .#sim`. Two things made this fiddlier than the `default`
  shell:
  - `nrf52_bsim`'s native build is 32-bit, and so is half of BabbleSim's
    own libraries (the half linked into the firmware executable) —
    nixpkgs' `gcc` has no 32-bit multilib support, so
    `pkgsi686Linux.stdenv.cc` supplies it. Zephyr's own cmake
    (`cmake/compiler/host-gcc/generic.cmake`) does
    `find_program(CMAKE_C_COMPILER gcc REQUIRED)` — it ignores `$CC`
    entirely, so that 32-bit compiler has to be *first on `PATH`*, not
    just present. BabbleSim's plain Makefiles, by contrast, do respect
    `$CC`/`$CC32` — `run-ci.sh` passes both explicitly (`CC32`/`CC64`,
    exported as absolute paths by the devShell's `shellHook`) rather than
    relying on PATH order a second time. Most of BabbleSim's own
    Makefiles pick up `CC32` through a plain `CC32 ?= ${CC}` default
    (patched in by `run-ci.sh`); `ext_libCryptov1` needs `override
    CC:=${CC32}` instead, since a command-line `CC=` assignment (the
    top-level `make everything CC=${CC64}` call) always wins over a plain
    in-makefile `CC:=` — `override` is the only way to beat it.
  - The shell uses `zephyr-nix`'s own (older, Zephyr-verified) `nixpkgs`
    pin for every cmake-adjacent package, not this repo's own
    `nixpkgs` input (which tracks `nixos-unstable` and had already
    drifted to a `cmake` release Zephyr 4.1's scripts don't parse).
- CI (`bsim-test` job, `.github/workflows/lint.yml`) caches
  `WORKSPACE_DIR` keyed on `sim/run-ci.sh`'s contents, so bumping
  `ZMK_REV` (or changing the BabbleSim patch) invalidates the cache.
  `run-ci.sh` also force-cleans the Zephyr `build/` directory before
  every run — CMake caches `CMAKE_C_COMPILER` on first configure and
  won't reconsider it just because the toolchain changed underneath a
  cached workspace.
