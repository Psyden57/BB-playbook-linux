/* l2dirty.c — session 10: prove whether PL310+0x770 (INV by PA) from NS
 * actually DISCARDS a dirty L2 line (the audit's cure candidate).
 *
 * Per line (three lines, one per op):
 *   1. cached write A          -> dirty in L1
 *   2. DCCIMVAC(va)            -> clean+inv L1: A lands DIRTY in L2,
 *                                 L1 empty, DRAM still = old (zeros)
 *   3. cached read (verify)    -> L1 miss -> L2 hit = A
 *   4. op: write pa to PL310+op (writable NOCACHE mapping), bounded sync
 *   5. cached read again:
 *        old (0x2C2C2C2C page content... we use 0) => op REALLY invalidated
 *                                                     (dirty data LOST)
 *        A                                          => op was a no-op
 *
 * Control expectations: 0x7F0 (clean+inv) => read = A (the clean saves it
 * to DRAM first); 0x770 (inv only) => read = old IF the register is real.
 * 0x768: A (no-op) or old (if it is the L210-style clean+inv).
 * Run as root; one process; bounded polls (rule 10); no by-way ops.
 * build: arm-unknown-nto-qnx6.6.0eabi-gcc -o l2dirty l2dirty.c
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

#define PL310_PA 0x48242000ULL
#define NSYNC    1000000u

static volatile uint32_t *pl310;

static void sync_bounded(const char *tag)
{
    unsigned n = 1000000;
    pl310[0x730 / 4] = 0;
    while ((pl310[0x730 / 4] & 1) && --n)
        ;
    printf("   [%s] sync %s\n", tag, n ? "clear" : "TIMEOUT");
}

static uint32_t A = 0x5A5A5A5Au;
static uint32_t OLD = 0x2C2C2C2Cu;   /* what DRAM holds: we pre-fill it */

/* keep it simple: three fixed line offsets in the scratch page */
int main(void)
{
    volatile uint32_t *pl310r;
    volatile uint8_t *pg, *nc;
    uint64_t pa = 0;
    size_t contig = 0;
    volatile uint32_t *c0, *c1, *c2;      /* cached views: 0x7F0/0x768/0x770 lines */
    uint32_t pa0, pa1, pa2;
    int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    ThreadCtl(_NTO_TCTL_IO_PRIV, 0);

    pl310r = mmap_device_memory(0, 0x1000, PROT_READ | PROT_WRITE | PROT_NOCACHE,
                                MAP_SHARED | MAP_PHYS, PL310_PA);
    if (pl310r == MAP_FAILED) { perror("pl310"); return 1; }
    pl310 = pl310r;

    pg = mmap(0, 0x1000, PROT_READ | PROT_WRITE,
              MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, 0);
    if (pg == MAP_FAILED) { perror("page"); return 1; }
    mem_offset64((void *)pg, NOFD, 0x1000, &pa, &contig);
    printf("scratch pa=%08llx\n", (unsigned long long)pa);

    memset((void *)pg, 0x2C, 0xC0);           /* DRAM pre-state = 0x2C bytes */

    c0 = (volatile uint32_t *)(pg + 0x00);    /* line for 0x7F0 */
    c1 = (volatile uint32_t *)(pg + 0x40);    /* line for 0x768 */
    c2 = (volatile uint32_t *)(pg + 0x80);    /* line for 0x770 */
    pa0 = (uint32_t)pa + 0x00;
    pa1 = (uint32_t)pa + 0x40;
    pa2 = (uint32_t)pa + 0x80;

    printf("== control 0x7F0 (clean+inv expected to preserve A) ==\n");
    *c0 = A;
    __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(c0) : "memory"); /* DCCIMVAC */
    __asm__ volatile("dsb" ::: "memory");
    printf("   pre-op cached read: %08x (expect A=%08x: L2 hit)\n", *c0, A);
    pl310r[0x7F0 / 4] = pa0;
    sync_bounded("7F0");
    printf("   post-op cached read: %08x (A=preserved-by-clean; OLD=??)\n", *c0);

    printf("== 0x768 (project's believed CLEAN_INV) ==\n");
    *c1 = A;
    __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(c1) : "memory");
    __asm__ volatile("dsb" ::: "memory");
    printf("   pre-op cached read: %08x\n", *c1);
    pl310r[0x768 / 4] = pa1;
    sync_bounded("768");
    printf("   post-op cached read: %08x (A=no-op; %08x=real inv, DATA LOST)\n",
           *c1, OLD);

    printf("== 0x770 (invalidate by PA, NO clean — the cure candidate) ==\n");
    *c2 = A;
    __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(c2) : "memory");
    __asm__ volatile("dsb" ::: "memory");
    printf("   pre-op cached read: %08x\n", *c2);
    pl310r[0x770 / 4] = pa2;
    sync_bounded("770");
    printf("   post-op cached read: %08x (A=no-op; %08x=REAL INVALIDATE, dirty line discarded)\n",
           *c2, OLD);

    printf("L2DIRTY DONE\n");
    return 0;
}
