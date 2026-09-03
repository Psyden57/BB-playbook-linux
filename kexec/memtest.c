/* memtest.c — standalone DRAM integrity payload (no kernel, no jump).
 *
 * Prime suspect for the M=1 hang (bc ceiling 122) and the payload setup
 * deaths: DRAM corruption. Observed on-device: DTB magic edfe0dd0 read
 * back as adfe0dd0 (single-bit flip, bit30) in the 0x9FE00000 bc page.
 *
 * What it does:
 *   1. arm bc (step 60) + mirrors, kick WDT2
 *   2. place a 24MB contiguous buffer (MAP_ANON|MAP_PHYS, same placement
 *      guards as do_t3 so results map 1:1 onto kernel-run placements)
 *   3. 4-pattern sweep (A5/5A/FF/00) through the NOCACHE view (= true DRAM
 *      content), reporting EVERY mismatch with its physical address, the
 *      flipped bit, and per-16MB-region error counts (does NOT abort on
 *      first error — we need the bad-region map for buf_placement_bad)
 *   4. canary sweep of the 4 bc/ring pages (0x88/0x90/0x94/0x9FE00000),
 *      4KB each, re-arming bc after each (the observed flip was THERE)
 *   5. verdict: CLEAN (exit 0) / DRAM ERRORS (exit 2)
 *
 * Optional args: [rounds] (default 2) — repeat sweeps to catch
 * intermittent flips. Never jumps, never touches CP15/GIC/CPU1: a crash
 * here would still take procnto down (expected, bc pages survive), but
 * there is no self-reset path.
 *
 * Run pinned to CPU0:  on -C 0 /tmp/memtest [rounds]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

#define BC_ADDR        0x90000000ULL
#define BC_MAGIC       0x4c424b43u
#define BC_MAGIC2      0x4c424b44u
#define BUF_SIZE       0x1800000u   /* 24MB, same as T3_BUF_SIZE */

#define STEP_ARMED     60u
#define STEP_PLACED    61u
#define STEP_P0        62u   /* A5 done */
#define STEP_P1        63u   /* 5A done */
#define STEP_P2        64u   /* FF done */
#define STEP_P3        65u   /* 00 done */
#define STEP_CANARY    66u   /* bc/ring pages done */
#define STEP_CLEAN     67u

static volatile uint32_t *bc;
static volatile uint32_t *bcmir[3];
static const uint64_t bcmir_pa[3] = { 0x94000000ull, 0x88000000ull, 0x9FE00000ull };

static void bc_write(uint32_t step)
{
    if (bc) { bc[1] = step; }
    int mi;
    for (mi = 0; mi < 3; mi++) if (bcmir[mi]) bcmir[mi][1] = step;
}

static void bc_arm(uint32_t step)
{
    int i;
    if (bc) {
        bc[0] = BC_MAGIC; bc[1] = step; bc[2] = 0;
        bc[3] = 0; bc[4] = 0; bc[5] = BC_MAGIC2;
        bc[0x80 / 4] = 0; bc[0x84 / 4] = 0;
    }
    for (i = 0; i < 3; i++) {
        if (!bcmir[i]) continue;
        bcmir[i][0] = BC_MAGIC; bcmir[i][1] = step;
        bcmir[i][2] = 0; bcmir[i][3] = 0; bcmir[i][4] = 0; bcmir[i][5] = BC_MAGIC2;
        bcmir[i][0x80 / 4] = 0;
        bcmir[i][0x84 / 4] = 0;
    }
}

static void *mapdev(uint64_t pa, size_t len)
{
    void *p = mmap_device_memory(0, len, PROT_READ | PROT_WRITE | PROT_NOCACHE,
                                 MAP_SHARED | MAP_PHYS, pa);
    if (p == MAP_FAILED) { perror("mmap_device_memory"); exit(1); }
    return p;
}

/* Kick WDT2 (0x4A314000): complement of WTGR (+0x30), same as wdtkick. */
static volatile uint32_t *wdt;
static void wdt2_kick(void)
{
    uint32_t wtgr = wdt[0x30 / 4];
    wdt[0x30 / 4] = ~wtgr;
}

/* Same protected regions as do_t3's buf_placement_bad: bc/ring pages +-4MB
 * and the top-of-RAM decompressor zone. The VA/PA sub-offset rule is NOT
 * applied (no trampoline here) so placement parity with kernel runs holds
 * via the phys guards alone. */
static int placement_bad(off64_t p, size_t size)
{
    static const uint64_t prot[] = {
        0x88000000ull, 0x90000000ull, 0x94000000ull, 0x9FE00000ull
    };
    off64_t end = p + (off64_t)size;
    int i;
    for (i = 0; i < 4; i++)
        if ((uint64_t)p < prot[i] + 0x400000ull &&
            (uint64_t)end > prot[i] - 0x400000ull)
            return 1;
    return (uint64_t)p > 0xBE000000ull;
}

static int popcount32(uint32_t x)
{
    int n;
    for (n = 0; x; n++) x &= x - 1;
    return n;
}

/* One 4-pattern sweep of [nv, size) at base phys. Returns error count;
 * prints every mismatch (capped detail, full counts). */
