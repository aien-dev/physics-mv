/* sem_test.c -- step 1: own channel, semaphore release, no libcuda. */

#include "nvrm.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int libcuda_mapped(void) {
    FILE *f = fopen("/proc/self/maps", "r");
    char line[512];
    int hit = 0;
    while (f && fgets(line, sizeof line, f))
        if (strstr(line, "libcuda") || strstr(line, "libcudart") || strstr(line, "libnvidia")) hit = 1;
    if (f) fclose(f);
    return hit;
}

int main(void) {
    Nvrm rm;
    if (nvrm_open(&rm)) { fprintf(stderr, "open: %s\n", rm.err); return 1; }
    printf("gpu minor %u compute 0x%x gpfifo 0x%x usermode 0x%x sm_version 0x%x\n",
           rm.minor, rm.compute_class, rm.gpfifo_class, rm.usermode_class, rm.sm_version);
    if (nvrm_channel(&rm)) { fprintf(stderr, "channel: %s\n", rm.err); return 1; }
    printf("channel up: token 0x%08x fifo va 0x%lx\n", rm.token, (unsigned long)rm.fifo.va);

    NvrmMem pb, marker;
    if (nvrm_alloc(&rm, 0x1000, &pb) || nvrm_alloc(&rm, 0x1000, &marker)) { fprintf(stderr, "alloc: %s\n", rm.err); return 1; }
    volatile uint32_t *m = marker.cpu;
    uint32_t *w = pb.cpu, n = 0;
    w[n++] = nvrm_mthd(1, 0x0000, 1); w[n++] = rm.compute_class;           /* SET_OBJECT on subch 1 */
    w[n++] = nvrm_mthd(0, 0x005c, 5);                                       /* SEM_ADDR_LO..SEM_EXECUTE */
    w[n++] = (uint32_t)marker.va; w[n++] = (uint32_t)(marker.va >> 32);
    w[n++] = 0x0417c0de; w[n++] = 0;
    w[n++] = 0x1 | (1u << 20);                                              /* RELEASE, WFI */
    __asm__ volatile("dsb sy" ::: "memory");
    nvrm_submit(&rm, &pb, 0, n);
    for (int i = 0; i < 20000 && *m != 0x0417c0de; i++) usleep(50);
    printf("marker = 0x%08x\n", *m);
    printf("libcuda/libnvidia mapped in process: %s\n", libcuda_mapped() ? "YES" : "no");
    int ok = (*m == 0x0417c0de) && !libcuda_mapped();
    puts(ok ? "M17_STEP1_OWN_CHANNEL_PASS" : "M17_STEP1_FAIL");
    nvrm_close(&rm);
    return ok ? 0 : 2;
}
