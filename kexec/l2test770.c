/* l2test770.c — session 10: does the NS side have a REAL L2 line-op register?
 *
 * The audit (newdocs/audit-approach-2026-09-11.md) found:
 *  - mainline 6.15 cache-l2x0.h: INV_LINE_PA=0x770 (invalidate by PA, NO
 *    clean), CLEAN_LINE_PA=0x7B0, CLEAN_INV_LINE_PA=0x7F0, SYNC=0x730.
 *  - the project has only ever written 0x768 from NS ("CLEAN_INV_LINE_PA")
 *    — an offset that appears NOWHERE in mainline's l2x0 register set
 *    (0x740 = DUMMY_REG; 0x768 is between it and 0x770).
 *
 * This tool builds a DRAM-vs-L2 divergence on three separate cache lines
 * and then applies one op per line, checking which register (if any)
 * actually changes what a cached read returns:
 *
 *   line A (control): CLEAN_INV by PA (0x7F0) — expected to invalidate
 *        AND poison DRAM with the stale L2 content (the W-33 mechanism,
 *        demonstrated end-to-end).
 *   line B: 0x768 — expected NO-OP if 0x768 is not a real register.
 *   line C: 0x770 — the REAL test: invalidate without clean.
 *
 * Divergence construction (per line, using the W-37-era model):
 *   1. cached write of A (dirty in L1)
 *   2. ~256 KB cacheable stream (evicts L1 -> line lands dirty in L2;
 *      DRAM still holds the old content)
 *   3. NOCACHE write of B (reaches DRAM, does NOT touch L2)
 *   => cached read = A (stale L2), NOCACHE read = B (DRAM truth)
 * Then: NOCACHE readback (DRAM) + cached readback (view) around each op.
 *
 * All polls bounded (rule 10). No by-way ops (0x77C/0x7FC = deadlock).
 * NS PL310 READS safe (proven); the only NS WRITES here are the by-PA
 * line ops + sync. Run as root. If the box wedges: WDT2 (~59 s) reboots.
 * build: arm-unknown-nto-qnx6.6.0eabi-gcc -o l2test770 l2test770.c
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

#define PL310_PA   0x48242000ULL
#define LINE       32u                      /* L2C-310 line size */
#define NSYNC      1000000u                 /* bounded sync-poll countdown */

static volatile uint32_t *pl310;

static void sync(void)
{
    unsigned n = NSYNC;
    pl310[0x730 / 4] = 0;
    while ((pl310[0x730 / 4] & 1) && --n)
        ;
    printf("   sync: busy-bit clear (%s after %u)\n",
           n ? "ok" : "TIMEOUT", NSYNC - n);
}

