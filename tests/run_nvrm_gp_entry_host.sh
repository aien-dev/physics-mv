#!/bin/bash
# tests/run_nvrm_gp_entry_host.sh -- host-only checks of nvrm_gp_entry() and the 40-bit
# pushbuffer address limit in nvrm_enqueue(). No GPU, no driver calls, no GPU lock.
# Exit code 0 only when every check passes.
set -uo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd -P)
cd "$REPO_ROOT" || exit 1

BUILD_DIR="$REPO_ROOT/build"
BIN="$BUILD_DIR/test_nvrm_gp_entry_host"
mkdir -p "$BUILD_DIR"

echo "=== NVRM GP entry host gates ==="
if ! gcc -O2 -Wall -Wextra -Werror \
  -I./nvrm -I./m16 \
  -I./third_party/nvidia-open-580.173.02/src/common/sdk/nvidia/inc \
  -I./third_party/nvidia-open-580.173.02/kernel-open/common/inc \
  -I./third_party/nvidia-open-580.173.02/kernel-open/nvidia-uvm \
  -I./third_party/nvidia-open-580.173.02/src/nvidia/arch/nvalloc/unix/include \
  nvrm/nvrm.c tests/test_nvrm_gp_entry_host.c -o "$BIN"; then
    echo "FAIL build test_nvrm_gp_entry_host"
    exit 1
fi
"$BIN"