static unsigned sweep(volatile uint8_t *ncb, off64_t phys, size_t size,
                      int round, const char *what)
{
    static const uint32_t pat[4] = { 0xA5A5A5A5u, 0x5A5A5A5Au,
                                     0xFFFFFFFFu, 0x00000000u };
    static const uint32_t step_after[4] = { STEP_P0, STEP_P1, STEP_P2, STEP_P3 };
    volatile uint32_t *nv = (volatile uint32_t *)ncb;
    size_t words = size / 4;
    unsigned errs = 0, printed = 0;
    int pi;
    uint32_t region_errs[16];
    memset(region_errs, 0, sizeof(region_errs));

    for (pi = 0; pi < 4; pi++) {
        size_t wi;
        unsigned perrs0 = errs;
        wdt2_kick();
        for (wi = 0; wi < words; wi++) nv[wi] = pat[pi];
        for (wi = 0; wi < words; wi++) {
            uint32_t got = nv[wi];
            if (got != pat[pi]) {
                off64_t pa = phys + (off64_t)(wi * 4);
                errs++;
                region_errs[(size_t)(pa >> 24) & 15]++;
                if (printed < 32) {
                    printf("  ERR r%d %s pat=%08x pa=%08llx got=%08x "
                           "xor=%08x bits=%d\n",
                           round, what, pat[pi], (unsigned long long)pa,
                           got, pat[pi] ^ got, popcount32(pat[pi] ^ got));
                    printed++;
                }
            }
        }
        printf("  r%d %s pat %08x: %u errs (total %u)\n",
               round, what, pat[pi], errs - perrs0, errs);
        bc_write(step_after[pi]);
        bc[2] = errs;
    }
    if (errs) {
        int r;
        printf("  r%d %s per-64MB region errs:", round, what);
        for (r = 0; r < 16; r++)
            if (region_errs[r]) printf(" [%x000000]=%u", r, region_errs[r]);
        printf("\n");
    }
    return errs;
}

int main(int argc, char **argv)
{
    int rounds = argc > 1 ? atoi(argv[1]) : 2;
    uint8_t *buf;
    volatile uint8_t *ncb;
    off64_t phys = 0;
    size_t contig = 0;
    unsigned total = 0;
    int round, attempt, i;
    static const off64_t want[] = { 0xA4000000ull, 0xA8000000ull, 0xAC000000ull };
    static const uint64_t canary[4] = { 0x88000000ull, 0x90000000ull,
                                        0x94000000ull, 0x9FE00000ull };

    setvbuf(stdout, NULL, _IONBF, 0);
    if (rounds < 1) rounds = 1;
    if (rounds > 100) rounds = 100;

    bc = mapdev(BC_ADDR, 0x100);
    for (i = 0; i < 3; i++) bcmir[i] = mapdev(bcmir_pa[i], 0x100);
    bc_arm(STEP_ARMED);
    printf("memtest: bc armed\n");

    wdt = mapdev(0x4A314000ull, 0x100);
    wdt2_kick();

    /* place the buffer: requested phys first, then fallback retries */
    buf = NULL;
    for (attempt = 0; attempt < 3 && !buf; attempt++) {
        uint8_t *b = mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE,
                          MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, want[attempt]);
        off64_t p = 0;
        if (b == MAP_FAILED) continue;
        if (mem_offset64(b, NOFD, BUF_SIZE, &p, &contig) == -1 ||
            contig < BUF_SIZE || p != want[attempt]) {
            munmap(b, BUF_SIZE);
            continue;
        }
        if (placement_bad(p, BUF_SIZE)) { munmap(b, BUF_SIZE); continue; }
        buf = b; phys = p;
        printf("buffer at requested phys %llx\n", (unsigned long long)p);
    }
    for (attempt = 0; attempt < 12 && !buf; attempt++) {
        uint8_t *b = mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE,
                          MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, 0);
        off64_t p = 0;
        if (b == MAP_FAILED) { perror("mmap(MAP_ANON|MAP_PHYS)"); return 1; }
        if (mem_offset64(b, NOFD, BUF_SIZE, &p, &contig) == -1 ||
            contig < BUF_SIZE) {
            fprintf(stderr, "buffer not contiguous (%llu)\n",
                    (unsigned long long)contig);
            return 1;
        }
        if (placement_bad(p, BUF_SIZE)) {
            /* leak rejected buffers (same rationale as do_t3) */
            printf("buffer phys=%llx rejected (kept), retrying\n",
                   (unsigned long long)p);
            continue;
        }
        buf = b; phys = p;
        break;
    }
    if (!buf) { fprintf(stderr, "no safe buffer placement\n"); return 1; }
    printf("buffer v=%p phys=%08llx size=%u contig=%llu\n", (void *)buf,
           (unsigned long long)phys, BUF_SIZE, (unsigned long long)contig);
    bc_write(STEP_PLACED);
    bc[3] = (uint32_t)phys;

    ncb = mapdev(phys, BUF_SIZE);

    for (round = 1; round <= rounds; round++) {
        printf("round %d/%d: 24MB sweep @%08llx\n",
               round, rounds, (unsigned long long)phys);
        total += sweep(ncb, phys, BUF_SIZE, round, "buf");
    }

    /* canary sweep of the bc/ring pages — the observed bit30 flip was in
     * the 0x9FE00000 page. Re-arm bc after each page. */
    for (i = 0; i < 4; i++) {
        volatile uint8_t *cv = mapdev(canary[i], 0x1000);
        printf("canary page %08llx\n", (unsigned long long)canary[i]);
        total += sweep(cv, (off64_t)canary[i], 0x1000, 1, "canary");
        bc_arm(STEP_CANARY);
        bc[2] = total;
    }

    bc[2] = total;
    if (total == 0) {
        bc_write(STEP_CLEAN);
        printf("VERDICT: CLEAN (%d rounds, 24MB + 4 canary pages)\n", rounds);
        return 0;
    }
    printf("VERDICT: DRAM ERRORS total=%u — blacklist bad regions in "
           "buf_placement_bad or switch units\n", total);
    return 2;
}
