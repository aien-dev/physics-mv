/* Host test for m16_native_wait_marker(_ge): a fake "GPU" thread writes the marker, no chip.
 * Built against m16/m16_native.c with the NVRM symbols left unresolved (the test never calls them).
 * L343 / G5-physics-train-1: a wait the device answers within microseconds must not sleep-poll. */
#define _GNU_SOURCE
#include "m16_native.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static int failures;
static uint64_t cpu_ns_total;
static uint64_t thread_cpu_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}

typedef struct { volatile uint32_t *word; uint32_t value; uint64_t delay_ns; uint64_t wrote_ns; } Writer;

static void *writer_main(void *p) {
    Writer *w = p;
    uint64_t end = now_ns() + w->delay_ns;
    while (now_ns() < end) { }
    w->wrote_ns = now_ns();
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    *w->word = w->value;
    return NULL;
}

/* One wait whose marker lands delay_us after the waiter starts. Returns the observe latency in ns. */
static uint64_t one_wait(uint64_t delay_us, uint64_t *sleeps) {
    static volatile uint32_t word;
    word = 0;
    Writer w = { &word, 0x16c0ffee, delay_us * 1000ull, 0 };
    pthread_t th;
    uint64_t s0 = m16_native_wait_sleep_count();
    pthread_create(&th, NULL, writer_main, &w);
    uint64_t c0 = thread_cpu_ns();
    int rc = m16_native_wait_marker_ge(&word, 0x16c0ffee, 2000);
    cpu_ns_total += thread_cpu_ns() - c0;
    uint64_t seen = now_ns();
    pthread_join(th, NULL);
    CHECK(rc == 0, "wait returned %d", rc);
    *sleeps = m16_native_wait_sleep_count() - s0;
    return seen > w.wrote_ns ? seen - w.wrote_ns : 0;
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y;
}

int main(int argc, char **argv) {
    int trials = argc > 1 ? atoi(argv[1]) : 200;
    uint64_t delay_us = argc > 2 ? strtoull(argv[2], NULL, 10) : 300;
    int print = argc > 3;
    uint64_t lat[2000], slept = 0;
    if (trials > 2000) trials = 2000;
    int sleepers = 0;
    for (int i = 0; i < trials; i++) {
        uint64_t s;
        lat[i] = one_wait(delay_us, &s);
        slept += s;
        if (s) sleepers++;
    }
    qsort(lat, (size_t)trials, sizeof lat[0], cmp_u64);
    if (print)
        printf("BENCH delay_us=%llu trials=%d waits_that_slept=%d sleeps=%llu p50_ns=%llu p90_ns=%llu p99_ns=%llu cpu_ns_per_wait=%llu\n",
               (unsigned long long)delay_us, trials, sleepers, (unsigned long long)slept,
               (unsigned long long)lat[trials / 2], (unsigned long long)lat[trials * 9 / 10],
               (unsigned long long)lat[trials * 99 / 100], (unsigned long long)(cpu_ns_total / (uint64_t)trials));
    /* A device that answers 300 us in must be seen without a single sleep-poll in nearly every wait. */
    CHECK(sleepers * 10 <= trials, "%d of %d waits sleep-polled although the marker landed after %llu us",
          sleepers, trials, (unsigned long long)delay_us);

    /* Timeout: a marker that never lands returns -1 after the deadline, not before, and not wildly after. */
    volatile uint32_t never = 0;
    uint64_t t0 = now_ns();
    int rc = m16_native_wait_marker_ge(&never, 1, 20);
    uint64_t dt = now_ns() - t0;
    CHECK(rc == -1, "timeout returned %d", rc);
    CHECK(dt >= 20ull * 1000000ull, "returned before the deadline: %llu ns", (unsigned long long)dt);
    CHECK(dt < 200ull * 1000000ull, "overshot the deadline by %llu ns", (unsigned long long)(dt - 20ull * 1000000ull));

    /* Serial-number compare across the 0xffffffff -> 0 wrap, and overshoot accepted. */
    volatile uint32_t wrapped = 2;
    CHECK(m16_native_wait_marker_ge(&wrapped, 0xfffffff0u, 5) == 0, "wrap: 2 is past 0xfffffff0");
    volatile uint32_t behind = 0xfffffff0u;
    CHECK(m16_native_wait_marker_ge(&behind, 2, 5) == -1, "wrap: 0xfffffff0 is behind 2");
    volatile uint32_t over = 9;
    CHECK(m16_native_wait_marker_ge(&over, 5, 5) == 0, "overshoot is satisfied");
    CHECK(m16_native_wait_marker_ge(NULL, 1, 5) == -1, "NULL marker");
    /* An already-satisfied marker returns at once with no sleep. */
    uint64_t s0 = m16_native_wait_sleep_count();
    CHECK(m16_native_wait_marker_ge(&over, 5, 1000) == 0, "ready marker");
    CHECK(m16_native_wait_sleep_count() == s0, "a ready marker must not sleep");

    /* The explicit window: 0 sleeps from the first miss (omega's platform poll after its own spin),
     * a long window spins through a marker that lands 1 ms in without one sleep. */
    volatile uint32_t late = 0;
    s0 = m16_native_wait_sleep_count();
    CHECK(m16_native_wait_marker_ge_spin(&late, 1, 3, 0) == -1, "spin 0 timeout");
    CHECK(m16_native_wait_sleep_count() > s0, "spin_us 0 must sleep-poll");
    Writer lw = { &late, 1, 1000000ull, 0 };
    pthread_t th;
    s0 = m16_native_wait_sleep_count();
    pthread_create(&th, NULL, writer_main, &lw);
    CHECK(m16_native_wait_marker_ge_spin(&late, 1, 2000, 500000) == 0, "long window wait");
    pthread_join(th, NULL);
    CHECK(m16_native_wait_sleep_count() == s0, "a 500 ms window must not sleep for a 1 ms wait");

    printf(failures ? "test_m16_native_wait: FAIL (%d)\n" : "test_m16_native_wait: PASS\n", failures);
    return failures ? 1 : 0;
}
