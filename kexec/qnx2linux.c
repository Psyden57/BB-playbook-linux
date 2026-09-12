/* qnx2linux.c — QNX-side kexec payload (PlayBook, QNX 6.6, root).
 *
 * Modes:
 *   --hello [blob] [maxcount]  CPU1-release mechanics test (see ROUND 3 log)
 *   --t2    [blob]             CPU0 kexec jump test (PLAN OF RECORD):
 *     ThreadCtl(_NTO_TCTL_IO_PRIV) -> System mode (privileged CP15/SMC)
 *     1. place hello blob in MAP_ANON|MAP_PHYS buffer, verify via NOCACHE
 *     2. build flat L1 page table in IRAM @0x40304000 (identity DRAM + IRAM
 *        + the QNX vaddr window used to enter the stub)
 *     3. copy stub2 to IRAM @0x40308000
 *     4. clean+invalidate L1 by set/way (CP15, privileged)
 *     5. L2 range flush via monitor SMC#0 service 0x101 (r12=API, r0=phys,
 *        r1=size) for jump buffer / breadcrumbs / IRAM ranges
 *     6. GICD off, CPU1 held in warm reset (proven safe), breadcrumbs
 *     7. cpsid + TTBR0/entry/bc regs -> blx stub (never returns)
 *     stub: TTBR0 switch -> identity -> SCTLR M/C/I off -> jump blob
 *     blob: breadcrumbs + UART3 loop -> WDT2 fires (~15s) -> reboot
 *     PASS = breadcrumbs show steps 20/21/23 + blob step 3 + count>0
 *
 * Breadcrumb layout (u32 @0x9f000000):
 *   [0] magic 0x4c424b43  [1] step  [2] hello count  [3] maxcount
 *   [4] cpu0-alive counter [5] magic2
 * Steps: 0 armed · 10 image · 11 stub · 19 L2 flushed · 20 jump started
 *        21 identity · 23 MMU off · 3 blob running · 4 blob self-penned
 *        5 returned to QNX (--hello only)
 *
 * Run pinned to CPU0:  on -C 0 /tmp/qnx2linux --t2
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

__asm__(".arch armv7-a\n.arch_extension sec\n");

extern void clean_inval_l1_all(void);
extern char cont_start, cont_end;
extern uint32_t read_ccsidr_decode(uint32_t*, uint32_t*, uint32_t*);
extern uint32_t mon_call_full(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

#define BC_ADDR        0x90000000ULL
#define BC_MAGIC       0x4c424b43u
#define BC_MAGIC2      0x4c424b44u
#define IRAM_TABLE     0x40304000ULL   /* 16KB flat L1 table */
#define IRAM_STUB      0x40308000ULL  /* stub physical = its vaddr (identity) */
#define IRAM_MAXCODE   0x1000ULL
#define RSTCTRL_CPU0   0x4824340cULL
#define RSTCTRL_CPU1   0x4824380cULL
#define AUX_BOOT       0x48281800ULL
#define GICD_CTLR      0x48241000ULL
#define QNX_STARTUP1   0x8010e0f4u

#define STEP_ARMED     0u
#define STEP_IMAGE     10u
#define STEP_STUB      11u
#define STEP_L2        19u
#define STEP_JUMP      20u
#define STEP_IDENTITY  21u
#define STEP_MMUOFF    23u
#define STEP_TARGET    3u
#define STEP_PENNED    4u
#define STEP_RETURNED  5u

static volatile uint32_t *bc;
static volatile uint32_t *bcmir[3];   /* mirrors: survivor band pages */
static const uint64_t bcmir_pa[3] = { 0x94000000ull, 0x88000000ull, 0x9FE00000ull };

/* Scatter-test (ROUND 5) result: pages at 16MB stride in 0x85-0xA1 survive a
 * full jump+WDT2+QNX-reboot cycle; 0x9F/A4+/B4+ get repurposed by QNX boot.
 * Breadcrumb primary = 0x90000000, mirrors above; kernel DEBUG_LL rings live
 * in the same pages (omap4bc.S: 0x88/0x90/0x94). */

static void bc_write(uint32_t step)
{
    if (bc) { bc[1] = step; }
    int mi;
    for (mi = 0; mi < 3; mi++) if (bcmir[mi]) bcmir[mi][1] = step;
}

static void bc_snapshot_file(uint32_t step)
{
    FILE *f = fopen("/accounts/devuser/kexec-bc.log", "w");
    if (!f) return;
    fprintf(f, "step=%u nonce=%08x\n", step, bc ? bc[3] : 0);
    fclose(f);
    sync();
}

static void *mapdev(uint64_t pa, size_t len)
{
    /* PROT_EXEC on device maps is honored by QNX (rule 7) — the IRAM map
     * must be executable: the trampoline is entered through its vaddr. */
    void *p = mmap_device_memory(0, len, PROT_READ | PROT_WRITE | PROT_EXEC |
                                 PROT_NOCACHE, MAP_SHARED | MAP_PHYS, pa);
    if (p == MAP_FAILED) { perror("mmap_device_memory"); exit(1); }
    return p;
}

/* Pre-jump LED: set the FAN5702 to BLUE via QNX's /dev/i2c3 (the module is
 * clocked during the devctl; afterwards the probe toggles only the EN pin).
 * devctl shape RE'd from led-fan5702.so (see LED-RE.md). */
static void led_color_qnx(uint8_t color)
{
    uint8_t buf[18];
    int fd = open("/dev/i2c3", O_RDWR);
    if (fd < 0) { perror("/dev/i2c3 (led)"); return; }
    memset(buf, 0, sizeof(buf));
    *(uint32_t *)(buf + 0) = 0x36;      /* FAN5702 7-bit slave */
    *(uint32_t *)(buf + 4) = 2;         /* len */
    *(uint32_t *)(buf + 8) = 2;         /* stop */
    *(uint32_t *)(buf + 12) = 1;        /* restart */
    buf[16] = 0x10;                     /* GENERAL */
    buf[17] = color;                    /* R=0x02 G=0x04 B=0x08 */
    if (devctl(fd, 0x80100505 /*SEND*/, buf, sizeof(buf), NULL) != 0)
        perror("devctl(i2c3 led)");
    close(fd);
}

static void led_blue_qnx(void)
{
    led_color_qnx(0x08);
}

/* PlayBook W-86 (session 12): the direct NS access to I2C4 (0x48350000)
 * is SECURE-FILTERED — the --ledprobe run SIGBUSed (fltno=5) on the
 * first SYSCONFIG read (the MMCHS class, KNOWN_ISSUES #8; the box
 * survived). So: no direct register pokes, no AUTOIDLE force-off, and
 * NO kernel-side LED colors (a kernel access would abort). The LED =
 * the QNX devctl path only (led_color_qnx). */

/* Kick WDT2 (0x4A314000): write the complement of WTGR (+0x30) — the same
 * thing QNX's wdtkick does every 15 s. Gives the kernel a deterministic full
 * watchdog window after the jump regardless of when wdtkick last ran.
 * 2026-09-03: first run the proper ENABLE sequence (SPR 0xBBBB then 0x4444,
 * per the kernel's omap_wdt_enable) — wdt2_disable() on a previous abort
 * leaves the watchdog OFF, and a TGR complement write on a disabled watchdog
 * is a no-op = the jump would run with NO recovery window. */
static void wdt2_kick(void)
{
    volatile uint32_t *w = mapdev(0x4A314000ull, 0x100);
    uint32_t wtgr;
    int i;
    w[0x48 / 4] = 0xBBBB;                 /* SPR: enable sequence step 1 */
    for (i = 0; i < 100000 && (w[0x34 / 4] & 0x10); i++)
        ;
    w[0x48 / 4] = 0x4444;                 /* SPR: enable sequence step 2 */
    for (i = 0; i < 100000 && (w[0x34 / 4] & 0x10); i++)
        ;
    wtgr = w[0x30 / 4];
    w[0x30 / 4] = ~wtgr;
    printf("wdt2 kicked (wtgr=%08x wps=%08x)\n", wtgr, w[0x34 / 4]);
}

/* Disable WDT2 entirely — the TI sequence from the kernel's own
 * drivers/watchdog/omap_wdt.c omap_wdt_disable() (grepped 2026-09-03):
 * SPR(0x48) = 0xAAAA then 0x5555, polling WPS(0x34) bit 0x10 between.
 * Called on abort paths so a refused jump leaves QNX alive (W-2 surprise:
 * the abort left WDT2 armed from the kick and the box reset later). */
static void wdt2_disable(void)
{
    volatile uint32_t *w = mapdev(0x4A314000ull, 0x100);
    int i;
    w[0x48 / 4] = 0xAAAA;
    for (i = 0; i < 100000 && (w[0x34 / 4] & 0x10); i++)
        ;
    w[0x48 / 4] = 0x5555;
    for (i = 0; i < 100000 && (w[0x34 / 4] & 0x10); i++)
        ;
    printf("wdt2 disabled (wps=%08x)\n", w[0x34 / 4]);
}

static int buf_placement_bad(uint8_t *b, off64_t p, size_t size);

/* fdt_patch_memory: rewrite the /memory node's reg in a DTB blob to
 * <bank_base bank_size> (both u32 cells, DTB = big-endian). Minimal FDT
 * walk: header + struct block tokens only; same-size in-place patch, so
 * totalsize/strings stay valid. Returns 0 on success. The kernel's pv
 * PHYS_OFFSET (= load phys) MUST equal bank_base — this keeps memblock
 * inside the linear map (the bc=127 wall fix, 2026-09-03). */
static int fdt_patch_memory(uint8_t *fdt, size_t fdtlen,
                            uint32_t bank_base, uint32_t bank_size)
{
#define BE32(x) __builtin_bswap32(x)
    uint32_t magic, struct_off, strings_off;
    uint32_t *p, *end;
    uint32_t *reg_val = NULL;
    uint8_t *name_area;
    int armed = 0;

    if (fdtlen < 40) return -1;
    magic = BE32(*(uint32_t *)(fdt + 0));
    if (magic != 0xd00dfeed) return -1;
    struct_off  = BE32(*(uint32_t *)(fdt + 8));
    strings_off = BE32(*(uint32_t *)(fdt + 12));
    if (struct_off >= fdtlen || strings_off >= fdtlen) return -1;
    p = (uint32_t *)(fdt + struct_off);
    end = (uint32_t *)(fdt + fdtlen);
    name_area = fdt + strings_off;

#define FDT_BEGIN_NODE 0x1u
#define FDT_END_NODE   0x2u
#define FDT_PROP       0x3u
#define FDT_END        0x9u
#define FDT_NOP        0x4u
    while (p + 1 <= end) {
        uint32_t tok = BE32(*p++);
        if (tok == FDT_BEGIN_NODE) {
            uint8_t *np = (uint8_t *)p;
            armed = 0;
            /* skip node name (0-terminated, 4-byte aligned); the NUL is
             * NOT necessarily word-aligned — scan bytes, then round up */
            while (np < (uint8_t *)end && *np)
                np++;
            np++;                       /* eat the NUL */
            p = (uint32_t *)(((uintptr_t)np + 3u) & ~(uintptr_t)3u);
        } else if (tok == FDT_END_NODE) {
            armed = 0;
        } else if (tok == FDT_END) {
            break;
        } else if (tok == FDT_PROP) {
            uint32_t len  = BE32(p[0]);
            uint32_t noff = BE32(p[1]);
            const char *name = (const char *)(name_area + noff);
            uint8_t *val = (uint8_t *)(p + 2);
            if (!strcmp(name, "device_type") && len == 7 &&
                !memcmp(val, "memory\0", 7))
                armed = 1;
            else if (armed && !strcmp(name, "reg") && len == 8) {
                reg_val = (uint32_t *)val;
                reg_val[0] = BE32(bank_base);
                reg_val[1] = BE32(bank_size);
                return 0;
            }
            p += 2 + ((len + 3) / 4);       /* skip len,nameoff + value */
        }
        /* FDT_NOP: loop */
    }
    return -1;
#undef FDT_BEGIN_NODE
#undef FDT_END_NODE
#undef FDT_PROP
#undef FDT_END
#undef FDT_NOP
#undef BE32
}