int main(void)
{
    volatile uint8_t *cache_map;   /* cacheable window */
    volatile uint8_t *nc_map;      /* NOCACHE window (DRAM truth) */
    uint64_t pa = 0;
    size_t contig = 0;
    uint32_t A = 0x5A5A5A5Au, B = 0xDEADBEEFu;
    volatile uint32_t *cA, *cB, *cC;   /* cached views of lines A/B/C */
    volatile uint32_t *nA, *nB, *nC;   /* nocache views */
    uint32_t paA, paB, paC;
    unsigned i;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (ThreadCtl(_NTO_TCTL_IO_PRIV, 0) == -1)
        perror("IO_PRIV");

    pl310 = mmap_device_memory(0, 0x1000, PROT_READ | PROT_NOCACHE,
                               MAP_SHARED | MAP_PHYS, PL310_PA);
    if (pl310 == MAP_FAILED) { perror("pl310"); return 1; }

    /* one phys-backed page, cacheable mapping */
    volatile uint8_t *pg = mmap(0, 0x1000, PROT_READ | PROT_WRITE,
                                MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, 0);
    if (pg == MAP_FAILED) { perror("mmap page"); return 1; }
    if (mem_offset64((void *)pg, NOFD, 0x1000, &pa, &contig) == -1) {
        perror("mem_offset64"); return 1;
    }
    printf("scratch page: va=%p pa=%08llx\n", pg, (unsigned long long)pa);

    cache_map = pg;
    /* second, NOCACHE mapping of the SAME phys */
    nc_map = mmap_device_memory(0, 0x1000, PROT_READ | PROT_WRITE | PROT_NOCACHE,
                                MAP_SHARED | MAP_PHYS, pa);
    if (nc_map == MAP_FAILED) { perror("nc map"); return 1; }

    /* lines at offsets 0x00 (A=0x7F0 test), 0x40 (B=0x768), 0x80 (C=0x770) */
    paA = (uint32_t)pa + 0x00;
    paB = (uint32_t)pa + 0x40;
    paC = (uint32_t)pa + 0x80;
    cA = (volatile uint32_t *)(cache_map + 0x00);
    cB = (volatile uint32_t *)(cache_map + 0x40);
    cC = (volatile uint32_t *)(cache_map + 0x80);
    nA = (volatile uint32_t *)(nc_map + 0x00);
    nB = (volatile uint32_t *)(nc_map + 0x40);
    nC = (volatile uint32_t *)(nc_map + 0x80);

    /* --- divergence construction for each line ---
     * cached write of A (DRAM untouched yet), evict to L2 via a 256KB
     * stream over OTHER memory (the anon region itself, beyond the 3
     * test lines), then NOCACHE write of B. */
    memset((void *)cache_map, 0, 0x1000);        /* initial DRAM content */
    *cA = A; *cB = A; *cC = A;                   /* dirty in L1 */

    {   /* stream 256KB of fresh anon memory through the caches */
        volatile uint8_t *churn = mmap(0, 0x40000,
                                       PROT_READ | PROT_WRITE,
                                       MAP_ANON | MAP_PRIVATE, NOFD, 0);
        if (churn == MAP_FAILED) { perror("churn"); return 1; }
        for (i = 0; i < 0x40000; i += 64)
            ((volatile uint32_t *)(churn + i))[0] = 0x12345678;
        /* read it back once so evictions actually propagate */
        uint32_t s = 0;
        for (i = 0; i < 0x40000; i += 64) s += ((volatile uint32_t *)(churn + i))[0];
        printf("churn done (sum=%08x)\n", s);
        munmap((void *)churn, 0x40000);
    }

    *nA = B; *nB = B; *nC = B;                   /* DRAM truth = B */

    printf("pre-op: cached A/B/C = %08x/%08x/%08x  nocache = %08x/%08x/%08x\n",
           *cA, *cB, *cC, *nA, *nB, *nC);
    printf("(expect cached=%08x stale vs nocache=%08x — divergence?)\n", A, B);

    /* ---- line A: 0x7F0 clean+inv by PA (the working control) ---- */
    printf("== op 0x7F0 on line A (pa=%08x)\n", paA);
    pl310[0x7F0 / 4] = paA;
    sync();
    printf("   cached=%08x nocache=%08x (cached==A + DRAM poisoned to A => CIPA proven)\n",
           *cA, *nA);

    /* --- line B: 0x768 (the project's believed CLEAN_INV) --- */
    printf("== op 0x768 on line B (pa=%08x)\n", paB);
    pl310[0x768 / 4] = paB;
    sync();
    printf("   cached=%08x nocache=%08x (still A => 0x768 = no-op; B => 0x768 = real inv)\n",
           *cB, *nB);

    /* --- line C: 0x770 invalidate by PA, NO clean --- */
    printf("== op 0x770 on line C (pa=%08x)\n", paC);
    pl310[0x770 / 4] = paC;
    sync();
    printf("   cached=%08x nocache=%08x (cached flips to B => 0x770 WORKS from NS; DRAM stays B)\n",
           *cC, *nC);

    printf("L2770 DONE\n");
    return 0;
}
