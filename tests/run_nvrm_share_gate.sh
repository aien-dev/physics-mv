#!/bin/sh
# Shared-upload gate (L383): two processes, one GPU allocation. Builds nvrm/share_test and runs it.
# Memory only (no kernel launch): needs /dev/nvidia*, ~1 GiB free, no chip lock. Exit 0 = every check PASS.
set -e
cd "$(dirname "$0")/.."
gcc -O2 -Wall -Wextra -Werror \
  -I./nvrm -I./m16 \
  -I./third_party/nvidia-open-580.173.02/src/common/sdk/nvidia/inc \
  -I./third_party/nvidia-open-580.173.02/kernel-open/common/inc \
  -I./third_party/nvidia-open-580.173.02/kernel-open/nvidia-uvm \
  -I./third_party/nvidia-open-580.173.02/src/nvidia/arch/nvalloc/unix/include \
  nvrm/nvrm.c nvrm/share_test.c -o nvrm/share_test
exec timeout "${SHARE_GATE_TIMEOUT:-600}" ./nvrm/share_test "${1:-1073741824}"
