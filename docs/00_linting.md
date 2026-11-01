# Feature: Linting

This feature should enable basic CI workflows for linting and
formatting, e.g., `clang-tidy`, `clang-format`. This should follow
upstreams `zmk` and `zephyr`. Additionally, `markdownlint` may be
useful.

TODO make a complete list of automated checks.

## Implementation notes

- CI and local _must_ share the same toolchain to get the exact
  same results!
- Implemented via a nix flake (`flake.nix`): a `devShell` for local use
  and `checks` (`clang-format`, `markdownlint`, `nixfmt`) that both
  `nix develop` and CI (`.github/workflows/lint.yml`, via
  `nix flake check`) run identically.
- `clang-tidy` is deferred: this module's sources only compile against
  real Zephyr headers and Kconfig symbols, so a real
  `compile_commands.json` requires a full `west build` of a ZMK firmware
  pulling in this module. That's a separate follow-up (west manifest +
  board/shield + build step in CI).