static uint8_t *readfile2(const char *path, size_t *len, size_t maxlen);

static uint8_t *readfile(const char *path, size_t *len)
{
    return readfile2(path, len, 0x100000);
}

static uint8_t *readfile2(const char *path, size_t *len, size_t maxlen)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    long n;
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0 || (size_t)n > maxlen) { fprintf(stderr, "%s: bad size %ld (max %zu)\n", path, n, maxlen); exit(1); }
    b = malloc(n);
    if (!b || fread(b, 1, n, f) != (size_t)n) { perror("read"); exit(1); }
    fclose(f);
    *len = (size_t)n;
    return b;
}

/* TI HAL monitor call: SMC #0, r12 = API number, r0/r1 = args. Requires
 * System mode (ThreadCtl(_NTO_TCTL_IO_PRIV)) — see ROUND 3 MASTER KEY. */
static uint32_t mon_call(uint32_t api, uint32_t a0, uint32_t a1)
{
    register uint32_t r0 __asm__("r0") = a0;
    register uint32_t r1 __asm__("r1") = a1;
    register uint32_t r12v __asm__("r12") = api;
    __asm__ volatile("dsb\n\tsmc #0\n\tdmb"
                     : "+r"(r0)
                     : "r"(r1), "r"(r12v)
                     : "memory");
    return r0;
}

static int do_smcprobe(void)
{
    uint8_t *buf;
    off64_t phys = 0;
    size_t contig = 0;
    volatile uint32_t *blk;

    setvbuf(stdout, NULL, _IONBF, 0);
    bc = mapdev(BC_ADDR, 0x100);
    bc[0] = BC_MAGIC; bc[1] = 40; bc[5] = BC_MAGIC2;

    if (ThreadCtl(_NTO_TCTL_IO_PRIV, 0) == -1) {
        perror("IO_PRIV"); return 1;
    }
    printf("System mode\n"); bc[1] = 41;
    (void)mon_call;

    buf = mmap(0, 0x1000, PROT_READ | PROT_WRITE,
               MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, 0);
    if (buf == MAP_FAILED) { perror("mmap"); return 1; }
    mem_offset64(buf, NOFD, 0x1000, &phys, &contig);
    blk = mapdev(phys, 0x1000);
    blk[0] = 1; blk[1] = 1;
    printf("block phys=%08llx\n", (unsigned long long)phys); bc[1] = 42;




    printf("E: PL310 cache-id read...\n"); bc[1] = 49;
    {
        volatile uint32_t *pl = mapdev(0x48242000ULL, 0x1000);
        printf("  cache-id=%08x\n", pl[0]); bc[1] = 50;

        printf("F: PL310 clean+inv by way...\n"); bc[1] = 51;
        pl[0x7FC/4] = 0xFFFF;               /* clean+inv all ways */
        while (pl[0x730/4] & 1) ;           /* wait sync complete */
        printf("  done, sync=%08x\n", pl[0x730/4]); bc[1] = 52;

        printf("G: end-to-end L2 check...\n"); bc[1] = 53;
        {
            volatile uint32_t *cv = mmap(0, 0x1000, PROT_READ|PROT_WRITE,
                                         MAP_SHARED|MAP_PHYS, NOFD, phys);
            if (cv == MAP_FAILED) { perror("cached map"); return 1; }
            cv[0] = 0xA5A5A5A5; cv[1] = 0x5A5A5A5A;
            pl[0x7FC/4] = 0xFFFF;
            while (pl[0x730/4] & 1) ;
            printf("  nocache view: %08x %08x (expect a5a5a5a5 5a5a5a5a)\n",
                   blk[0], blk[1]);
            bc[1] = 54;
        }
    }
    printf("all probes survived\n");
    return 0;
}


/* L2 range flush via monitor service 0x101 (clean+invalidate by phys range) */
static uint32_t l2_flush_range(uint32_t pa, uint32_t size)
{
    return mon_call(0x101, pa, size);
}

/* NS-side PL310 clean+invalidate by PA (0x7F0 = the L2C-310 op register,
 * foreground + NS-writable, PROVEN on-device 2026-09-11: the l2canary
 * ladder discarded a dirty L2 line and served the DRAM canary; 0x768,
 * which this code used before, is not a real op register — no-op at
 * best). Used to force DRAM truth for memory the post-L2-disable path
 * will read (stranded-dirty-line rule). */
static volatile uint32_t *pl310_ns;
static void l2c_ns_clean_range(off64_t pa, uint32_t size)
{
    while (size) {
        uint32_t chunk = size > 0x1000 ? 0x1000 : size;
        pl310_ns[0x7F0 / 4] = (uint32_t)pa;
        pl310_ns[0x730 / 4] = 0;
        while (pl310_ns[0x730 / 4] & 1)
            ;
        pa += chunk;
        size -= chunk;
    }
}

/* PlayBook W-90 (session 13): the wide stale-line cure for the L2-on
 * lottery. W-90a NAILED the mechanism: the L2 SURVIVES the WDT2 warm
 * reset + QNX's reboot — this run's kernel cached-read the DTB header
 * at 0xa34ee9a8 and got totalsize=15519 (= W-88's DTB, which sat at
 * that exact PA: W-88's blob + tree_zlen) instead of the deployed
 * 87321 — a previous run's CLEAN L2 line served as a FOSSIL. The
 * lottery = which fossil lines overlap the current run's PAs (the
 * per-run placement decides). Two region classes, two ops:
 *  - op 0x7F0 CIPA for regions with NO fresh DRAM truth pre-jump (the
 *    decompressor destination + pgd; QNX's live lowmem — the clean is
 *    data-preserving for QNX and writes junk over junk),
 *  - op 0x770 INV-ONLY for the jump buffer: DRAM = the payload's
 *    NOCACHE-verified fresh copy; the fossils there can be DIRTY
 *    (previous kernels' cached writes) and 0x7F0's clean step would
 *    push them OVER the fresh copy (W-33, for real). QNX-safe: the
 *    buffer = QNX's FREE pool (no live dirty QNX lines). The payload's
 *    own cached-written trampoline page is EXCLUDED here (caller
 *    cleans it with 0x7F0 separately).
 * Per-LINE ops (32B, mainline's CACHE_LINE_SIZE — the old
 * l2c_ns_clean_range does one op per 4KB = 1 line in 128!). Bounded
 * sync poll every 4096 lines (rule 10); *heartbeat = the chunk count
 * (names a mid-sweep wedge); return = sync timeouts. */
static uint32_t l2c_ns_line_range(uint32_t op, off64_t pa, uint32_t size,
                                  volatile uint32_t *heartbeat)
{
    uint32_t chunks = 0, timeouts = 0;
    size &= ~0x1Fu;     /* rule 13, learned W-90b: a non-32-multiple size
                         * underflows the tail chunk's `size -= 32` into
                         * an infinite sweep (bc[27]=476782 chunks at the
                         * WDT2 expiry — 1.95G device stores, no cliff) */
    while (size) {
        uint32_t i;
        unsigned n;
        for (i = 0; i < 4096 && size; i++) {
            pl310_ns[op / 4] = (uint32_t)pa;
            pa += 32;
            size -= 32;
        }
        n = 100000;
        pl310_ns[0x730 / 4] = 0;
        while ((pl310_ns[0x730 / 4] & 1) && --n)
            ;
        if (!n)
            timeouts++;
        if (heartbeat)
            *heartbeat = ++chunks;
    }
    return timeouts;
}

