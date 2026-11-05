# Changelog

## [1.1.0](https://github.com/jkorinth/zmk-tincan/compare/v1.0.1...v1.1.0) (2026-11-05)


### Features

* **sim:** add BabbleSim test harness ([3f475ce](https://github.com/jkorinth/zmk-tincan/commit/3f475ce9c0400ce9f19ffecf926cf70babc52289))


### Bug Fixes

* **tincan:** stop storing compound-literal UUIDs in dis_params ([c68bb70](https://github.com/jkorinth/zmk-tincan/commit/c68bb70e25969e16182f73150028b76ad84abc2b))

## [1.0.1](https://github.com/jkorinth/zmk-tincan/compare/v1.0.0...v1.0.1) (2026-11-02)


### Bug Fixes

* **tincan:** replace undeclared BT_ATT_DEFAULT_LE_MTU with local constant ([356d319](https://github.com/jkorinth/zmk-tincan/commit/356d31900cb3eb719c83311af340e9d64804d654))

## 1.0.0 (2026-11-01)


### Features

* **ble:** basic scaffolding from peripheral_sc_only zephyr example ([f0cda53](https://github.com/jkorinth/zmk-tincan/commit/f0cda53da06946a5194cf4511fef6098fc3aac1f))
* **gatt:** WIP ([b8fa1da](https://github.com/jkorinth/zmk-tincan/commit/b8fa1da646db6b905757b683b195025c3234994f))
* **lint:** add nix-based CI linting for clang-format, markdownlint, nixfmt ([baaa486](https://github.com/jkorinth/zmk-tincan/commit/baaa4865c3a0f0429e002bbd09f38b1c1376b79d))
* **queue:** implement message queue for tincan ([de20d93](https://github.com/jkorinth/zmk-tincan/commit/de20d938b88462a65fb6a7e42ffc6ffc559d3d2a))
* **release:** add release-please automation and CONTRIBUTING.md ([0715ab3](https://github.com/jkorinth/zmk-tincan/commit/0715ab315e24c54f85c725120cfba10a4370de16))
* **tincan:** add ATT-packet framing with property-based test ([8db674f](https://github.com/jkorinth/zmk-tincan/commit/8db674fc8a8e4f6d85672d1ac755a6d599abaf3c))
* **tincan:** add channel routing and HELLO handshake ([096c596](https://github.com/jkorinth/zmk-tincan/commit/096c596ff6addddbc6bac01703e8c910923849d5))
* **tincan:** add symmetric speak/listen on both split roles ([3ae0346](https://github.com/jkorinth/zmk-tincan/commit/3ae0346d909c8e0f503c1888a6a6eb43f05490f0))


### Bug Fixes

* **lint:** exempt CHANGELOG.md from markdownlint ([19a989f](https://github.com/jkorinth/zmk-tincan/commit/19a989fcb5b026c6ea8b7779a081c7f7f2b52d5f))
