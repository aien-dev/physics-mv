#!/bin/sh
# Offline M2 host-verifier tests; no GPU, QEMU, receipt or historical edits.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
T=$(mktemp -d "${TMPDIR:-/tmp}/physics-host-security.XXXXXXXX")
trap 'rm -rf "$T"' EXIT
trap 'exit 1' INT TERM
: "${CC:=gcc}"
"$CC" -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -std=c11 -Wall -Wextra -Werror \
  -o "$T/m2tool" "$ROOT/tools/m2tool.c" "$ROOT/sha256_clean.c"
export ASAN_OPTIONS=detect_leaks=0

audit() {
  "$T/m2tool" audit-verify "$ROOT/physics.bin" "$ROOT/physics.sha256" \
    "$ROOT/physics.manifest" "$ROOT/machine_contract.json" "$ROOT/physics.audit" "$1"
}
audit "$ROOT/physics.decode" > "$T/golden.log"
sed 's/ldr[[:space:]]x10, 40200280/ldr\tx10, fffffffffffffffc/' \
  "$ROOT/physics.decode" > "$T/bad.decode"
grep -q fffffffffffffffc "$T/bad.decode"
if audit "$T/bad.decode" > "$T/bad.log" 2>&1; then
  echo 'unexpected PASS: wrapping literal address' >&2; exit 1
fi
grep -q 'FAIL: SP literal' "$T/bad.log"
if grep -q AddressSanitizer "$T/bad.log"; then
  echo 'sanitizer abort: literal address' >&2; exit 1
fi

aarch64-linux-gnu-gcc -c -nostdlib "$ROOT/physics.s" -o "$T/physics.o"
"$T/m2tool" get-symbol "$T/physics.o" _physics_entry > "$T/symbol.log"
grep -q '^0x' "$T/symbol.log"
marker="$T/shell-marker"
if "$T/m2tool" get-symbol "missing; touch $marker; #" _physics_entry > "$T/injection.log" 2>&1; then
  echo 'unexpected PASS: nonexistent object' >&2; exit 1
fi
[ ! -e "$marker" ] || { echo 'shell interpreted object path' >&2; exit 1; }

mkdir "$T/public"
chmod 777 "$T/public"
if QEMU=/bin/true "$ROOT/tools/qemu_inspect.sh" --out "$T/public" --marker READY \
  > "$T/inspect.log" 2>&1; then
  echo 'inspector accepted publicly writable output' >&2; exit 1
fi
grep -q 'output directory is not private' "$T/inspect.log"
ln -s "$T/public" "$T/symlink"
if QEMU=/bin/true "$ROOT/tools/qemu_inspect.sh" --out "$T/symlink" --marker READY \
  > "$T/symlink.log" 2>&1; then
  echo 'inspector accepted output symlink' >&2; exit 1
fi
grep -q 'output directory must not be a symlink' "$T/symlink.log"

# The historical checksum file stays immutable; each absolute path resolves
# against the original source or receipt commit, not today's working tree.
# a2c0d7f is not on any branch, so a default clone lacks it: fetch it by sha.
# A pin that still cannot be read fails below as a digest mismatch.
for pin in a2c0d7fbc03f7ce91fcb6389312a0ee8630322cd b64753d95bacb1114ba48decde48239f0c542e12; do
  git -C "$ROOT" cat-file -e "$pin^{commit}" 2>/dev/null || git -C "$ROOT" fetch -q origin "$pin" || true
done
n=0
while read -r digest historical; do
  case $historical in
    /home/*/workspace/m17-physics/*)
      relative=${historical#*/workspace/m17-physics/}
      case $relative in
        evidence/*) pin=b64753d95bacb1114ba48decde48239f0c542e12 ;;
        *) pin=a2c0d7fbc03f7ce91fcb6389312a0ee8630322cd ;;
      esac
      actual=$(git -C "$ROOT" show "$pin:$relative" | sha256sum)
      [ "${actual%% *}" = "$digest" ] || { echo "historical digest mismatch: $relative" >&2; exit 1; }
      n=$((n + 1)) ;;
  esac
done < "$ROOT/evidence/m16-requalification/SHA256SUMS"
[ "$n" -eq 8 ] || { echo "historical path count changed: $n" >&2; exit 1; }
printf 'PHYSICS-HOST-SECURITY PASS (sanitizer bounds, argv, private output refusal, M16 provenance)\n'
