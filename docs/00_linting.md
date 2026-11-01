# Feature: Linting

This feature should enable basic CI workflows for linting and
formatting, e.g., `clang-tidy`, `clang-format`. This should follow
upstreams `zmk` and `zephyr`. Additionally, `markdownlint` may be
useful.

TODO make a complete list of automated checks.

## Implementation notes

- CI and local _must_ share the same toolchain to get the exact
  same results!
- Could use `pre-commit` or `prek` with pinned `uv.lock` or so?
- Alternative/complement: nix flake + devShell

