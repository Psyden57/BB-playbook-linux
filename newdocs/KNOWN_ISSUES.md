# Known Issues & Open Problems

Current as of session 7 (2026-09-03). Ordered by blocking priority.

## 1. bc=171 wall — taskstats_init_early / kmem_cache_create

The boot's current death point. Reached twice (W-4 with probe: bc=171;
W-5 without: bc=126 — the no-DTB 16 MB fallback shifts the death point).
Session 6 analyzed this as "slab_mutex corruption"; that analysis predates
the placement/DTB discoveries and may not apply. **Untested with a working
DTB + full memory** — the zImage run comes first.

Era note (session-5 backfill): runs 23–32 (UNCACHED kernel, L2-off, DTB +
512 MB) also died at exactly 171, and session 5's finer markers
(181/182 inside taskstats_init_early, 183–185 inside
__kmem_cache_create_args) never landed — placing that era's death inside
kmem_cache_create, before the mutex unlock. The co-occurring **bc[2]=0x3E7
(999) wild write** was never explained: it is not the pb_bc pair (which
writes bc[2]=427), not PB_MMU_BC (would need v=0x2E7, no such marker), and
the console's ring2 char window (PA 0x90000100+) cannot reach bc[2]
(0x90000008) with a bounded index. If it recurs, dump bc[0x80–0x8F] (the
console's ring2 header region at the same time) — a corrupted ring2 index
producing a wild strb is the surviving candidate (see
session-notes/session-05.md §analyses).

**UPDATE 2026-09-04 (W-6): 0x3E7 is BACK — and it is zImage-correlated.**
Run W-6 (the first zImage run of the session-7 kernel) returned bc[2]=0x3E7
exactly, while the Image-path runs (W-4/W-5) showed other values
(0x163/0x17e). Runs 23-32 were also zImage. Two candidate readings: (a) a
writer specific to the zImage environment (the decompressor's
cache-clean/decompress traffic interacting with DRAM), or (b) 0x3E7 is a
legitimate kernel/payload value for a code path only the zImage reaches.
bc[2]'s writer remains unidentified. On the next zImage run, dump
bc[0x80–0x8F] alongside as planned.

## 2. DTB delivery broken on the uncompressed-Image path (parked)

`Warning: Neither atags nor dtb found` ×2 despite a verified-correct
`params[1]` (W-4: address math closed exactly; probe validated magic;
REVERIFY passed). Somewhere between the cont's `r2 = params[1]` and the
kernel's `__vet_atags` the value is lost. The 1 GB probe loop is
exonerated (W-5 lost it too). Parked in favor of the zImage path
(DECISIONS D6); investigate only if zImage ever fails.

## 3. l2x0_of_init hazards (will fire when the boot reaches init_IRQ)

- `l2c_enable` (arch/arm/mm/cache-l2x0.c) writes `L2X0_INV_WAY` (0x7FC) —
  the by-way op that DEADLOCKS this machine from NS.
- `l2c_wait_mask` = a poll loop on the sync register.
- The DTB latency writes are safe (routed through `omap4_l2c310_write_sec`
  → default WARN+skip; the old "guard the driver against NS latency
  SIGBUS" advice is a no-op).
Fix: patch `l2c_enable` to skip the by-way op on winchester, or
`CONFIG_CACHE_L2X0=n` (keep QNX's L2 setup). Current config has L2X0 on.

## 4. The unpatched-pv-stub signature (observed once, mechanism unknown)

Session 6's "barrier stack struct corruption" (0xfe600000 → 0x1f7f0000) and
one W-4-era reading both match the **unpatched pv-stub placeholder delta**
(0x81810000 = `__PV_BITS_31_24` + `__PV_BITS_23_16`): an unpatched pv site
computes `__phys_to_virt(x) = x - 0x81810000`. The pv fixup code is
byte-identical to mainline; how a site could stay unpatched is unknown.
Not reproduced since the placement fixes. The `PB-CMA` print (kernel) will
catch it (variable correct + VA wrong → site unpatched).
Correction (session-6 context, see session-06.md): the 0x1f7f0000 BUG was
`dma_contiguous_remap`, not the barrier (the barrier-disabled run reproduced
it identically) — and the remap's base ALSO read wrong (0xa1000000 vs the
printed CMA at 0xbe800000), so either two independent anomalies or one wild
write hit both fields.

## 5. Console output stops mid-boot (secondary)

In W-4/W-5 the ring1 capture ends right after the `PB-ADJ` print while the
bc ladder continued (126/171). Something silences earlycon/printascii after
the memblock prints. Not blocking yet (breadcrumbs still work); investigate
when the boot goes deeper.

## 6. SMC 0x112 — undocumented monitor service

The QNX initial loader (eMMC secure-boot chain) contains NS-side SMC
wrappers for 0x112 (two shapes). Semantics unknown; two candidate readings:
an L2-adjacent service (the observed QNX latency value 0x111 is suspicious)
or something else entirely. RE of `bootblob_arm9000.bin` pending.

## 7. W-4/W-2 "corrupted" bc tails after reboot

After a reboot the bc page's tail (bc[6]+) holds garbage while magic +
bc[1] survive — QNX's own boot tramples DRAM. Mirror pages can be zeroed.
Always verify magic + nonce before trusting a readback.

Session-4 addendum (2026-09-01): the trap is older and subtler — **bc_arm
sanitizes only bc[0..5] + the ring headers**, so bc[6] (the probe's DTB-magic
canary 0xedfe0dd0) and bc[7] (the probe's PL310 cache-id 0x410000c4) SURVIVE
from previous runs and look like fresh evidence. A 2026-09-01 run was
briefly misread this way (the probe appeared to have run when it had not —
bc[4] was the tell: it held the payload's value, not the probe's dtb_phys).
Also: bc[2] reads 0x60540100 in any pre-probe dump (the probe's raw LE load
of the big-endian DTB totalsize field) — it is probe residue, not kernel
output. And bc[12] (0x90000030) holds dtb_phys with no identified writer.
Distrust everything past bc[5] unless this run's code demonstrably wrote it.

Session-2 backfill (context of 2026-08-31, code-verified 2026-09-03):
bc[12]'s writer IS identified — the cont's step-211 marker
(`stub3.S`: `str r7, [r11, #0x30]`) records the enter_stub r7 chain value,
which is dtb_phys whenever GCC's register pinning held for that run; it is
chain forensics, not fresh-DTB proof (the chain is unreliable — GCC reused
r7 as a loop counter in one build; the params block is authoritative). And
bc[2] is a validity check, not garbage: FDT header fields are big-endian
per spec — 0x60540100 = BE 0x00015460 = 87136 = the exact DTB file size
(see session-notes/session-02.md CONFIRMED #5-6).

## 8. Miscellaneous

- The payload requires root + `ThreadCtl(_NTO_TCTL_IO_PRIV)`; all device
  work assumes the rooted 2.0.0.4869 WiFi-only unit.
- The eMMC rootfs path (`root=/dev/mmcblk0p2`) is in the cmdline but the
  kernel has no rootfs yet (T4, not started).
- `qnx-env.sh` hardcodes `/home/psyden` paths (machine-specific, benign).

## 9. The TTBR0 walk-attribute strip in head.S is standing and untested (session 3)

`__enable_mmu` contains `bic r4, r4, #0x6A` — it strips TTB_FLAGS_SMP
(S|NOS|RGN|IRGN: shareable + cacheable-WBWA PTW-read attributes) that
`v7_ttb_setup` OR'd into r4 in place. Rationale (session 3, 2026-08-31,
never disproven): a *shareable* PTW read goes through the SCU and snoops
CPU1, which D2 holds in PRCM warm reset and which cannot respond — a
theoretical deadlock on the very first walk after MMU-on. Every session-3
test of that theory was confounded by the ring-map shift bug; the ring-map
fix (session 4) unlocked M=1 *with the strip still in place*, so nobody has
observed whether shareable walks actually deadlock on this machine or not.
Removing the strip for one run is a cheap experiment: if shareable walks
are fine, the strip is dead weight (and the non-cacheable walk it forces is
slower than a WBWA walk-cache-able one would be). If the boot wedges at M=1
without it, the strip is load-bearing and the D2 decision (CPU1 in reset
vs parked alive) needs revisiting for SMP coherence reasons.
