/* xntest.c — probe QNX 6.6 execute permissions for various mappings.
 * 1. heap (malloc)         2. anon|phys + PROT_EXEC   3. IRAM device map + EXEC
 * Each writes "bx lr" (0xE12FFF1E) and calls it. Run: on -C 0 /tmp/xntest
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

__asm__(".arch armv7-a\n.arch_extension sec\n");

static const uint32_t bxlr = 0xE12FFF1Eu;

typedef void (*fn_t)(void);

static int try_call(const char *name, void *code)
{
    printf("%-28s vaddr=%p ... ", name, code);
    fflush(stdout);
    ((fn_t)code)();
    printf("EXEC OK\n");
    return 0;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    uint32_t bc[8] = {0};
    volatile uint32_t *bcm = mmap_device_memory(0, 0x100,
            PROT_READ | PROT_WRITE | PROT_NOCACHE, MAP_SHARED | MAP_PHYS,
            0x9f000000ULL);
    if (bcm != MAP_FAILED) { bcm[0] = 0x54535845; bcm[1] = 0; }  /* 'EXST' */

    /* 1: heap — CONFIRMED XN-ENFORCED (skipped: SIGSEGV) */
    uint32_t *heap = (uint32_t *)malloc(64);
    memcpy(heap, &bxlr, 4);
    printf("1 heap: skipped (XN confirmed)\n");
    (void)heap;

    /* 2: anon|phys + PROT_EXEC */
    uint32_t *pg = mmap(0, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, 0);
    if (pg == MAP_FAILED) { perror("2 mmap"); return 1; }
    off64_t phys = 0; size_t cg = 0;
    mem_offset64(pg, NOFD, 0x1000, &phys, &cg);
    printf("2 anon|phys+EXEC phys=%llx\n", (unsigned long long)phys);
    if (bcm) bcm[1] = 2;
    memcpy(pg, &bxlr, 4);
    try_call("2 anon|phys+EXEC", pg);

    /* 3: IRAM via mmap_device_memory + PROT_EXEC */
    if (ThreadCtl(_NTO_TCTL_IO_PRIV, 0) == -1) perror("IO_PRIV");
    uint32_t *iram = mmap_device_memory(0, 0x1000,
            PROT_READ | PROT_WRITE | PROT_EXEC | PROT_NOCACHE,
            MAP_SHARED | MAP_PHYS, 0x40308000ULL);
    if (iram == MAP_FAILED) { perror("3 iram map"); return 1; }
    printf("3 iram+EXEC mapped\n");
    if (bcm) bcm[1] = 3;
    iram[0] = bxlr;
    if (iram[0] != bxlr) { printf("3 iram write verify FAILED\n"); return 1; }
    try_call("3 iram+EXEC", iram);

    printf("ALL EXEC PROBES SURVIVED\n");
    if (bcm) bcm[1] = 99;
    return 0;
}
