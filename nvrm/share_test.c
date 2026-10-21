/* share_test: one GPU upload used by two processes (L383, 2026-10-21).
 *
 * Owner A allocates `size` bytes (nvrm_alloc, the weights' path), fills a pattern and exports the
 * handle over a unix socket. Importer B duplicates it read-only (nvrm_import) and checks every word,
 * then again after A frees its handle, then again after A has exited. A write by B must fault
 * (SIGSEGV in a grandchild), a bogus handle must be refused cleanly, the import must not take new
 * memory, and B's free must give the pages back. Prints SHARE_GATE <check> PASS|FAIL lines and a
 * SHARE_GATE_SUMMARY; exit 0 only when every check passes. Memory only: no kernel is launched.
 *
 *   share_test [bytes]      default 1 GiB
 */
#include "nvrm.h"

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static uint64_t pattern(uint64_t i, uint64_t seed) { return (i * 0x9E3779B97F4A7C15ull) ^ seed; }

static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }

static long mem_avail_mb(void) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    char k[64], u[16]; long v; long out = -1;
    while (fscanf(f, "%63s %ld %15s", k, &v, u) == 3) if (!strcmp(k, "MemAvailable:")) { out = v / 1024; break; }
    fclose(f);
    return out;
}

static uint64_t verify(const uint64_t *p, uint64_t words, uint64_t seed) {
    uint64_t bad = 0;
    for (uint64_t i = 0; i < words; i++) bad += (p[i] != pattern(i, seed));
    return bad;
}

static int xfer(int fd, void *b, size_t n, int send) {
    uint8_t *p = b;
    while (n) {
        ssize_t r = send ? write(fd, p, n) : read(fd, p, n);
        if (r <= 0) return -1;
        p += r; n -= (size_t)r;
    }
    return 0;
}

static int fails;
static void check(const char *name, int ok) { printf("SHARE_GATE %s %s\n", name, ok ? "PASS" : "FAIL"); if (!ok) fails++; }

