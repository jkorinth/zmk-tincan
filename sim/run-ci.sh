#!/usr/bin/env bash
set -euo pipefail

# Orchestrates sim/run-bsim-test.sh in CI: stands up the west workspace
# (real zmk + zephyr + the babblesim west group), builds BabbleSim, wires
# zmk-tincan in via ZMK_EXTRA_MODULES, and runs the test case(s) given on
# the command line (defaults to sim/queue-block-repro).
#
# Run inside the `sim` devShell (`nix develop .#sim`), which provides
# west/cmake/ninja/dtc/gperf/ccache/git and exports CC32/CC64. See
# docs/06_simulation-testing.md for why each of these pieces is needed.
#
# WORKSPACE_DIR (default: a sibling of this repo) is reused across runs —
# pass a persistent, cached path in CI to avoid re-fetching/rebuilding
# zephyr/zmk/BabbleSim every time.

# Pinned for reproducibility; bump deliberately (re-verify the bsim harness
# still builds/runs against the new pin before updating).
ZMK_REV=4dd95d7840eb3d5a3f59bb29fbf1ab054715ef85

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
workspace_dir=${WORKSPACE_DIR:-"${repo_root}/../zmk-tincan-sim-workspace"}
bsim_out="${workspace_dir}/bsim-out"

if [ -z "${CC32:-}" ] || [ -z "${CC64:-}" ]; then
    echo "CC32/CC64 must be set (run inside 'nix develop .#sim')." >&2
    exit 1
fi

# avoids a find_package(Zephyr) registry gotcha; see docs/06_simulation-testing.md
export CMAKE_PREFIX_PATH="${workspace_dir}/zephyr/share/zephyr-package/cmake"

# nrf52_bsim is native; skip the ARM SDK lookup Zephyr defaults to
export ZEPHYR_TOOLCHAIN_VARIANT=host

mkdir -p "${workspace_dir}"
cd "${workspace_dir}"

# marker avoids silently reusing a cache from a failed `west update`;
# see docs/06_simulation-testing.md
west_update_marker="${workspace_dir}/.west-update-ok"
if [ ! -f "${west_update_marker}" ]; then
    if [ ! -d .west ]; then
        # `west init --mr` can't take a commit SHA; clone the default
        # branch, pin to ZMK_REV below
        west init -m https://github.com/zmkfirmware/zmk --mf app/west.yml .
    fi
    git -C zmk fetch origin "${ZMK_REV}"
    git -C zmk checkout "${ZMK_REV}"
    west config manifest.group-filter +babblesim
    west update
    touch "${west_update_marker}"
fi

zmk_app="${workspace_dir}/zmk/app"
bsim_components="${workspace_dir}/tools/bsim/components"

# patches in a CC32 for the 32-bit half of BabbleSim's libs; idempotent.
# see docs/06_simulation-testing.md
if ! grep -qF 'CC32 ?= ${CC}' "${bsim_components}/common/make.lib_soeta64et32.inc"; then
    sed -i '1a CC32 ?= ${CC}' \
        "${bsim_components}/common/make.lib_soeta64et32.inc" \
        "${bsim_components}/common/make.lib_a64et32.inc"
    sed -i 's/@\${CC} \(.*\) -m32 -c \$< -o \$@/@${CC32} \1 -m32 -c $< -o $@/' \
        "${bsim_components}/common/make.lib_soeta64et32.inc" \
        "${bsim_components}/common/make.lib_a64et32.inc"
fi

# same fix for ext_libCryptov1's own Makefile; needs `override` to beat
# the command-line CC= below. see docs/06_simulation-testing.md
if ! grep -qF 'override CC:=${CC32}' "${bsim_components}/ext_libCryptov1/Makefile"; then
    sed -i 's/^ARCH:=-m32$/ARCH:=-m32\noverride CC:=${CC32}/' \
        "${bsim_components}/ext_libCryptov1/Makefile"
fi

if [ ! -x "${bsim_out}/bin/bs_2G4_phy_v1" ]; then
    BSIM_OUT_PATH="${bsim_out}" make -C tools/bsim everything CC="${CC64}" CC32="${CC32}" -j"$(nproc)"
fi

cp "${repo_root}/sim/run-bsim-test.sh" "${zmk_app}/run-bsim-test.sh"
chmod +x "${zmk_app}/run-bsim-test.sh"
mkdir -p "${zmk_app}/tests/ble"

# force a clean configure; CMake won't reconsider CC on a cached build
rm -rf "${zmk_app}/build"

testcases=${*:-sim/queue-block-repro}
status=0
for testcase_src in ${testcases}; do
    name=$(basename "${testcase_src}")
    ln -sfn "${repo_root}/${testcase_src}" "${zmk_app}/tests/ble/${name}"

    (
        cd "${zmk_app}"
        # CC32 is first on PATH (flake.nix); nothing to pass explicitly here
        BSIM_OUT_PATH="${bsim_out}" \
            BSIM_COMPONENTS_PATH="${bsim_components}" \
            BLE_TESTS_NO_CENTRAL_BUILD=y \
            ZMK_EXTRA_MODULES="${repo_root}" \
            TINCAN_EXTRA_CFLAGS=-DZMK_TINCAN_DEBUG_SEND \
            bash run-bsim-test.sh "tests/ble/${name}"
    ) || status=1
done

exit "${status}"
