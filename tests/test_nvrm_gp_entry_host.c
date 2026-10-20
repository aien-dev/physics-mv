/*
 * test_nvrm_gp_entry_host.c -- host-only checks of the GPFIFO entry encoder and the
 * 40-bit pushbuffer address limit in nvrm_enqueue(). No GPU, no driver calls: the
 * channel is faked with plain memory and every refusal happens before any store.
 *
 * clc96f.h: GP_ENTRY0 GET 31:2, GP_ENTRY1 GET_HI 7:0, LEVEL 9:9, LENGTH 30:10, so the
 * pushbuffer address field is 40 bits and a VA at or above 2^40 would spill into
 * LEVEL/LENGTH.
 */
#include "nvrm.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VA40 (1ull << 40)

static int g_fail;
#define CHECK(cond, ...) do { if (!(cond)) { g_fail++; printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

static Nvrm g_rm;
static uint64_t g_ring[8];
static uint8_t *g_fifo;

static void fake_channel(void) {
    memset(&g_rm, 0, sizeof g_rm);
    memset(g_ring, 0, sizeof g_ring);
    g_rm.gpfifo = 1;
    g_rm.entries = 8;
    g_rm.userd_off = 4096;
    g_rm.fifo.cpu = g_fifo;
    g_rm.fifo.size = 8192;
    memset(g_fifo, 0, 8192);
}

static uint64_t ring_slot(uint32_t i) { return ((volatile uint64_t *)g_fifo)[i]; }

static void expect_enqueue(uint64_t va, uint32_t off, uint32_t nwords, int want_ok, const char *name) {
    NvrmMem pb = { .handle = 1, .va = va, .size = 0x10000, .cpu = NULL };
    fake_channel();
    int rc = nvrm_enqueue(&g_rm, &pb, off, nwords);
    CHECK((rc == 0) == want_ok, "%s: rc=%d err='%s'", name, rc, g_rm.err);
    CHECK(g_rm.put == (want_ok ? 1u : 0u), "%s: put=%u", name, g_rm.put);
    if (want_ok) {
        CHECK(ring_slot(0) == nvrm_gp_entry(va + off, nwords), "%s: ring entry %016llx", name,
              (unsigned long long)ring_slot(0));
    } else {
        CHECK(ring_slot(0) == 0, "%s: refused enqueue wrote ring entry %016llx", name,
              (unsigned long long)ring_slot(0));
        CHECK(strstr(g_rm.err, "40-bit") != NULL, "%s: err='%s'", name, g_rm.err);
    }
}

int main(void) {
    g_fifo = aligned_alloc(4096, 8192);
    if (!g_fifo) return 2;

    /* Encoder vectors for valid inputs: the chip-proven layout, bit-identical. */
    CHECK(nvrm_gp_entry(0x1000000000ull, 4) == ((4ull << 42) | (1ull << 41) | 0x1000000000ull), "vector 0x1000000000/4");
    CHECK(nvrm_gp_entry(0x1000000007ull, 1) == ((1ull << 42) | (1ull << 41) | 0x1000000004ull), "unaligned low bits dropped");
    CHECK(nvrm_gp_entry(VA40 - 4, 1) == ((1ull << 42) | (1ull << 41) | (VA40 - 4)), "top valid dword");
    CHECK(nvrm_gp_entry(0, 0x1fffff) == ((0x1fffffull << 42) | (1ull << 41)), "max length");
    CHECK(nvrm_gp_entry(0, 0x200001) == ((1ull << 42) | (1ull << 41)), "length masked to 21 bits");

    /* Out-of-field VA must never reach LEVEL/LENGTH (entry1 bits 8+). */
    uint64_t e = nvrm_gp_entry(VA40 + 0x1000, 4);
    CHECK(((e >> 32) & ~0xffull) == (((4ull << 10) | (1ull << 9)) & ~0xffull), "va>=2^40 spilled into entry1: %016llx", (unsigned long long)e);
    CHECK((e & (VA40 - 1)) == 0x1000, "va low 40 bits kept: %016llx", (unsigned long long)e);

    /* nvrm_enqueue: below the limit unchanged, over it refused. */
    expect_enqueue(0x1000000000ull, 0, 4, 1, "low va");
    expect_enqueue(VA40 - 16, 0, 4, 1, "span ends exactly at 2^40");
    expect_enqueue(VA40 - 16, 0, 5, 0, "span ends 4 bytes past 2^40");
    expect_enqueue(VA40 - 16, 8, 3, 0, "off pushes span past 2^40");
    expect_enqueue(VA40, 0, 1, 0, "va == 2^40");
    expect_enqueue(VA40 + 0x4000, 0, 4, 0, "va above 2^40");
    expect_enqueue(UINT64_MAX - 3, 0, 1, 0, "va near uint64 max");

    free(g_fifo);
    if (g_fail) { printf("RESULT: FAIL (%d)\n", g_fail); return 1; }
    printf("RESULT: PASS\n");
    return 0;
}
