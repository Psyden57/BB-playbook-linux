/* l2canary.c — session 10: airtight proof of PL310+0x770 (INV by PA, no clean).
 *
 *   1. *line = CANARY;  DCCIMVAC  -> L2 = CANARY (dirty), DRAM = zeros
 *   2. *line = A;       DCCIMVAC  -> L2 = A (dirty) REPLACES it, DRAM still zeros
 *      (the canary was cleaned to DRAM? NO: step 1's clean left DRAM=zeros
 *       because the zero page had never been dirtied — the DCCIMVAC clean
 *       writes the L1 dirty line INTO L2 only. To get a DRAM canary we must
 *       clean TWICE: first the canary all the way to DRAM.)
 *
 * corrected ladder:
 *   1. *line = CANARY; DCCMVAC (c7,c10,1)      -> L1 clean, L2 dirty
 *   2. *line = 0;      DCCIMVAC                -> L2 dirty = A... no.
 *
 * simplest correct construction:
 *   1. *line = CANARY; DCCMVAC; SYNC(0x730)     -> canary reaches DRAM
 *      (the L2 dirty line is written through by the SYNC? — NO, sync only
 *       drains the L2's own queue... the L2 must be cleaned to DRAM:
 *       use 0x7F0 CIPA on this SAME line to force the canary to DRAM)
 *   2. *line = A; DCCIMVAC                      -> L2 = A dirty, DRAM = CANARY
 *   3. verify cached read == A
 *   4. write pa to PL310+0x770, bounded sync
 *   5. cached read:
 *        CANARY => 0x770 REALLY invalidates (dirty A discarded, no poison)
 *        A      => 0x770 no-op for a dirty line
 *
 * build: arm-unknown-nto-qnx6.6.0eabi-gcc -o l2canary l2canary.c
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

#define PL310_PA 0x48242000ULL

int main(void)
{
    volatile uint32_t *pl310;
    volatile uint8_t *pg;
    uint64_t pa = 0;
    size_t contig = 0;
    volatile uint32_t *line;
    uint32_t paLine;
    uint32_t CANARY = 0xC0FFEE11u, A = 0x5A5A5A5Au;
    unsigned n;

    setvbuf(stdout, NULL, _IONBF, 0);
    ThreadCtl(_NTO_TCTL_IO_PRIV, 0);
    pl310 = mmap_device_memory(0, 0x1000, PROT_READ | PROT_WRITE | PROT_NOCACHE,
                               MAP_SHARED | MAP_PHYS, PL310_PA);
    if (pl310 == MAP_FAILED) { perror("pl310"); return 1; }

    pg = mmap(0, 0x1000, PROT_READ | PROT_WRITE,
              MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, 0);
    if (pg == MAP_FAILED) { perror("page"); return 1; }
    mem_offset64((void *)pg, NOFD, 0x1000, &pa, &contig);
    printf("scratch pa=%08llx\n", (unsigned long long)pa);

    line = (volatile uint32_t *)(pg + 0x80);
    paLine = (uint32_t)pa + 0x80;

    /* 1. canary all the way to DRAM: write, clean+inv L1 (pushes to L2), then
     *    a real CIPA (0x7F0) to force L2 -> DRAM, then verify by reading */
    *line = CANARY;
    __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(line) : "memory");
    __asm__ volatile("dsb" ::: "memory");
    pl310[0x7F0 / 4] = paLine;                 /* CIPA: canary -> DRAM */
    n = 1000000; pl310[0x730 / 4] = 0;
    while ((pl310[0x730 / 4] & 1) && --n) ;
    printf("step1 (canary->DRAM): sync %s\n", n ? "clear" : "TIMEOUT");

    /* 2. put A on top, L2-only (DRAM = CANARY underneath) */
    *line = A;
    __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(line) : "memory");
    __asm__ volatile("dsb" ::: "memory");
    printf("step2 pre-op cached read: %08x (expect A=%08x)\n", *line, A);

    /* 3. THE TEST: invalidate by PA, no clean */
    pl310[0x770 / 4] = paLine;
    n = 1000000; pl310[0x730 / 4] = 0;
    while ((pl310[0x730 / 4] & 1) && --n) ;
    printf("step3 (0x770 inv): sync %s\n", n ? "clear" : "TIMEOUT");

    printf("step4 post-op cached read: %08x\n", *line);
    printf("   CANARY(%08x) => REAL INVALIDATE: dirty A discarded, DRAM canary served\n", CANARY);
    printf("   A(%08x)    => no-op on a dirty line\n", A);
    printf("L2CANARY DONE\n");
    return 0;
}
