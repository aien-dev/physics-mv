#!/bin/sh
# L343 G5-physics-train-1 receipt: observe latency and CPU cost per wait, old primitive vs new.
# usage: bench.sh OLD_M16_NATIVE_C   (the pre-change m16/m16_native.c, e.g. git show 3e29cf0:m16/m16_native.c)
set -e
here=$(cd "$(dirname "$0")/../.." && pwd); old=$1; out=$(mktemp -d)
cc -O2 -DM16_TEST_NO_SPIN_API -I "$here/m16" -I "$here/nvrm" "$here/tests/test_m16_native_wait.c" "$old" -lpthread -Wl,--unresolved-symbols=ignore-all -o "$out/old"
cc -O2 -I "$here/m16" -I "$here/nvrm" "$here/tests/test_m16_native_wait.c" "$here/m16/m16_native.c" -lpthread -Wl,--unresolved-symbols=ignore-all -o "$out/new"
echo "host $(hostname) $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg)"
for d in 100 300 1000 1900 5000; do
  for b in old new; do printf '%s ' "$b"; "$out/$b" 400 "$d" print | grep BENCH; done
done