extern void clean_inval_l1_all(void);
extern char cont_start, cont_end;
extern char tramp_pos_start, tramp_pos_end;
extern uint32_t read_ccsidr_decode(uint32_t*, uint32_t*, uint32_t*);
extern uint32_t mon_call_full(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

/* 4-word TTBR0-switch trampoline, written into the (cached) jump buffer:
 *   mcr  p15,0,r8,c2,c0,0    TTBR0 = flat table
 *   isb
 *   ldr  pc, [pc, #-4]       branch to IRAM stub (0x40308000, identity-mapped)
 *   .word 0x40308000
 * The old TLB still covers the buffer's vaddr for these instructions; the
 * branch target walks the NEW table where the 0x403 section is identity. */
/* The ENTIRE jump stub lives in the executable buffer (PROT_EXEC proven OK):
 *   +0x000 blob (target)
 *   +0x800 trampoline (from stub3.S): bc=61, TTBCR=0, bc=62, DACR, bc=63,
 *         TTBR0=flat, bc=64, TLBIMVA(cont), bx r12 -> continuation
 *   +0x830 continuation (VA == PA): bc=21, TLBIALL, MMU/C/I off, bc=23,
 *         bx r9 -> blob (physical)
 * enter_stub passes: r8=TTBR0, r9=entry, r10=bc QNX vaddr (pre-switch),
 * r11=bc physical (post-switch), r12=continuation physical. */

static void enter_stub(uint32_t ttbr0, uint32_t entry, volatile uint32_t *bcv,
                       uint32_t bcphys, uint32_t cont_phys, uint32_t trampv,
                       uint32_t dtbphys, volatile uint32_t *gicdv)
{
    /* PIN the protocol registers — with plain "r" inputs the compiler may
     * allocate e.g. dtbphys to r2 itself, and `mov r7, r2` then forwards
     * QNX-context garbage (seen live: probe got r2=4, DTB read aborted). */
    register uint32_t r8v  __asm__("r8")  = ttbr0;
    register uint32_t r9v  __asm__("r9")  = entry;
    register uint32_t r10v __asm__("r10") = (uint32_t)bcv;
    register uint32_t r11v __asm__("r11") = bcphys;
    register uint32_t r12v __asm__("r12") = cont_phys;
    register uint32_t r7v  __asm__("r7")  = dtbphys;
    register uint32_t r6v  __asm__("r6")  = trampv;
    register uint32_t r4v  __asm__("r4")  = (uint32_t)gicdv;
    __asm__ volatile(
        /* NO SMC here — see do_t3: the 0x102 disable runs via mon_call()
         * in the C flow (the ONLY shape that ever returned: runs 2/3;
         * every enter_stub-inline SMC hung, 9 attempts, all contexts). */
        "cpsid if\n"
        "dsb\n"
        "mov r0, #70\n"
        "str r0, [r10, #4]\n"
        "blx r6\n"
        :
        : "r"(r8v), "r"(r9v), "r"(r10v), "r"(r11v), "r"(r12v),
          "r"(r7v), "r"(r6v), "r"(r4v)
        : "r0", "memory");
    __builtin_unreachable();
}

static int do_t2(const char *blobpath)
{
    size_t blen, slen;
    uint8_t *blob, *stub;
    volatile uint8_t *iram;
    volatile uint32_t *tt, *rst, *aux, *gicd;
    volatile uint32_t *nc, *bcv;
    uint8_t *buf;
    off64_t phys = 0, stub_phys = 0x40308000;
    size_t contig = 0;
    uint32_t v;
    int rc;
    int attempt;

    setvbuf(stdout, NULL, _IONBF, 0);

    if (ThreadCtl(_NTO_TCTL_IO_PRIV, 0) == -1) {
        perror("ThreadCtl(_NTO_TCTL_IO_PRIV) — cannot enter System mode");
        return 1;
    }
    printf("System mode entered\n");

    bc = mapdev(BC_ADDR, 0x100);
    bcv = bc;
    int i2;
    for (i2 = 0; i2 < 3; i2++) bcmir[i2] = mapdev(bcmir_pa[i2], 0x100);
    bc[0] = BC_MAGIC; bc_write(STEP_ARMED); bc[2] = 0;
    bc[3] = 0; bc[4] = 0; bc[5] = BC_MAGIC2;
    for (i2 = 0; i2 < 3; i2++) {
        bcmir[i2][0] = BC_MAGIC; bcmir[i2][1] = STEP_ARMED;
        bcmir[i2][2] = 0; bcmir[i2][3] = 0; bcmir[i2][4] = 0; bcmir[i2][5] = BC_MAGIC2;
        /* sanitize the DEBUG_LL ring headers (count + index): stale garbage
         * here makes the kernel's first print compute a wild strb address */
        bcmir[i2][0x80 / 4] = 0;
        bcmir[i2][0x84 / 4] = 0;
    }
    bc[0x80 / 4] = 0;
    bc[0x84 / 4] = 0;
    bc_write(30);
    printf("bc armed\n");
    led_blue_qnx();

    blob = readfile(blobpath, &blen);

    for (attempt = 0; attempt < 8; attempt++) {
        buf = mmap(0, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, 0);
        if (buf == MAP_FAILED) { perror("mmap(MAP_ANON|MAP_PHYS)"); return 1; }
        if (mem_offset64(buf, NOFD, 0x1000, &phys, &contig) == -1) {
            perror("mem_offset64"); return 1;
        }
        if (buf_placement_bad(buf, phys, 0x1000)) {
            printf("buffer v=%p phys=%llx rejected, retrying\n",
                   (void *)buf, (unsigned long long)phys);
            munmap(buf, 0x1000);
            continue;
        }
        break;
    }
    if (attempt == 8) { fprintf(stderr, "no safe buffer placement\n"); return 1; }
    bc_write(31);
    printf("jump buffer v=%p phys=0x%llx (%llu contiguous)\n", (void *)buf,
           (unsigned long long)phys, (unsigned long long)contig);
    nc = mapdev(phys, 0x1000);
    memcpy((void *)nc, blob, blen);
    if (memcmp((void *)nc, blob, blen)) {
        printf("FAIL: blob verify\n"); return 1;
    }
    bc_write(STEP_IMAGE);

    /* IRAM: flat table (0x40304000) + trampoline (0x40308000) + continuation
     * (0x40308040), all through one NOCACHE(+EXEC) map of 0x8000 bytes.
     * Trampoline executes from this map's vaddr (QNX-mapped, pre-switch);
     * continuation is entered at its physical address (identity-mapped). */
    iram = mapdev(IRAM_TABLE, 0x8000);
    printf("iram pre: %08x %08x\n", *(volatile uint32_t *)iram,
           *(volatile uint32_t *)(iram + 0x4000));
    memset((void *)iram, 0, 0x4000);                    /* table */
    tt = (volatile uint32_t *)iram;
    /* identity sections: DRAM 0x80000000-0xBFFFFFFF, AP=11, section */
    for (v = 0x800; v < 0xC00; v++)
        tt[v] = (v << 20) | 0xC02;
    /* peripheral domain (GPIO1 0x4A3, PL310 0x482, I2C4 0x483, UART3 0x480,
     * PRCM 0x4A0...): needed post-TTBR0-switch by the probe's LED toggles */
    for (v = 0x400; v < 0x500; v++)
        tt[v] = (v << 20) | 0xC02;
    tt[0x403] = 0x40300000 | 0xC02;                     /* IRAM identity */
    /* the buffer's own vaddr section -> its own phys section (identity for
     * the stub continuation after the TTBR0 switch) */
    tt[((uint32_t)(uintptr_t)buf >> 20)] =
        ((uint32_t)phys & 0xFFF00000u) | 0xC02;
    /* trampoline -> jump buffer +0x800: entered pre-switch via the buffer's
     * QNX vaddr, continues post-switch through the flat table's buffer
     * section alias — exact ONLY when (VA & 0xFFFFF) == (PA & 0xFFFFF),
     * which buf_placement_bad enforces. */
    memcpy((void *)(buf + 0x800), &tramp_pos_start,
           (size_t)(&tramp_pos_end - &tramp_pos_start));
    if (memcmp((void *)(buf + 0x800), &tramp_pos_start,
               (size_t)(&tramp_pos_end - &tramp_pos_start))) {
        printf("FAIL: tramp verify"); return 1;
    }
    {
        volatile uint8_t *trampv = iram + (IRAM_STUB - IRAM_TABLE);
        size_t clen = (size_t)(&cont_end - &cont_start);
        memcpy((void *)trampv, &cont_start, clen);
        if (memcmp((void *)trampv, &cont_start, clen)) {
            printf("FAIL: stub verify\n"); return 1;
        }
        printf("table + cont %zu bytes placed (cont 0x40308000)\n", clen);
        bc_write(STEP_STUB);
    }

    bc_write(39);
    bc[3] = (uint32_t)phys;
    bc[15] = (uint32_t)time(NULL) ^ (uint32_t)phys;   /* run nonce (phys varies per run) */
    bc_snapshot_file(39);                /* early snapshot: survives power-off */
    bc_write(40);

    /* ALL I/O BEFORE the CPU1 hold — after it, the console is dead */
    {
        uint32_t assoc = 0, sets = 0, lineb = 0;
        read_ccsidr_decode(&assoc, &sets, &lineb);
        printf("CCSIDR: assoc=%u numsets=%u linebytes=%u\n", assoc, sets, lineb);
    }
    bc_snapshot_file(41);   /* + sync: survives the reset */

    wdt2_kick();            /* fresh 15 s window for the kernel */

    /* NO I/O past this point */
    rst = mapdev(RSTCTRL_CPU1, 4);
    *rst = 1;               /* CPU1 held in warm reset */
    bc_write(37);
    clean_inval_l1_all();   /* single-core now: no cross-cache hazards */
    bc_write(32);
    gicd = mapdev(GICD_CTLR, 4);
    *gicd = 0;              /* GICD off — LAST step */
    bc_write(41);
    enter_stub(0x40304000u, (uint32_t)phys + 0x8000u, bcmir[0], 0x90000000u,
               0x40308000u,                 /* continuation phys (IRAM) */
               (uint32_t)(uintptr_t)buf + 0x800u, 0u,    /* T2: no DTB */
               gicd);
    return 0;
}

/* ---- T3: jump into a real Linux kernel (zImage with appended DTB) ----
 *  - blob buffer raised to 16 MB (MAP_ANON|MAP_PHYS, PROT_EXEC)
 *  - layout: zImage at +0 (padded to 8), DTB appended after it
 *  - entry = zImage base (word 0 is 'b start'), r2 = DTB phys (the appended
 *    copy — belt and braces: CONFIG_ARM_APPENDED_DTB finds it from the image
 *    end even if r2 were lost)
 *  - NO self-reset path: the kernel runs for real. WDT2 (armed, un-kicked)
 *    warm-resets the board <=15 s after the jump regardless of kernel state;
 *    breadcrumbs + the DEBUG_LL capture ring at 0x9FE00080 survive.
 */
#define T3_BUF_SIZE    0x1800000u   /* 24 MB (uncompressed Image is ~18 MB) */
#define ZIMG_PAD_MAX   0x1000u

/* Jump-buffer placement guards (phys only — the buffer holds just the blob
 * now; tramp/cont live in IRAM). Reject any overlap of the buffer SPAN
 * [p, p+size) with the breadcrumb/ring pages (±4 MB), or with the top of RAM
 * (the ARM decompressor relocates the whole zImage there). */
static int buf_placement_bad(uint8_t *b, off64_t p, size_t size)
{
    static const uint64_t prot[] = {
        0x88000000ull, 0x90000000ull, 0x94000000ull, 0x9FE00000ull
    };
    off64_t end = p + (off64_t)size;
    int i;

    /* the trampoline (buf+0x800) spans the TTBR0 switch and is fetched via
     * the flat table's buffer-section alias — exact only when the VA and PA
     * share the same 1 MB sub-offset */
    if (((uintptr_t)b & 0xFFFFFu) != ((uint32_t)p & 0xFFFFFu))
        return 1;

    for (i = 0; i < 4; i++) {
        if (p < prot[i] + 0x400000ull && end > prot[i] - 0x400000ull)
            return 1;
    }
    /* PlayBook W-35 (2026-09-05): the zImage decompressor inflates the
     * kernel to the BAKED zreladdr [0xa0008000, _end≈0xa0f80000) and
     * relocates itself + its malloc pool just above the destination when
     * they overlap. A window overlapping that region (W-35: 0xa0e00000 —
     * 1.5 MB overlap) tramples the zImage body/DTB mid-decompression and
     * died in head.S's tail (bc[1]=142, ring 0, the 0x3E7 wild write).
     * Reserve [0xa0000000, 0xa1000000): inflation + relocated
     * decompressor + heap margin. Every clean run placed >= 0xa1200000. */
    if (p < 0xA1000000ull && end > 0xA0000000ull)
        return 1;
    return p > 0xBE000000ull;
}

static int g_l2on;      /* --l2on: keep the PL310 enabled through the jump */
static int g_dmaquiet;  /* --dmaquiet: softreset MMC2 (eMMC DMA master) pre-jump */
/* PlayBook W-47 (2026-09-11): the fresh-boot (post-battery-pull) QNX pool
 * has NO 24 MB contiguous run at all (frag=129 on every hinted slot,
 * protected=12 -> NONE, twice) — but the buffer only NEEDS ~9 MB on the
 * no-probe path (kernel 5.6 MB + DTB 90 KB + pad; the cont/params live
 * in IRAM). Size the buffer to fit: 12 MB when no probe is loaded. */
#define T3_BUF_NOPROBE 0xC00000u    /* 12 MB — enough for zImage+DTB */
static uint32_t g_bufsize = T3_BUF_SIZE;

static int do_t3(const char *zpath, const char *dtbpath, const char *probepath)
{
    size_t zlen, dlen, plen = 0, padded;
    if (!probepath)
        g_bufsize = T3_BUF_NOPROBE;   /* W-47: no probe => 12 MB buffer */
    uint8_t *zimg, *dtb, *probe = NULL;
    volatile uint8_t *iram;
    volatile uint32_t *tt, *rst, *gicd;
    volatile uint32_t *nc, *bcv;
    volatile uint32_t *wdt;
    volatile uint32_t *pl310;
    uint8_t *buf;
    off64_t phys = 0;
    size_t contig = 0;
    uint32_t v, dtb_phys, kern_off, kern_phys, blob_off;
    int i2, attempt;

    setvbuf(stdout, NULL, _IONBF, 0);

    if (ThreadCtl(_NTO_TCTL_IO_PRIV, 0) == -1) {
        perror("ThreadCtl(_NTO_TCTL_IO_PRIV) — cannot enter System mode");
        return 1;
    }
    printf("System mode entered\n");
    /* CP15 state QNX leaves behind — the kernel ORs its ACTLR bits onto
     * this (proc-v7 preserves unknown bits!), and the M=1 hang suspect is
     * some QNX-set ACTLR/diagnostic bit. */
    {
        register uint32_t a __asm__("r0"), d __asm__("r1");
        __asm__ volatile("mrc p15, 0, %0, c1, c0, 1\n\t"   /* ACTLR */
                         "mrc p15, 0, %1, c15, c0, 1"      /* diagnostic */
                         : "=r"(a), "=r"(d) :: );
        printf("QNX ACTLR=%08x diag=%08x\n", a, d);
    }

    bc = mapdev(BC_ADDR, 0x100);
    bcv = bc;
    for (i2 = 0; i2 < 3; i2++) bcmir[i2] = mapdev(bcmir_pa[i2], 0x100);
    bc[0] = BC_MAGIC; bc_write(STEP_ARMED); bc[2] = 0;
    bc[3] = 0; bc[4] = 0; bc[5] = BC_MAGIC2;
    bc[15] = (uint32_t)time(NULL);      /* interim nonce (superseded below) */
    for (i2 = 0; i2 < 3; i2++) {
        bcmir[i2][0] = BC_MAGIC; bcmir[i2][1] = STEP_ARMED;
        bcmir[i2][2] = 0; bcmir[i2][3] = 0; bcmir[i2][4] = 0; bcmir[i2][5] = BC_MAGIC2;
        /* sanitize the DEBUG_LL ring headers (count + index): stale garbage
         * here makes the kernel's first print compute a wild strb address */
        bcmir[i2][0x80 / 4] = 0;
        bcmir[i2][0x84 / 4] = 0;
    }
    bc[0x80 / 4] = 0;
    bc[0x84 / 4] = 0;
    /* Post-M abort trap (cpt maps VA 0x000 + 0xFFFF to this mirror page):
     * the handler writes 0xAB to PA 0x900000A0 (via VA 0xD00000A0 — the
     * early ring section), flushes it through PL310 so it survives the
     * WDT2 reset, then spins. Readback 0x900000A0 != 0 => an abort fired
     * after MMU-on; 0 => death before any abort (M=1 write itself). */
    bcmir[2][0x0C / 4] = 0xEA00000Bu;   /* prefetch abort: b 0x40 */
    bcmir[2][0x10 / 4] = 0xEA00000Au;   /* data abort:     b 0x40 */
    bcmir[2][0x40 / 4] = 0xE3A000ABu;   /* mov r0, #0xAB */
    bcmir[2][0x44 / 4] = 0xE59F1030u;   /* ldr r1, [pc, #48]  -> 0x7C */
    bcmir[2][0x48 / 4] = 0xE5810000u;   /* str r0, [r1]  (flag VA) */
    bcmir[2][0x4C / 4] = 0xE59F102Cu;   /* ldr r1, [pc, #44]  -> 0x80 */
    bcmir[2][0x50 / 4] = 0xE59F2030u;   /* ldr r2, [pc, #48]  -> 0x88 */
    bcmir[2][0x54 / 4] = 0xE5821000u;   /* str r1, [r2]  (CIPA <- flag PA) */
    bcmir[2][0x58 / 4] = 0xE59F202Cu;   /* ldr r2, [pc, #44]  -> 0x8C */
    bcmir[2][0x5C / 4] = 0xE3A01000u;   /* mov r1, #0 */
    bcmir[2][0x60 / 4] = 0xE5821000u;   /* str r1, [r2]  (sync <- 0) */
    bcmir[2][0x64 / 4] = 0xE5921000u;   /* ldr r1, [r2] */
    bcmir[2][0x68 / 4] = 0xE3110001u;   /* tst r1, #1 */
    bcmir[2][0x6C / 4] = 0x1AFFFFFCu;   /* bne 0x60 */
    bcmir[2][0x70 / 4] = 0xEAFFFFFEu;   /* 1: b 1b (spin; WDT2 resets) */
    bcmir[2][0x7C / 4] = 0xD0000004u;   /* literal: flag VA = bc[1] itself */
    bcmir[2][0x80 / 4] = 0x90000004u;   /* literal: flag PA (reliable word) */
    bcmir[2][0x88 / 4] = 0xFEB42768u;   /* literal: PL310 clean+inv by PA */
    bcmir[2][0x8C / 4] = 0xFEB42730u;   /* literal: PL310 cache sync */
    bcmir[2][0xA0 / 4] = 0;             /* clear the flag */
    bc_write(30);
    printf("bc armed\n");
    led_blue_qnx();
    /* Kill DISPC scanout: the display stack userspace is already slain by
     * jump.sh, but DISPC hardware keeps fetching the last frame over the
     * L3/EMIF. The kernel's early C is the first sustained cacheable WB
     * traffic post-jump; concurrent scanout on marginally-timed PL310
     * latencies (see below) is the prime suspect for the random silent
     * wedges. Blanking scanout removes that contention. */
    {
        volatile uint32_t *dispc = mapdev(0x48050000ull, 0x1000);
        dispc[0x440 / 4] = 0;       /* DISPC_CONTROL: clear ENABLE */
        dispc[0x4A0 / 4] = 0;       /* DISPC_GFX_ATTRIBUTES */
        dispc[0x4C0 / 4] = 0;       /* DISPC_VID1_ATTRIBUTES */
        dispc[0x500 / 4] = 0;       /* DISPC_VID2_ATTRIBUTES */
        /* W-29 forensics: the docs say the kill "does not blank the
         * screen" — verify the registers actually read back cleared
         * (backlight-off != scanout-off; DISPC keeps fetching frames
         * with the backlight timed out — a ~150 MB/s DMA read master).
         * W-30: slots moved to bc[16]/bc[17] (0x90000040/44) — the probe
         * post-jump clobbered bc[7]/bc[14] in W-29; these survive it.
         * Read manually: memdump3 90000040 8. */
        bc[16] = dispc[0x440 / 4];  /* CONTROL readback (expect 0) */
        bc[17] = dispc[0x4A0 / 4];  /* GFX_ATTRIBUTES readback */
    }
    bc_write(34);
    /* PL310 config registers (control/aux/latency) are SECURE-FILTERED from
     * NS (SIGBUS/hang), and NS by-way clean+inv (0x7FC) is a BACKGROUND op
     * that deadlocked the machine twice (PL310 r3p2 erratum 727915 class).
     * By-PA line ops (0x7F0/0x770) are foreground + NS-writable (proven
     * 2026-09-11); the config registers are the filtered class. So: map the PL310
     * now, and AFTER the CPU1 hold let the TI monitor disable the L2
     * secure-side via SMC 0x105 (its internal clean sequence is
     * foreground/safe). Results go to breadcrumbs (console dead). */
    pl310 = mapdev(0x48242000ull, 0x1000);
    pl310_ns = pl310;   /* PlayBook W-40: the image sweep (bc 47) runs
                         * BEFORE the --l2on branch's own assignment —
                         * without this it deref'd NULL (+0x7f0, W-40a). */
    bc_write(35);
    /* Fresh 15 s WDT2 window for the whole setup: the file reads + 24 MB
     * NOCACHE copies + verifies take multiple seconds, and the remainder of
     * wdtkick's cycle is random -> the device reset mid-setup at random
     * points (observed 2026-08-31). The late kick before enter_stub then
     * gives the kernel its own full window. */
    wdt2_kick();

    /* PlayBook W-91 (session 13): the WHOLE-DRAM fossil sweep. W-90a
     * nailed the mechanism (the L2 = a cross-run fossil record: W-88's
     * kernel-era DTB-header line served W-90a's cached read at the same
     * PA, across the WDT2 reset + QNX's reboot) and W-90c proved the
     * dest+buffer sweeps do NOT cure the early C (the W-84 triad
     * reproduced) — the fossils live everywhere the early C's memblock
     * allocations land. ONE early clean+inv sweep of the whole DRAM
     * window, QNX-safe (0x7F0 = clean-first: every QNX dirty line goes
     * to DRAM before eviction; QNX keeps running through it; the
     * payload's own stack/heap lines = cleaned, not lost), placed
     * BEFORE the file reads so nothing we stage gets re-fossilized.
     * ~1GB = 33.5M line ops at the W-90b-proven ~33M ops/s = ~1-2 s.
     * Heartbeats: bc[29] = chunks, bc[30] = sync timeouts (rule 10
     * bounded polls); bc_write(36) = survived. The bc-51 dest/buffer
     * sweeps stay = belt-and-braces (the re-dirty window between here
     * and the jump = QNX-current lines only). --l2on only (one
     * variable). */
    if (g_l2on) {
        bc[30] = l2c_ns_line_range(0x7F0, 0x80000000ull, 0x40000000u,
                                   &bc[29]);
        bc_write(36);   /* heartbeat: the whole-DRAM sweep survived */
    }

    /* zImage (with appended DTB) + standalone DTB read */
    zimg = readfile2(zpath, &zlen, g_bufsize);
    dtb = readfile2(dtbpath, &dlen, 0x100000);
    bc_write(42);   /* heartbeat: files read */
    /* PlayBook W-90 (session 12): the MAGENTA marker moved HERE (from
     * bc 32) — the bc-32 window was ~1-3 s before the jump and the user
     * never saw it in W-88/89 (only blue). Here = mid-payload (~15 s
     * before the jump), still BEFORE the GICD-off (bc 41) — the devctl
     * is safe (the W-86/87 rule). Blue → magenta = "the payload is
     * past the file reads, jumping soon". */
    led_color_qnx(0x0A);
    padded = (zlen + 7u) & ~(size_t)7u;
    /* W-3 worst case: the kernel may sit up to 2MB into the buffer
     * (kern_off, below) — size the check for that */
    if (0x1FFFFFu + 0x8000u + padded + dlen + ZIMG_PAD_MAX > g_bufsize) {
        fprintf(stderr, "zImage+DTB too big: %zu+%zu\n", zlen, dlen);
        return 1;
    }
    if (probepath)
        probe = readfile2(probepath, &plen, 0x800);
    bc_write(43);   /* heartbeat: probe read */

    /* --dmaquiet (W-28/29 revision): direct MMC2 register access took a
     * SIGBUS (fltno=5) on the first SYSCONFIG read — the eMMC MMCHS is
     * NOT NS-accessible from the payload (secure-filtered/clock-domain
     * class, like the PL310 latency write). The QNX-native equivalent:
     * SLAY devb — no driver = no commands = no DMA, no register access
     * needed. Placed AFTER the last file read (all further payload I/O is
     * RAM-only; console output sits in the 64KB stdout buffer and never
     * reaches the eMMC-backed jump.log). devb's 10MB block-cache periodic
     * flush is the suspected periodic DMA source (W-25/26/27 per-run
     * randomness). QNX survives the slay (rootfs I/O dies, procnto and
     * SSH/RNDIS live; the device reboots fresh post-run anyway). */
    if (g_dmaquiet) {
        int rc = system("slay -f devb-mmcsd-winchester >/dev/null 2>&1");
        bc[14] = 0xD1EB0000u | (unsigned)(rc & 0xFFFFu);   /* slay rc */
        bc_write(55);
    }

    /* contiguous buffer (2026-09-03 v3): the DTB memory bank is PATCHED at
     * runtime to match the chosen placement, so placement flexibility is
     * safe — head.S derives PHYS_OFFSET = entry-0x8000 = buf phys, and the
     * kernel's memblock = exactly [phys, 0xc0000000). Constraints:
     *  - phys 2MB-aligned (the pv delta must stay 2MiB-aligned or
     *    __fixup_pv_table deadloops),
     *  - bank >= 256MB (phys <= 0xb0000000) — enough for early boot+CMA,
     *  - phys >= 0xa0000000 (the 2nd 512MB DRAM bank, per cfp flashinfo:
     *    banks 0x80000000-9FFFFFFF / 0xA0000000-BFFFFFFF),
     *  - buf_placement_bad guards the bc/ring/trap pages and the 1MB
     *    sub-offset VA/PA aliasing the trampoline needs.
     * Search: sweep EVERY 2MB-aligned slot of the upper bank as an mmap
     * hint (QNX honors hints for free ranges), then a generic any-address
     * loop. Every failure is counted by reason — v2 aborted blind
     * ("no usable placement" after a CLEAN boot) with zero visibility. */
    buf = NULL;
    /* PlayBook W-61 (2026-09-11): the fresh-boot QNX pool is fragmented
     * enough that even a 12 MB contiguous grab can fail twice in a row
     * (all 129 hinted slots non-contiguous; the generic fallback's grabs
     * land in the guarded inflation region). Ladder the size: 12 -> 8 ->
     * 6 MB (the kernel+DTB need ~5.6 MB; the memtest shrinks with it),
     * with a 2 s settle between passes so the pool can coalesce. */
    {
        static const uint32_t ladder[] = { 0xC00000u, 0x800000u, 0x600000u };
        unsigned li;
        for (li = 0; li < sizeof(ladder) / sizeof(ladder[0]) && !buf; li++) {
            if (li > 0)
                sleep(2);   /* settle: let the pool coalesce */
            g_bufsize = ladder[li];
            printf("placement pass %u: buffer %u KB\n", li,
                   g_bufsize / 1024);
            {
        int a, sweep = 0;
        /* failure counters by reason */
        int c_mmap = 0, c_moved = 0, c_frag = 0, c_unaligned = 0,
            c_bank = 0, c_prot = 0, c_alias = 0;
        for (a = 0; a < 129 && !buf; a++) {
            off64_t off = 0xA0000000ll + (off64_t)a * 0x200000ll;
            uint8_t *b = mmap(0, g_bufsize, PROT_READ | PROT_WRITE | PROT_EXEC,
                              MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, off);
            off64_t p = 0;
            contig = 0;
            sweep++;
            if (b == MAP_FAILED) { c_mmap++; continue; }
            if (mem_offset64(b, NOFD, g_bufsize, &p, &contig) == -1 ||
                contig < g_bufsize) { c_frag++; munmap(b, g_bufsize); continue; }
            if (p != off) { c_moved++; munmap(b, g_bufsize); continue; }
            if (p + (off64_t)g_bufsize > 0xC0000000ll ||
                p + 0x10000000ll > 0xC0000000ll) { c_bank++; munmap(b, g_bufsize); continue; }
            if (((uintptr_t)b & 0xFFFFFull) != ((uint32_t)p & 0xFFFFFull)) {
                if (c_alias < 3)
                    printf("slot %08llx: VA/PA sub-1MB mismatch (va %p pa %llx)\n",
                           (unsigned long long)p, (void *)b, (unsigned long long)p);
                c_alias++; munmap(b, g_bufsize); continue;
            }
            if (buf_placement_bad(b, p, g_bufsize)) { c_prot++; munmap(b, g_bufsize); continue; }
            buf = b; phys = p;
        }
        if (!buf) {
            /* generic fallback: any address QNX will give (usually fails
             * the 2MB alignment, but costs nothing to try) */
            for (a = 0; a < 12 && !buf; a++) {
                uint8_t *b = mmap(0, g_bufsize, PROT_READ | PROT_WRITE | PROT_EXEC,
                                  MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, 0);
                off64_t p = 0;
                contig = 0;
                if (b == MAP_FAILED) { c_mmap++; break; }
                if (mem_offset64(b, NOFD, g_bufsize, &p, &contig) == -1 ||
                    contig < g_bufsize) { c_frag++; munmap(b, g_bufsize); continue; }
                /* alignment NOT required here: kern_off (below) places the
                 * kernel at a 2MB-aligned offset inside the buffer (W-3:
                 * QNX's free pool has no aligned 24MB run) */
                if (p < 0xA0000000ll || p + 0x10000000ll > 0xC0000000ll) {
                    c_bank++; munmap(b, g_bufsize); continue;
                }
                if (((uintptr_t)b & 0xFFFFFull) != ((uint32_t)p & 0xFFFFFull)) {
                    c_alias++; munmap(b, g_bufsize); continue;
                }
                if (buf_placement_bad(b, p, g_bufsize)) { c_prot++; munmap(b, g_bufsize); continue; }
                buf = b; phys = p;
            }
        }
            printf("placement sweep: %d 2MB slots tried, failures: mmap=%d moved=%d "
                   "frag=%d unaligned=%d bank=%d protected=%d alias=%d -> %s\n",
                   sweep, c_mmap, c_moved, c_frag, c_unaligned, c_bank, c_prot,
                   c_alias, buf ? "FOUND" : "NONE");
            }
        }
    }
    if (!buf) {
        fprintf(stderr, "FATAL: no usable 2MB-aligned 24MB placement — "
                        "see the sweep counters above\n");
        wdt2_disable();
        return 1;
    }
    printf("jump buffer v=%p phys=0x%llx size=%u contig=%llu\n", (void *)buf,
           (unsigned long long)phys, g_bufsize, (unsigned long long)contig);
    /* W-31: record the chosen placement BEFORE the memtest/copies — W-30
     * died between bc 31 and bc 39 (memtest/copy phase, box frozen) with
     * the placement unrecoverable (bc[3] is only written at bc 39). If a
     * future run dies here, bc[18] names the suspect window. */
    bc[18] = (uint32_t)phys;
    bc_write(31);

    /* Kernel placement INSIDE the buffer (2026-09-03, from W-3): QNX's free
     * pool has no 2MB-aligned 24MB run (frag=129 on all hinted slots) but
     * the pv fixup needs the kernel entry 2MB-aligned (PHYS_OFFSET =
     * entry-0x8000 must be 2MB-aligned or __fixup_pv_table deadloops).
     * Place the kernel at the first 2MB-aligned offset inside the buffer —
     * costs <2MB, ~6MB slack. All downstream uses (blob_off, dtb_phys,
     * entry, DTB bank base) go through kern_phys. */
    kern_off = (uint32_t)(((phys + 0x1FFFFFull) & ~0x1FFFFFull) - phys);
    kern_phys = (uint32_t)phys + kern_off;
    printf("kernel_phys=%08x (buf phys %08llx kern_off=%x)\n",
           kern_phys, (unsigned long long)phys, kern_off);
    blob_off = kern_off + 0x8000u;

    /* Blob placed at buffer + blob_off: the kernel entry (kern_phys+0x8000)
     * must be (2MB-aligned + 0x8000) — head.S derives PHYS_OFFSET. */
    nc = mapdev(phys, g_bufsize);
    {
        /* DRAM integrity sweep: single-bit flips in the buffer region
         * corrupt the kernel image/table at placement-dependent offsets —
         * the leading suspect for the random silent deaths (observed:
         * DTB magic edfe0dd0 read back as adfe0dd0). Patterns through the
         * NOCACHE view = true DRAM content. First mismatch aborts. */
        static const uint32_t pat[4] = { 0xA5A5A5A5u, 0x5A5A5A5Au,
                                         0xFFFFFFFFu, 0x00000000u };
        volatile uint32_t *nv = (volatile uint32_t *)nc;
        size_t words = g_bufsize / 4;
        int pi;
        printf("memtest: sweeping %zu KB\n", g_bufsize / 1024);
        for (pi = 0; pi < 4; pi++) {
            size_t wi;
            for (wi = 0; wi < words; wi++) nv[wi] = pat[pi];
            for (wi = 0; wi < words; wi++) {
                if (nv[wi] != pat[pi]) {
                    printf("FAIL: DRAM bit error at phys=%08x "
                           "(want %08x got %08x)\n",
                           (uint32_t)(phys + wi * 4), pat[pi], nv[wi]);
                    return 1;
                }
            }
        }
        printf("memtest: clean\n");
        bc_write(44);   /* heartbeat: 24MB sweep clean */
    }
    {
        /* BYTE pointer for raw copies (uint32_t* + N scales by 4!);
         * rule #4, again */
        volatile uint8_t *ncb = (volatile uint8_t *)nc;
        printf("nc map v=%p\n", (void *)nc);
        memcpy((void *)(ncb + blob_off), zimg, zlen);
        printf("kernel copied to %08x\n", kern_phys + 0x8000u);
        if (memcmp((void *)(ncb + blob_off), zimg, zlen)) { printf("FAIL: kernel verify\n"); return 1; }
        bc_write(45);   /* heartbeat: kernel copy verified */
        if (padded > zlen)
            memset((void *)(ncb + blob_off + zlen), 0, padded - zlen);   /* pad to 8 */
        /* Patch the DTB memory bank to the ACTUAL placement BEFORE the copy:
         * PHYS_OFFSET (derived by head.S from the load address) must equal
         * the bank base or memblock spans unlinear-mappable DRAM (the
         * bc=127 wall). Bank = [kern_phys, 0xc0000000). */
        if (fdt_patch_memory(dtb, dlen, kern_phys,
                             0xC0000000ull - kern_phys)) {
            printf("FAIL: DTB memory-node patch\n"); return 1;
        }
        printf("DTB bank patched: base=%08x size=%08x\n",
               kern_phys, 0xC0000000u - kern_phys);
        dtb_phys = (uint32_t)phys + blob_off + (uint32_t)padded;
        memcpy((void *)(ncb + blob_off + padded), dtb, dlen);
        printf("DTB copied to %08x\n", dtb_phys);
        if (memcmp((void *)(ncb + blob_off + padded), dtb, dlen)) {
            printf("FAIL: DTB verify\n"); return 1;
        }
        bc_write(46);   /* heartbeat: DTB copy verified */
        /* PlayBook W-40 (2026-09-11): the image-region CIPA sweep. The
         * copy's own dirty L2 lines get forced to DRAM and the region's
         * QNX-era stale lines are discarded — the decompressor and the
         * C world then read only DRAM truth. 0x7F0 = clean+inv by PA,
         * proven NS-safe (kexec/l2canary.c). */
        l2c_ns_clean_range(phys, blob_off + padded + dlen);
        bc_write(47);   /* heartbeat: image sweep done */
    }
    printf("zImage %zu B + DTB %zu B (appended) placed; dtb_phys=%08x\n",
           zlen, dlen, dtb_phys);
    bc_write(STEP_IMAGE);
    bc[2] = (uint32_t)zlen;                  /* blob size in breadcrumbs */

    /* IRAM: flat table + cont (identical to T2) */
    iram = mapdev(IRAM_TABLE, 0x8000);
    memset((void *)iram, 0, 0x4000);
    tt = (volatile uint32_t *)iram;
    for (v = 0x800; v < 0xC00; v++)
        tt[v] = (v << 20) | 0xC02;
    /* peripheral domain (GPIO1 0x4A3, PL310 0x482, I2C4 0x483, UART3 0x480,
     * PRCM 0x4A0...): needed post-TTBR0-switch by the probe's LED toggles */
    for (v = 0x400; v < 0x500; v++)
        tt[v] = (v << 20) | 0xC02;
    tt[0x403] = 0x40300000 | 0xC02;
    tt[0x700] = (0xB00u << 20) | 0xC06;  /* CACHEABLE alias of PA 0xB0000000
                                          * (high DRAM) — probe.S long-loop
                                          * diagnostic writes here */
    tt[((uint32_t)(uintptr_t)buf >> 20)] =
        ((uint32_t)phys & 0xFFF00000u) | 0xC02;
    /* trampoline -> jump buffer +0x800: entered pre-switch via the buffer's
     * QNX vaddr, continues post-switch through the flat table's buffer
     * section alias — exact ONLY when (VA & 0xFFFFF) == (PA & 0xFFFFF),
     * which buf_placement_bad enforces. */
    memcpy((void *)(buf + 0x800), &tramp_pos_start,
           (size_t)(&tramp_pos_end - &tramp_pos_start));
    if (memcmp((void *)(buf + 0x800), &tramp_pos_start,
               (size_t)(&tramp_pos_end - &tramp_pos_start))) {
        printf("FAIL: tramp verify"); return 1;
    }
    bc_write(47);   /* heartbeat: tramp copied+verified */
    {
        volatile uint8_t *trampv = iram + (IRAM_STUB - IRAM_TABLE);
        size_t clen = (size_t)(&cont_end - &cont_start);
        memcpy((void *)trampv, &cont_start, clen);
        if (memcmp((void *)trampv, &cont_start, clen)) {
            printf("FAIL: stub verify\n"); return 1;
        }
        printf("table + cont %zu bytes placed (cont 0x40308000)\n", clen);
        if (probepath) {
            /* probe at IRAM 0x40309000; params at 0x40309800:
             * [0] kernel entry phys, [1] DTB phys */
            volatile uint8_t *probep = iram + 0x5000;
            memcpy((void *)probep, probe, plen);
            if (memcmp((void *)probep, probe, plen)) {
                printf("FAIL: probe verify\n"); return 1;
            }
            printf("probe %zu bytes placed (entry 40309000, params 40309800)\n",
                   plen);
        }
        /* params block: written UNCONDITIONALLY (2026-09-03, W-4 follow-up) —
         * the cont reads params[1] for r2 even on the no-probe (--t3) path,
         * where the old code left it stale/0 = the kernel silently booted
         * with NO DTB. [2]/[3] = kern_phys / DTB size for post-mortem. */
        {
            volatile uint32_t *parp = (volatile uint32_t *)(iram + 0x5800);
            parp[0] = kern_phys + 0x8000u;
            parp[1] = dtb_phys;
            parp[2] = kern_phys;
            parp[3] = (uint32_t)dlen;
        }
        bc_write(48);   /* heartbeat: cont(+probe) copied+verified */
        bc_write(STEP_STUB);
    }

    bc_write(39);
    bc[3] = (uint32_t)phys;
    bc[15] = (uint32_t)time(NULL) ^ (uint32_t)phys;   /* run nonce (phys varies per run) */
    bc_snapshot_file(39);
    bc_write(40);
    bc_snapshot_file(41);
    printf("jumping: entry=%08x dtb=%08x\n",
           probepath ? 0x40309000u : kern_phys + 0x8000u, dtb_phys);

    /* Re-verify EVERYTHING before entering the no-I/O zone, through the
     * NOCACHE/IRAM views (actual DRAM content): a stray DMA write into the
     * buffer or IRAM between the copy-time verifies and the jump corrupts
     * the kernel at a fixed physical region -> deterministic-per-placement
     * deaths (observed 2026-08-31). Abort the jump on any mismatch. */
    bc_write(49);   /* heartbeat: re-verify start */
    {
        volatile uint8_t *ncb = (volatile uint8_t *)nc;
        int bad = 0;
        if (memcmp((void *)(ncb + blob_off), zimg, zlen)) {
            printf("REVERIFY: kernel corrupted\n"); bad = 1;
        }
        if (memcmp((void *)(ncb + blob_off + padded), dtb, dlen)) {
            printf("REVERIFY: DTB corrupted\n"); bad = 1;
        }
        if (memcmp((void *)(buf + 0x800), &tramp_pos_start,
                   (size_t)(&tramp_pos_end - &tramp_pos_start))) {
            printf("REVERIFY: tramp corrupted\n"); bad = 1;
        }
        if (memcmp((void *)(iram + (IRAM_STUB - IRAM_TABLE)), &cont_start,
                   (size_t)(&cont_end - &cont_start))) {
            printf("REVERIFY: cont corrupted\n"); bad = 1;
        }
        if (probepath && memcmp((void *)(iram + 0x5000), probe, plen)) {
            printf("REVERIFY: probe corrupted\n"); bad = 1;
        }
        {
            uint32_t want[6];
            want[0] = 0x40300000u | 0xC02;                       /* IRAM */
            want[1] = ((uint32_t)phys & 0xFFF00000u) | 0xC02;    /* buffer */
            want[2] = (0x800u << 20) | 0xC02;                    /* DRAM lo */
            want[3] = (0xBFFu << 20) | 0xC02;                    /* DRAM hi */
            want[4] = (0x482u << 20) | 0xC02;                    /* PL310 */
            want[5] = (0x483u << 20) | 0xC02;                    /* I2C4 */
            if (tt[0x403] != want[0] ||
                tt[((uint32_t)(uintptr_t)buf >> 20)] != want[1] ||
                tt[0x800] != want[2] || tt[0xBFF] != want[3] ||
                tt[0x482] != want[4] || tt[0x483] != want[5]) {
                printf("REVERIFY: table corrupted\n"); bad = 1;
            }
        }
        if (bad) { printf("ABORT: RAM corruption detected pre-jump\n"); return 1; }
        printf("re-verify OK\n");
        bc_write(50);   /* heartbeat: re-verify OK, entering no-I/O zone */
    }

    /* --- L2 disable via TI monitor CTRL service 0x102 -------------------
     * RE'd from mainline TI OMAP4 monitor API (arch/arm/mach-omap2/
     * omap-secure.h + omap-smc.S): SMC#0, r12=0x100 L2X0 DBG_CTRL write,
     * 0x101 L2 clean+inv by PA (verified on-device), 0x102 L2X0 CTRL write,
     * 0x103/0x104/0x105 auxcoreboot read/modify/addr — which EXPLAINS the
     * 0x105 anomaly: it is the auxcoreboot-addr service, NOT L2 disable.
     * 0x108 SCU_PWR, 0x109 L2X0 AUXCTRL write, 0x113 L2X0 PREFETCH write.
     * No tag/data-latency service exists in this monitor API, so the
     * 0x333-latency plan is impossible via SMC: full disable instead.
     * On-device 2026-09-01: SMC 0x102 r0=0 RETURNS, control readback = 0.
     * With CTRL=0 the PL310 is bypassed and the no-L2X0 kernel runs
     * L1-only — a direct test of the early-C wedge theory. */
    printf("PL310 pre : ctrl=%08x aux=%08x dlat=%08x\n",
           pl310[0x100 / 4], pl310[0x104 / 4], pl310[0x10C / 4]);
    /* trampoline was copied through the CACHED view (kernel/DTB went via
     * NOCACHE): flush it so DRAM holds truth once the L2 is bypassed */
    l2_flush_range((uint32_t)phys, 0x1000);

    /* NOTE: the L3/EMIF auto-idle disable (CLKTRCTRL := SW_WKUP via the
     * CM2/CM1 registers) was REMOVED — the PRCM registers are SECURE-
     * FILTERED from NS: the first CLKSTCTRL write took a data abort
     * (SIGSEGV, run 31 — QNX survived, bc frozen at 50). The auto-idle
     * theory needs the monitor's PPA clock-domain service instead. */

    /* --dmaquiet W-28: the direct MMC2 softreset took a SIGBUS (fltno=5,
     * first SYSCONFIG read) — the MMCHS registers are NOT NS-accessible
     * from the payload. REMOVED; replaced by the QNX-native devb slay
     * after the file reads (see bc 55). */

    bc_write(53);

    /* NO I/O past this point (console dies at the CPU1 hold — rule #2).
     * Kernel calls (mapdev) happen here, before GICD off (rule #3). ALL
     * mappings happen BEFORE the L2 disable: with the L2 off, QNX cannot
     * survive a kernel call (stranded dirty L2 lines -> stale reads ->
     * wedge — observed twice, 2026-09-01). */
    rst = mapdev(RSTCTRL_CPU1, 4);
    wdt = mapdev(0x4A314000ull, 0x100);
    gicd = mapdev(GICD_CTLR, 4);
    /* PlayBook 2026-09-11 (session 11, W-78): the W-73 CPU1 PARK is
     * REVERTED to the proven hold. The park (a) did not cure the
     * TLB-op wedge (W-73 = still bc[1]=145), and (b) BROKE the
     * post-WDT2-reset recovery: the PRCM hold bit persists across
     * warm resets by design, so the released CPU1 re-entered the
     * ROM's SAR path after the reset and resurrected QNX mid-boot
     * (the W-77 post-mortem: the dark device for 11 minutes, the
     * dual-core collision). The hold = load-bearing. */
    *rst = 1;
    bc_write(37);
    clean_inval_l1_all();
    /* Fresh WDT2 window EARLY (right after the L1 clean): the SMC-hang
     * matrix correlates every 0x102 hang with a WDT kick in the immediate
     * pre-SMC window (runs 5-9) and both returns with no kick there (runs
     * 2/3) — possibly coincidence, but the reorder is free. Bare
     * device-register write. */
    { uint32_t g = wdt[0x30 / 4]; wdt[0x30 / 4] = ~g; }
    bc_write(32);
    /* PlayBook W-87/90: the magenta write was here (before the GICD-off,
     * the W-86/87 rule) but the bc-32 window = ~1-3 s before the jump —
     * the user never saw it in W-88/89. MOVED to bc 42 (mid-payload,
     * ~15 s of visibility). The GICD-off rule stands: NOTHING QNX-side
     * past the next line. */
    /* GICD off BEFORE the SMC (proven-safe position): no IRQ can fire in
     * the post-disable window, and the GICD store never runs with the L2
     * off — that store hung in runs 14/15 (bc stores + PL310 reads work
     * post-disable; the GICD store does not — bypass-path quirk). */
    *gicd = 0;
    bc_write(41);
    /* TI monitor L2 disable via mon_call — the C-flow position (the ONLY
     * shape that ever returned: runs 2/3; enter_stub-inline hung 9/9).
     * Post-SMC code is minimal and L2-off-safe: device-view readbacks,
     * device writes, then enter_stub (whose stack-arg loads are L1-hot
     * and whose downstream — tramp/cont/probe/kernel — is uncached or
     * DRAM-truth). NO kernel calls, NO printf, NO cached-data touches
     * after the disable (stranded-dirty-line rule). */
    bc_write(51);
    /* Pre-SMC DRAM-truth guarantee (the run-14 lesson): clean+inv by PA
     * (NS 0x7F0/0x730, foreground-safe) the payload's globals (.data/.bss
     * around &bc) and stack (around a local) regions, so every value the
     * post-disable path reads comes from DRAM instead of lines stranded
     * dirty in the about-to-be-bypassed L2. mon_call itself touches zero
     * stack (leaf, verified in disassembly). */
    {
        off64_t gpa = 0, spa = 0;
        uint64_t cg = 0, cs = 0;
        volatile char *gv = (volatile char *)&bc;   /* .data/.bss anchor */
        volatile char *sv = (volatile char *)&gpa;  /* stack anchor */
        pl310_ns = pl310;
        if (mem_offset64((void *)gv, NOFD, 0x4000, &gpa, &cg) == 0 &&
            mem_offset64((void *)sv, NOFD, 0x4000, &spa, &cs) == 0) {
            off64_t b = gpa & ~0xFFFULL, e = b + 0x4000;
            for (; b < e; b += 0x1000) l2c_ns_clean_range(b, 0x1000);
            b = (spa & ~0xFFFULL) - 0x8000;
            e = (spa & ~0xFFFULL) + 0x8000;
            for (; b < e; b += 0x1000) l2c_ns_clean_range(b, 0x1000);
        }
        /* stack slots for post-SMC code rewritten here so their lines are
         * dirty-in-L1 only if touched again before the disable (gpa/spa
         * live in registers through the mon_call — leaf-safe) */
    }
    {
        /* PlayBook W-90 (session 13): the wide stale-line cure — see
         * l2c_ns_line_range above. --l2on only (ONE VARIABLE vs W-89;
         * under the L2-off modes the SMC below bypasses the L2 anyway).
         * Positioned AFTER clean_inval_l1_all + the GICD-off (no IRQ can
         * re-dirty anything, no other thread runs) and BEFORE
         * enter_stub — the last word on the L2's fossil state. Three
         * regions, two ops:
         *  1. [0xa0000000, 0xa1069000) via 0x7F0 CIPA: the pgd PA
         *     0xa0004000 (head.S writes it C-off = DRAM truth; PTW reads
         *     must not hit fossils — the W-38 pair, generalized), the
         *     decompressor destination (Image 0x101ba50 -> 0xa1023a50 —
         *     142KB PAST the W-35 guard end!) and .bss (_end 0xa10680c8
         *     -> rounded). Clamped to the placement so it can never
         *     touch the buffer itself.
         *  2. [phys+0x1000, phys+used) via 0x770 INV-ONLY: the jump
         *     buffer's fossils (the kernel's fixed-map FDT reads and the
         *     decompressor's cached source reads hit these — W-90a's
         *     totalsize=15519 fossil). DRAM = the NOCACHE-verified fresh
         *     copy; inv-only cannot poison it. Excludes the trampoline
         *     page (the payload's cached memcpy = dirty lines that must
         *     NOT be discarded).
         *  3. [phys, phys+0x1000) via 0x7F0 CIPA: the trampoline page —
         *     clean the payload's own dirty cached lines to DRAM so
         *     enter_stub's cached re-read refetches the same bytes.
         * used = blob_off+padded+dlen (the blob + the standalone DTB).
         * bc[19]/bc[26] = dest heartbeat/timeouts; bc[27]/bc[28] =
         * buffer heartbeat/timeouts (kernel-era writers overwrite them
         * post-jump — they only matter for a mid-sweep wedge). */
        if (g_l2on) {
            uint32_t end = 0xA1069000u;
            uint32_t used, to;
            if ((off64_t)end > phys)
                end = (uint32_t)phys & ~0x1Fu;
            bc[26] = l2c_ns_line_range(0x7F0, 0xA0000000ull,
                                       end - 0xA0000000u, &bc[19]);
            used = blob_off + padded + dlen;
            if (used > g_bufsize)
                used = g_bufsize;
            if (used > 0x1000u) {
                to = l2c_ns_line_range(0x770, phys + 0x1000, used - 0x1000,
                                       &bc[27]);
                bc[28] = to;
            }
            l2c_ns_line_range(0x7F0, phys, 0x1000, NULL);
            bc_write(48);   /* heartbeat: all sweeps survived */
        }
    }
    {
        /* --l2on (2026-09-02): SKIP the disable entirely. The l2test A/B
         * proved the L2-off bypass path wedges under sustained device
         * traffic (phase A clean, phase C wedge at the pattern loops) —
         * the 171 wall / console corruption / count freezes are all the
         * bypass. The kernel (CONFIG_CACHE_L2X0=n) inherits QNX's enabled
         * L2 as-is; the kernel-side L2X0 driver is the follow-up. */
        if (g_l2on) {
            /* PlayBook W-84 (2026-09-12): the era --l2on semantics
             * RESTORED. W-46's rewrite (a692300) put the L2-off
             * mon_call(0x102) in here, silently disabling the L2 for
             * --l2on too (every W-78..83 run ran L2-OFF — the
             * docs/03 session-12 erratum). NO SMC in this branch:
             * the L2 stays ON, and the stub-safe readbacks are the
             * run's discriminator (bc[10] expect 1; probe.S's bc[8]
             * post-jump expect 1). */
            bc[6] = 0x2102;             /* marker: L2 kept on (may be
                                           overwritten by the stub echo) */
            bc[10] = pl310[0x100 / 4];  /* CTRL readback, stub-safe slot
                                           (expect 1 = L2 ON) */
            bc[4] = pl310[0x10C / 4];   /* data-latency readback */
        } else {
        uint32_t st = mon_call(0x102, 0, 0);
        bc[6] = st;                     /* SMC return status */
        bc[2] = pl310[0x100 / 4];       /* control readback (expect 0) */
        bc[4] = pl310[0x10C / 4];       /* data-latency readback (stale) */
        }
    }
    bc_write(52);
    /* PlayBook W-86/87 LESSON: NOTHING QNX-side may run after the GICD
     * went off (bc 41) — the interrupt-driven drivers block forever (the
     * magenta devctl froze the payload here, 2/2). The LED color is set
     * before the GICD-off now. NO kernel calls, NO printf, NO syscalls
     * from here to enter_stub. */
    enter_stub(0x40304000u,
               probepath ? 0x40309000u : kern_phys + 0x8000u,
               bcmir[0], 0x90000000u,
               0x40308000u,                 /* continuation phys (IRAM) */
               (uint32_t)(uintptr_t)buf + 0x800u, dtb_phys,
               gicd);                       /* mapped GICD view (post-SMC off) */
    return 0;
}

static int do_hello(const char *blobpath, unsigned maxcount)
{
    size_t blen, slen;
    uint8_t *blob, *stub;
    volatile uint32_t *rst, *aux, *par;
    volatile uint8_t *iram, *bufnc;
    uint8_t *buf;
    off64_t phys = 0;
    size_t contig = 0;
    int i;

    if (ThreadCtl(_NTO_TCTL_IO, 0) == -1)
        perror("ThreadCtl(IO)");

    bc = mapdev(BC_ADDR, 0x100);
    bc[0] = BC_MAGIC; bc[1] = STEP_ARMED; bc[2] = 0;
    bc[3] = maxcount; bc[4] = 0; bc[5] = BC_MAGIC2;
    bc[15] = (uint32_t)time(NULL);      /* interim nonce (superseded below) */

    blob = readfile(blobpath, &blen);
    stub = readfile("/tmp/stub.bin", &slen);
    if (slen > 0xF00) { fprintf(stderr, "stub too big\n"); return 1; }

    buf = mmap(0, 0x1000, PROT_READ | PROT_WRITE,
               MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, 0);
    if (buf == MAP_FAILED) { perror("mmap"); return 1; }
    if (mem_offset64(buf, NOFD, 0x1000, &phys, &contig) == -1) {
        perror("mem_offset64"); return 1;
    }
    printf("jump buffer phys=0x%llx\n", (unsigned long long)phys);
    bufnc = mapdev(phys, 0x1000);
    memcpy((void *)bufnc, blob, blen);
    bc[1] = STEP_IMAGE;

    iram = mapdev(0x40304000ULL, 0x1000);
    memcpy((void *)iram, stub, slen);
    par = (volatile uint32_t *)(iram + 0xf00);
    par[0] = (uint32_t)phys;
    par[1] = (uint32_t)BC_ADDR;
    par[2] = 0;
    bc[1] = STEP_STUB;

    rst = mapdev(RSTCTRL_CPU1, 4);
    aux = mapdev(AUX_BOOT, 8);
    *rst = 1;
    aux[1] = (uint32_t)0x40304000;
    aux[0] = (aux[0] & ~0xCu) | 0x6u;
    bc[1] = 12;
    *rst = 0;
    printf("CPU1 released; polling (max 10s)\n");
    for (i = 0; i < 100; i++) {
        if (bc[1] >= STEP_PENNED || bc[2] >= maxcount) break;
        bc[4]++;
        delay(100);
    }
    printf("bc: step=%u count=%u cpu0ticks=%u\n", bc[1], bc[2], bc[4]);
    if (bc[2] >= maxcount && bc[1] == STEP_PENNED) {
        aux[1] = QNX_STARTUP1;
        *rst = 1;
        *rst = 0;
        delay(1000);
        bc[1] = STEP_RETURNED;
        printf("CPU1 returned to QNX\n");
    } else {
        printf("unexpected state; rst=%08x boot1=%08x\n", *rst, aux[1]);
    }
    return 0;
}

/* --l2test: A/B the DRAM write path around the L2 disable, entirely from
 * the payload — no kernel jump, QNX SURVIVES. The kernel's corrupted
 * console text (ring chars |0x80, ring2 differently damaged) is written
 * via DEVICE-memory stores; this test replays exactly that path.
 *   phase A: L2 ON  — device-store pattern to a mapdev'd DRAM window,
 *                      verify (control: is the raw path already flaky?)
 *   phase B: hold CPU1, L1 clean+inv, NS CIPA payload globals+stack,
 *                      WDT2 kick EARLY, GICD off (the do_t3 pre-SMC
 *                      recipe — 0x102 hangs correlate with IRQs/mid-
 *                      flight; the first --l2test attempt hung and ate
 *                      a WDT2 reboot without this block)
 *   phase C: mon_call(0x102,0,0) disable, device-store pattern + verify
 *   phase D: mon_call(0x102,1,0) re-enable, verify CTRL=1, GICD on,
 *                      CPU1 back to QNX, THEN printf + bc results.
 * Rules honored: all mappings pre-disable; no printf/kernel calls while
 * the L2 is off; payload text/data/stack = DRAM truth via CIPA. */
static int do_l2test(void)
{
    uint8_t *buf;
    off64_t phys = 0, gpa = 0, spa = 0;
    size_t contig = 0;
    uint64_t cg = 0, cs = 0;
    volatile uint32_t *blk, *pl, *rst, *wdt, *gicd, *aux;
    unsigned i, mism_a = 0, mism_c = 0;
    uint32_t st_dis, st_en, ctrl_dis, ctrl_en;
    const unsigned N = 2048;                /* 8 KB */

    setvbuf(stdout, NULL, _IONBF, 0);
    bc = mapdev(BC_ADDR, 0x100);
    bc[0] = BC_MAGIC; bc[1] = 40; bc[5] = BC_MAGIC2;
    bc[2] = 0;

    if (ThreadCtl(_NTO_TCTL_IO_PRIV, 0) == -1) {
        perror("IO_PRIV"); return 1;
    }
    printf("System mode\n");

    buf = mmap(0, 0x2000, PROT_READ | PROT_WRITE,
               MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, 0);
    if (buf == MAP_FAILED) { perror("mmap"); return 1; }
    if (mem_offset64(buf, NOFD, 0x2000, &phys, &contig) == -1) {
        perror("mem_offset64"); return 1;
    }
    blk = mapdev(phys, 0x2000);
    pl = mapdev(0x48242000ULL, 0x1000);
    pl310_ns = pl;                  /* l2c_ns_clean_range's controller base */
    rst = mapdev(RSTCTRL_CPU1, 4);
    aux = mapdev(AUX_BOOT, 8);
    wdt = mapdev(0x4A314000ULL, 0x100);
    gicd = mapdev(GICD_CTLR, 4);
    printf("test block phys=%08llx (%u words), CTRL=%08x\n",
           (unsigned long long)phys, N, pl[0x100 / 4]);

    /* phase A — L2 on, device stores, QNX fully alive */
    for (i = 0; i < N; i++)
        blk[i] = 0xA5000000u ^ (i * 0x9E3779B9u);
    for (i = 0; i < N; i++)
        if (blk[i] != (0xA5000000u ^ (i * 0x9E3779B9u)))
            mism_a++;
    printf("A (L2 on ): mism=%u/%u\n", mism_a, N);
    bc[2] = (mism_a << 16);

    /* phase B — the do_t3 pre-SMC recipe; bc[6] = step marker (device
     * store, safe in every context — the readback pinpoints the wedge) */
    *rst = 1;                               /* hold CPU1 (warm reset) */
    bc[6] = 1;
    clean_inval_l1_all();
    bc[6] = 2;
    {   /* NS CIPA: payload globals + stack -> DRAM truth */
        volatile char *gv = (volatile char *)&bc;
        volatile char *sv = (volatile char *)&gpa;
        if (mem_offset64((void *)gv, NOFD, 0x4000, &gpa, &cg) == 0 &&
            mem_offset64((void *)sv, NOFD, 0x4000, &spa, &cs) == 0) {
            off64_t b = gpa & ~0xFFFULL, e = b + 0x4000;
            for (; b < e; b += 0x1000) l2c_ns_clean_range(b, 0x1000);
            b = (spa & ~0xFFFULL) - 0x8000;
            e = (spa & ~0xFFFULL) + 0x8000;
            for (; b < e; b += 0x1000) l2c_ns_clean_range(b, 0x1000);
        }
    }
    { uint32_t g = wdt[0x30 / 4]; wdt[0x30 / 4] = ~g; }   /* kick EARLY */
    bc[6] = 4;
    *gicd = 0;                              /* no IRQ can fire mid-SMC */
    bc[6] = 5;

    /* phase C — disable, device stores, verify */
    st_dis = mon_call(0x102, 0, 0);
    bc[6] = 6;
    bc[10] = st_dis;
    ctrl_dis = pl[0x100 / 4];
    bc[7] = ctrl_dis;
    for (i = 0; i < N; i++)
        blk[i] = 0x5C000000u ^ (i * 0x9E3779B9u) ^ 0x12345678u;
    for (i = 0; i < N; i++)
        if (blk[i] != (0x5C000000u ^ (i * 0x9E3779B9u) ^ 0x12345678u))
            mism_c++;
    bc[6] = 7;
    bc[9] = mism_c & 0xFFFF;
    bc[2] = (mism_a << 16) | (mism_c & 0xFFFF);

    /* phase D — re-enable, restore, return to QNX */
    st_en = mon_call(0x102, 1, 0);
    bc[6] = 8;
    bc[11] = st_en;
    ctrl_en = pl[0x100 / 4];
    *gicd = 1;                              /* GICD back on */
    bc[6] = 9;
    aux[1] = QNX_STARTUP1;                  /* CPU1 -> QNX startup */
    *rst = 1;
    *rst = 0;
    { uint32_t g = wdt[0x30 / 4]; wdt[0x30 / 4] = ~g; }
    bc[6] = 10;

    printf("B: disable st=%08x CTRL=%08x\n", st_dis, ctrl_dis);
    printf("C (L2 off): mism=%u/%u\n", mism_c, N);
    printf("D: enable  st=%08x CTRL=%08x\n", st_en, ctrl_en);
    {
        unsigned shown = 0;
        for (i = 0; i < N && shown < 6; i++) {
            uint32_t want = 0x5C000000u ^ (i * 0x9E3779B9u) ^ 0x12345678u;
            if (blk[i] != want) {
                printf("  [%3u] want=%08x got=%08x xor=%08x\n",
                       i, want, blk[i], want ^ blk[i]);
                shown++;
            }
        }
    }
    printf("L2TEST DONE: A=%u C=%u\n", mism_a, mism_c);
    bc[8] = ctrl_en;
    bc[1] = 45;
    return 0;
}

/* --l2lat: try to fix the PL310 tag/data latencies from NS. QNX left
 * 1-cycle latencies (the prime suspect for the session's silent memory
 * corruption and traffic wedges). If the NS writes stick, the payload
 * can set sane latencies pre-jump and the kernel's l2x0 driver inherits
 * a healthy controller. */
static int do_l2lat(void)
{
    volatile uint32_t *pl;
    uint32_t tag0, dat0, tag1, dat1;

    setvbuf(stdout, NULL, _IONBF, 0);
    bc = mapdev(BC_ADDR, 0x100);
    bc[0] = BC_MAGIC; bc[1] = 46; bc[5] = BC_MAGIC2;
    pl = mapdev(0x48242000ULL, 0x1000);
    tag0 = pl[0x108 / 4];
    dat0 = pl[0x10C / 4];
    printf("lat before: tag(0x108)=%08x data(0x10C)=%08x\n", tag0, dat0);
    pl[0x108 / 4] = 0x333;      /* tag latency: 3/3/3 cycles */
    pl[0x10C / 4] = 0x333;      /* data latency: 3/3/3 cycles */
    tag1 = pl[0x108 / 4];
    dat1 = pl[0x10C / 4];
    printf("lat after : tag=%08x data=%08x (%s)\n",
           tag1, dat1, (tag1 == 0x333 && dat1 == 0x333) ? "STUCK-SANE" : "NOT-WRITABLE");
    printf("aux=%08x prefetch=%08x ctrl=%08x\n",
           pl[0x104 / 4], pl[0x110 / 4], pl[0x100 / 4]);
    bc[2] = (tag1 == 0x333 && dat1 == 0x333) ? 1 : 0;
    bc[1] = 47;
    return 0;
}

/* --ppa: probe the PPA (Primary Protected Application) services of the TI
 * secure monitor. The monitor API has NO L2 tag/data-latency service (the
 * 0x100-0x113 table is exhausted — see docs/03 session 2026-09-01/02), but
 * the PPA services are a second, richer surface:
 *   0x21 PPA_SERVICE_0 (post-OSWR housekeeping, mainline)
 *   0x23 PPA_L2_POR    (L2 power-on-reset — a full SECURE L2 re-init; if it
 *                       touches the latencies, the corruption theory gets a
 *                       fix path: call it pre-jump, verify by readback)
 *   0x25 PPA_CPU_ACTRL_SMP (known-good PPA baseline call, mainline)
 *   0x26/0x27          (devpm-omap4's suspend pair — NOT in mainline; shape
 *                       lifted from devpm bf9c/bfd0: r0=idx r1=0 r2=4
 *                       r3=pargs r6=0xFF r12=0; pargs = one word = 1)
 * All calls use the devpm shape via ppa_call() (C-flow, user context — a bad
 * call at worst SIGSEGVs the payload or wedges the box until WDT2).
 * Readbacks: PL310 ctrl/aux/tag/data/prefetch before and after each call.
 * NS PL310 READS are safe (proven by --l2lat); nothing here WRITES PL310.
 * bc ladder: 48 armed, 49 after 0x25, 50 after 0x26, 51 after 0x27,
 *            52 after 0x23, 53 done. WDT2 mid-probe => bc[1] = the hung call.
 */
extern uint32_t ppa_call(uint32_t, uint32_t, uint32_t, uint32_t);

static void ppa_l2dump(const char *when)
{
    volatile uint32_t *pl = mapdev(0x48242000ULL, 0x1000);
    printf("L2 %s: ctrl=%08x aux=%08x tag=%08x data=%08x prefetch=%08x\n",
           when, pl[0x100 / 4], pl[0x104 / 4], pl[0x108 / 4],
           pl[0x10C / 4], pl[0x110 / 4]);
}

static int do_ppa(void)
{
    uint32_t *pargs_v, pargs_pa;
    off64_t phys = 0;
    size_t contig = 0;
    uint32_t r25, r26, r27, r23;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (ThreadCtl(_NTO_TCTL_IO_PRIV, 0) == -1)
        perror("IO_PRIV");
    bc = mapdev(BC_ADDR, 0x100);
    bc[0] = BC_MAGIC; bc[1] = 48; bc[5] = BC_MAGIC2;
    ppa_l2dump("before");

    /* phys-backed pargs word (secure world reads PHYS addresses) */
    pargs_v = mmap(0, 0x1000, PROT_READ | PROT_WRITE,
                   MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, 0);
    if (pargs_v == MAP_FAILED) { perror("mmap"); return 1; }
    mem_offset64(pargs_v, NOFD, 0x1000, &phys, &contig);
    pargs_v[0] = 1;                     /* devpm passes a single word = 1 */
    pargs_pa = (uint32_t)phys;
    printf("pargs_pa=%08x *pargs=%u\n", pargs_pa, pargs_v[0]);

    /* 0x25 = known-good baseline: does the PPA dispatch answer at all? */
    r25 = ppa_call(0x25, 0, 4, 0);
    printf("PPA 0x25 (CPU_ACTRL_SMP, pargs=0): ret=%08x\n", r25);
    bc[1] = 49; bc[2] = r25;

    /* devpm's pair, exact devpm shape (flag=4, pargs=&{1}) */
    r26 = ppa_call(0x26, 0, 4, pargs_pa);
    printf("PPA 0x26 (devpm suspend pair): ret=%08x\n", r26);
    bc[1] = 50; bc[3] = r26;
    ppa_l2dump("after 0x26");

    r27 = ppa_call(0x27, 0, 4, pargs_pa);
    printf("PPA 0x27 (devpm suspend pair): ret=%08x\n", r27);
    bc[1] = 51; bc[4] = r27;
    ppa_l2dump("after 0x27");

    /* L2 POR: the secure-side L2 re-init. If ANY reg changes here = win. */
    r23 = ppa_call(0x23, 0, 4, 0);
    printf("PPA 0x23 (L2_POR, pargs=0): ret=%08x\n", r23);
    bc[1] = 52; bc[6] = r23;
    ppa_l2dump("after 0x23");

    printf("PPA DONE: r25=%08x r26=%08x r27=%08x r23=%08x\n",
           r25, r26, r27, r23);
    bc[1] = 53;
    return 0;
}

int main(int argc, char **argv)
{
    /* --dmaquiet insurance: after devb is slain, a stdout flush to the
     * eMMC-backed jump.log would block forever. A 64KB buffer holds the
     * whole run's output in RAM. */
    setvbuf(stdout, NULL, _IOFBF, 65536);
    /* PlayBook W-86: --ledprobe = CLOSED (the direct I2C4 access
     * SIGBUSed 2026-09-12: fltno=5 at the first SYSCONFIG read, the
     * MMCHS secure-filter class; the box survived). The record lives in
     * docs/03 W-86a; the mode is removed. */
    if (argc > 1 && !strcmp(argv[1], "--ppa")) {
        return do_ppa();
    }
    if (argc > 1 && !strcmp(argv[1], "--l2lat")) {
        return do_l2lat();
    }
    if (argc > 1 && !strcmp(argv[1], "--l2test")) {
        return do_l2test();
    }
    if (argc > 1 && !strcmp(argv[1], "--l2on")) {
        g_l2on = 1;
        return do_t3(argc > 2 ? argv[2] : "/tmp/zImage",
                     argc > 3 ? argv[3] : "/tmp/omap4-winchester.dtb",
                     argc > 4 ? argv[4] : "/tmp/probe.bin");
    }
    if (argc > 1 && !strcmp(argv[1], "--dmaquiet")) {
        g_l2on = 1;
        g_dmaquiet = 1;
        return do_t3(argc > 2 ? argv[2] : "/tmp/zImage",
                     argc > 3 ? argv[3] : "/tmp/omap4-winchester.dtb",
                     argc > 4 ? argv[4] : "/tmp/probe.bin");
    }
    if (argc > 1 && !strcmp(argv[1], "--smcprobe")) {
        return do_smcprobe();
    }
    if (argc > 1 && !strcmp(argv[1], "--t2")) {
        return do_t2(argc > 2 ? argv[2] : "/tmp/hello.bin");
    }
    if (argc > 1 && !strcmp(argv[1], "--t3")) {
        return do_t3(argc > 2 ? argv[2] : "/tmp/zImage",
                     argc > 3 ? argv[3] : "/tmp/omap4-winchester.dtb", NULL);
    }
    if (argc > 1 && !strcmp(argv[1], "--probe")) {
        return do_t3(argc > 2 ? argv[2] : "/tmp/zImage",
                     argc > 3 ? argv[3] : "/tmp/omap4-winchester.dtb",
                     argc > 4 ? argv[4] : "/tmp/probe.bin");
    }
    if (argc > 1 && !strcmp(argv[1], "--hello")) {
        unsigned mc = argc > 3 ? (unsigned)strtoul(argv[3], NULL, 0) : 10;
        return do_hello(argc > 2 ? argv[2] : "/tmp/hello.bin", mc);
    }
    fprintf(stderr, "usage: qnx2linux --t2 [blob] | --t3 [zImage] [dtb] | "
                    "--hello [blob] [maxcount]\n"
                    "(run pinned to CPU0: on -C 0 ...)\n");
    return 1;
}
