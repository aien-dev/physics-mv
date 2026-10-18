#!/bin/bash
# tests/test_gb10_lock.sh -- host-only tests (no GPU) for tests/gb10_lock.sh, on a scratch lock file.
#
#   red    a fresh descriptor under a parent that holds the lock is refused (the 2026-10-18 judge deadlock)
#   green  gb10_lock_acquire under that parent returns at once and says so
#   wait   a holder that is not an ancestor is waited for, then the lock is ours
#   own    with no holder the lock is taken through fd 9 and gb10_lock_release drops it
#   new    a missing lock file is created
#   marker AIEN_GB10_LOCK_HELD=1 from the runner skips the lock without opening the file; other values are ignored
# A broken helper would hang, so every case that could block runs bounded in its own process group (scratch file, no
# chip work). Exit 0 only when every case passes. Shell + coreutils + util-linux flock. No Python.
set -u
HERE=$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)
LIB=$HERE/gb10_lock.sh
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
L=$work/gb10.lock
: > "$L"
pass=0 fail=0
check() { if [ "$1" = 0 ]; then pass=$((pass + 1)); echo "[PASS] $2"; else fail=$((fail + 1)); echo "[FAIL] $2"; fi; }
# held_elsewhere FILE: 0 when another process holds FILE (a non-blocking try from a fresh process fails)
held_elsewhere() { ! flock -n "$1" true; }
# run_bounded TENTHS OUT CMD...: CMD in its own process group (setsid), output to OUT; past TENTHS tenths of a
# second the whole group is killed and the status is 124 (a broken helper leaves a child waiting on a lock it
# inherited, so killing only the top process would not end the case)
run_bounded() {
    local t=$1 o=$2 p i=0; shift 2
    setsid "$@" > "$o" 2>&1 &
    p=$!
    while kill -0 "$p" 2>/dev/null; do
        if [ "$i" -ge "$t" ]; then kill -- -"$p" 2>/dev/null; wait "$p" 2>/dev/null; return 124; fi
        sleep 0.1; i=$((i + 1))
    done
    wait "$p"
}

# red: the old pattern (open the file again, flock it) under a holding parent cannot get the lock
flock -x "$L" bash -c 'exec 9< "$1"; flock -w 2 -x 9' _ "$L"
check $([ $? -ne 0 ] && echo 0 || echo 1) "red: a second descriptor under the holding parent is refused (the deadlock)"

# green: the helper under the same parent
out=$work/green.out
run_bounded 50 "$out" flock -x "$L" bash -c '. "$1"; GB10_LOCK=$2; gb10_lock_acquire && echo ACQUIRED' _ "$LIB" "$L"; rc=$?
check $([ $rc = 0 ] && grep -q 'held by a parent process' "$out" && grep -q ACQUIRED "$out" && echo 0 || echo 1) "green: under a holding parent gb10_lock_acquire returns at once (rc $rc)"

# a deeper ancestor counts too (holder -> bash -> bash -> helper)
run_bounded 50 "$out" flock -x "$L" bash -c 'bash -c ". \"\$1\"; GB10_LOCK=\$2; gb10_lock_acquire" _ "$1" "$2"' _ "$LIB" "$L"; rc=$?
check $([ $rc = 0 ] && grep -q 'held by a parent process' "$out" && echo 0 || echo 1) "green: a grandparent holder is found (rc $rc)"

# wait: a sibling holds the lock for 1.5 s; the helper waits, then holds it itself
setsid flock -x "$L" sleep 1.5 &
h=$!
i=0; while ! held_elsewhere "$L" && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
t0=$(date +%s%N)
run_bounded 50 "$out" bash -c '. "$1"; GB10_LOCK=$2; gb10_lock_acquire || exit 1; date +%s%N > "$3"; sleep 0.5' _ "$LIB" "$L" "$work/t1"; rc=$?
kill -- -"$h" 2>/dev/null; wait "$h" 2>/dev/null
t1=$(cat "$work/t1" 2>/dev/null || echo "$t0")
waited_ms=$(( (t1 - t0) / 1000000 ))
check $([ $rc = 0 ] && [ $waited_ms -ge 1000 ] && ! grep -q 'held by a parent' "$out" && echo 0 || echo 1) "wait: a non-ancestor holder is waited for (${waited_ms} ms, rc $rc)"

# own: no holder; the lock is ours while fd 9 is open, and released after
. "$LIB"
GB10_LOCK=$L
gb10_lock_acquire; rc=$?
held_elsewhere "$L"; h1=$?
gb10_lock_release
held_elsewhere "$L"; h2=$?
check $([ $rc = 0 ] && [ $h1 = 0 ] && [ $h2 != 0 ] && echo 0 || echo 1) "own: taken through fd 9 (held $h1), dropped by gb10_lock_release (held $h2)"

# new: a missing file is created and locked
GB10_LOCK=$work/new.lock
gb10_lock_acquire; rc=$?
check $([ $rc = 0 ] && [ -e "$work/new.lock" ] && held_elsewhere "$work/new.lock" && echo 0 || echo 1) "new: a missing lock file is created and locked"
gb10_lock_release

# marker: the runner says it holds the lock (AIEN_GB10_LOCK_HELD=1, aien-sovereign-core #580); the helper returns at
# once without opening the file, even with no visible holder (the runner may sit in another PID namespace)
GB10_LOCK=$work/marker.lock
out=$work/marker.out
run_bounded 50 "$out" env AIEN_GB10_LOCK_HELD=1 bash -c '. "$1"; GB10_LOCK=$2; gb10_lock_acquire && echo ACQUIRED; if [ -e /proc/self/fd/9 ]; then echo FD9; fi' _ "$LIB" "$GB10_LOCK"; rc=$?
check $([ $rc = 0 ] && grep -q 'held by the caller' "$out" && grep -q ACQUIRED "$out" && ! grep -q FD9 "$out" && [ ! -e "$GB10_LOCK" ] && echo 0 || echo 1) "marker: AIEN_GB10_LOCK_HELD=1 skips the lock without opening it (rc $rc)"

# marker under the judge: sandbox-shaped call, a holding parent plus the marker
run_bounded 50 "$out" flock -x "$L" env AIEN_GB10_LOCK_HELD=1 bash -c '. "$1"; GB10_LOCK=$2; gb10_lock_acquire && echo ACQUIRED' _ "$LIB" "$L"; rc=$?
check $([ $rc = 0 ] && grep -q ACQUIRED "$out" && echo 0 || echo 1) "marker: under a holding parent with the marker, returns at once (rc $rc)"

# any other marker value is ignored: with a non-ancestor holder the helper still waits
setsid flock -x "$L" sleep 3 &
h=$!
i=0; while ! held_elsewhere "$L" && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
run_bounded 10 "$out" env AIEN_GB10_LOCK_HELD=0 bash -c '. "$1"; GB10_LOCK=$2; gb10_lock_acquire && echo ACQUIRED' _ "$LIB" "$L"; rc=$?
kill -- -"$h" 2>/dev/null; wait "$h" 2>/dev/null
check $([ $rc = 124 ] && ! grep -q ACQUIRED "$out" && echo 0 || echo 1) "marker: AIEN_GB10_LOCK_HELD=0 is not trusted, an unrelated holder is waited for (rc $rc)"

echo "gb10_lock: $pass passed, $fail failed: $([ $fail = 0 ] && echo PASS || echo FAIL)"
[ $fail = 0 ]