static int importer(int fd, uint64_t size) {
    Nvrm rm; memset(&rm, 0, sizeof rm);
    if (nvrm_open(&rm) != 0) { fprintf(stderr, "B nvrm_open: %s\n", rm.err); return 2; }
    NvrmShare sh; uint64_t seed;
    if (xfer(fd, &sh, sizeof sh, 0) || xfer(fd, &seed, sizeof seed, 0)) return 2;
    printf("B got client 0x%x mem 0x%x size 0x%" PRIx64 "\n", sh.hclient, sh.hmemory, sh.size);

    NvrmShare bad = sh; bad.hmemory ^= 0x0f0f0000u; NvrmMem junk;
    int rc_bad = nvrm_import(&rm, &bad, 1, &junk);
    check("bogus_handle_refused_cleanly", rc_bad != 0 && !rm.faulted);
    if (rc_bad) printf("B bogus import refused: %s\n", rm.err);

    long a1 = mem_avail_mb(); double t0 = now_ms();
    NvrmMem m; int rc = nvrm_import(&rm, &sh, 1, &m);
    double t1 = now_ms(); long a2 = mem_avail_mb();
    if (rc != 0) { fprintf(stderr, "B nvrm_import: %s\n", rm.err); check("import", 0); goto done; }
    printf("B import %.2f ms; MemAvailable %ld MB -> %ld MB (drop %ld MB for %" PRIu64 " MB shared)\n", t1 - t0, a1, a2, a1 - a2, size >> 20);
    check("import_takes_no_new_memory", (a1 - a2) < (long)(size >> 20) / 4);

    double v0 = now_ms(); uint64_t bad_words = verify(m.cpu, size / 8, seed);
    printf("B verify %.0f ms, %" PRIu64 " bad words\n", now_ms() - v0, bad_words);
    check("bits_identical_owner_live", bad_words == 0);
    char msg[4] = "OK1"; if (xfer(fd, msg, 4, 1)) return 2;

    if (xfer(fd, msg, 4, 0)) return 2; /* "FRD": the owner freed its handle */
    check("bits_identical_after_owner_free", verify(m.cpu, size / 8, seed) == 0);
    memcpy(msg, "OK2", 4); if (xfer(fd, msg, 4, 1)) return 2;

    char eof; while (read(fd, &eof, 1) > 0) {} /* the owner exits: socket EOF */
    usleep(300000);
    check("bits_identical_after_owner_exit", verify(m.cpu, size / 8, seed) == 0);

    pid_t g = fork();
    if (g == 0) { volatile uint64_t *w = m.cpu; w[0] = 0xDEADull; _exit(0); }
    int st = 0; waitpid(g, &st, 0);
    check("cpu_write_refused_SIGSEGV", WIFSIGNALED(st) && WTERMSIG(st) == SIGSEGV);
    check("bits_identical_after_write_attempt", verify(m.cpu, size / 8, seed) == 0);

    long a3 = mem_avail_mb();
    int frc = nvrm_free(&rm, &m);
    if (frc) printf("B free: %s\n", rm.err);
    usleep(300000);
    long a4 = mem_avail_mb();
    printf("B free rc %d; MemAvailable %ld MB -> %ld MB (released %ld MB)\n", frc, a3, a4, a4 - a3);
    check("import_free_ok", frc == 0 && !rm.faulted);
    check("pages_released_after_last_free", (a4 - a3) > (long)(size >> 20) / 2);
done:
    nvrm_close(&rm);
    printf("SHARE_GATE_SUMMARY %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}

static int owner(int fd, uint64_t size, pid_t child) {
    Nvrm rm; memset(&rm, 0, sizeof rm);
    if (nvrm_open(&rm) != 0) { fprintf(stderr, "A nvrm_open: %s\n", rm.err); return 2; }
    long a0 = mem_avail_mb();
    NvrmMem m;
    if (nvrm_alloc(&rm, size, &m) != 0) { fprintf(stderr, "A nvrm_alloc: %s\n", rm.err); return 2; }
    uint64_t seed = 0x5EED0000C0FFEEull ^ (uint64_t)getpid();
    uint64_t *p = m.cpu;
    double f0 = now_ms();
    for (uint64_t i = 0; i < size / 8; i++) p[i] = pattern(i, seed);
    long a1 = mem_avail_mb();
    printf("A alloc+fill %" PRIu64 " MB in %.0f ms; MemAvailable %ld MB -> %ld MB\n", size >> 20, now_ms() - f0, a0, a1);
    NvrmShare sh;
    if (nvrm_export(&rm, &m, &sh) != 0) { fprintf(stderr, "A nvrm_export failed\n"); return 2; }
    printf("A export client 0x%x mem 0x%x size 0x%" PRIx64 "\n", sh.hclient, sh.hmemory, sh.size);
    if (xfer(fd, &sh, sizeof sh, 1) || xfer(fd, &seed, sizeof seed, 1)) return 2;
    char msg[4];
    if (xfer(fd, msg, 4, 0)) return 2; /* OK1 */
    int frc = nvrm_free(&rm, &m);
    printf("A free rc %d%s%s; MemAvailable %ld MB (B still holds the pages)\n", frc, frc ? ": " : "", frc ? rm.err : "", mem_avail_mb());
    memcpy(msg, "FRD", 4); if (xfer(fd, msg, 4, 1)) return 2;
    if (xfer(fd, msg, 4, 0)) return 2; /* OK2 */
    nvrm_close(&rm);
    close(fd); /* EOF: A is gone */
    int st = 0; waitpid(child, &st, 0);
    printf("A: owner free rc %d; importer exit %d\n", frc, WIFEXITED(st) ? WEXITSTATUS(st) : -1);
    return (frc == 0 && WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : 1;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0); /* both processes print through one pipe; _exit() must not lose lines */
    uint64_t size = argc > 1 ? strtoull(argv[1], NULL, 0) : (1ull << 30);
    size = (size + 0xfff) & ~0xfffull;
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { perror("socketpair"); return 2; }
    pid_t child = fork();
    if (child < 0) { perror("fork"); return 2; }
    if (child == 0) { close(sv[0]); _exit(importer(sv[1], size)); }
    close(sv[1]);
    return owner(sv[0], size, child);
}
