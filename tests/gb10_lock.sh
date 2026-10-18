#!/bin/bash
# tests/gb10_lock.sh -- the GB10 lock for GPU gate scripts (sourced, not run).
#
#   gb10_lock_acquire   exclusive flock(2) on $GB10_LOCK through fd 9, waiting for any other holder;
#                       or nothing, when a process above this shell already holds it
#   gb10_lock_release   closes fd 9 (the kernel drops the lock with the last copy of the descriptor)
#
# Why the parent check: the Spark judge (aien-test worker) takes /tmp/aien-gb10.lock for a whole gb10
# job and then runs the gate in its sandbox. The gate opening the file again gets a separate open file
# description, and man 2 flock: "An attempt to lock the file using one of these file descriptors may be
# denied by a lock that the calling process has already placed via another file descriptor." The gate
# then waits on its own parent forever (2026-10-18: physics #64, forge-gate34, 83 minutes, every chip
# job on the Spark queued behind it). A holder that is not an ancestor is waited for, as before.
#
# The file is opened read-only when it exists: fs.protected_regular=2 refuses an O_CREAT open of another
# user's file in sticky /tmp, and flock(2) locks whatever the open mode (sc#627).
# The gb10 sandbox profile shares the PID namespace and /proc (aien-run-exec: unshare -m only), so the
# ancestors and /proc/locks are visible from inside. Shell + coreutils + awk. No Python.

GB10_LOCK=${GB10_LOCK:-/tmp/aien-gb10.lock}

# gb10_lock_held_by_ancestor FILE: 0 when this shell or one of its ancestors holds a flock on FILE.
gb10_lock_held_by_ancestor() {
    local id holders pid p
    # /proc/locks names the file as MAJOR:MINOR:INODE (major and minor in hex); waiters are "N: -> FLOCK ...".
    id=$(stat -L -c '%Hd %Ld %i' "$1" 2>/dev/null) || return 1
    id=$(printf '%02x:%02x:%s' $id)
    holders=$(awk -v id="$id" '$2 == "FLOCK" && $6 == id { print $5 }' /proc/locks 2>/dev/null)
    [ -n "$holders" ] || return 1
    p=$$
    while [ -n "$p" ] && [ "$p" -gt 0 ]; do
        for pid in $holders; do [ "$p" = "$pid" ] && return 0; done
        [ "$p" -eq 1 ] && break
        p=$(awk '/^PPid:/ { print $2 }' "/proc/$p/status" 2>/dev/null)
    done
    return 1
}

gb10_lock_acquire() {
    if gb10_lock_held_by_ancestor "$GB10_LOCK"; then
        echo "[gb10-lock] $GB10_LOCK is held by a parent process (the judge's gb10 job): not taking it again"
        return 0
    fi
    if [ -e "$GB10_LOCK" ]; then exec 9< "$GB10_LOCK"; else exec 9>> "$GB10_LOCK"; fi \
        || { echo "cannot open $GB10_LOCK" >&2; return 1; }
    flock -x 9 || { echo "cannot take $GB10_LOCK" >&2; return 1; }
}

gb10_lock_release() { exec 9>&-; }
