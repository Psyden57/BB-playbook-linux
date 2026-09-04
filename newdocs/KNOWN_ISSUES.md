# Known Issues & Open Problems

Current as of session 9 (2026-09-05). Ordered by blocking priority.
Session-9 updates are appended per issue; the session-7-era text stands
for context. See docs/03 W-25..W-38 and
newdocs/session-notes/session-09.md.

## 1. bc=171 wall — taskstats_init_early / kmem_cache_create

**SESSION-9 UPDATE (2026-09-05): the 171 wall is NOT the front anymore.**
The boots now die BEFORE it, and the randomness is root-caused: the
stale-pv regime (see #4's update) broke every C-world __va/__pa inline,
producing the "wandering" deaths (svm alloc W-25/31/32a, FDT walk
W-26, map_lowmem pte alloc W-27, MMU-enable W-29). The pv regime is now
CURED (W-32c direct store, tries=0, deterministic). The remaining wall =
the stale pgd pair (see #4b) — the 171 wall resumes after the boot
passes it. The old taskstats analysis predates all of this.

**0x3E7 update**: it returned in the head.S-era deaths (W-29's 121,
W-35's 142). The bc[0x80-0x8F] dump found only sanitized headers +
residue. Writer still unidentified.

## 2. DTB delivery broken on the uncompressed-Image path (parked)

Unchanged — parked in favor of the zImage path (DECISIONS D6).

## 3. l2x0_of_init hazards (will fire when the boot reaches init_IRQ)

Unchanged: the by-way 0x7FC deadlock risk vs CONFIG_CACHE_L2X0=n.
**Session-9 note: the boot has never reached init_IRQ this session —
this is the NEXT hazard after the paging region is passed.**

## 4. The unpatched-pv-stub signature — **SOLVED AS A CLASS (2026-09-05)**

**SESSION-9 UPDATE: root-caused and cured.** The stale reads came from
decompressor-era L2 lines surviving eviction (per-run luck): the
fixup's MMU-off SO stores reach DRAM but not L2, so the C world's
cached reads hit the stale line. The old W-24 fix was self-defeating
(its own __pa() consumed the stale pv it repaired; SMC 0x101's clean
step then poisons DRAM with the stale line). THE FIX (build #106,
"the W-32c block" in start_kernel): directly store the build-time
constants (offset 0xffffffffe0000000, pfn 0xa0000) and DCCIMVAC — the
clean carries the correct value through the L2C. Deterministic
(tries=0, bc[19] slot 0xD000004C = retry count). KEEP.

### 4b. NEW — the stale pgd pair (the front as of W-38)

The pair [VA 0xdfc/0xdfd] = PA [0xbfc00000/0xbfd00000] — the LAST mapped
pair of the linear map, the final populated cache line of
swapper_pg_dir — reads as IDENTICAL bogus TABLE descriptors
(0xbfc1141e → a table at 0xbfc11400 = a never-allocated QNX-era pte
table for that DRAM region). Deterministic across runs AND kernel
placements. map_lowmem's section write for that pair doesn't survive in
the PTW's view ([0xdf8] = a CORRECT section desc, 0xbf81141e). Any
allocation/access in the top 2MB of the linear map wedges on the walk
(the svm deaths W-25/31/32a/34/36/37). Build #110 shaves
arm_lowmem_limit by 2MB (nothing allocated in the poisoned top);
untested as of this note. Longer-term: self-heal the pgd (the W-32c
store+clean pattern: write the expected section desc + DCCIMVAC before
any access through it).

## 5. Console output stops mid-boot (secondary)

**SESSION-9 UPDATE: the "console death" was the pv-stale regime's
symptom.** With pv correct (W-34+), the console prints through PB-CMA
normally. printk demonstrably alive at the W-26 panic (the panic line
itself printed). Re-open only if a fresh instance appears.

## 6. SMC 0x112 — undocumented monitor service

Unchanged.

## 7. W-4/W-2 "corrupted" bc tails after reboot

Unchanged (always verify magic + nonce).

## 8. Miscellaneous

- **NEW (W-28): MMCHS/device-register access = NOT NS-accessible**
  (SIGBUS fltno=5 on reads, 0x480B4000+), AND that abort class FROZE the
  box (SSH dead, display dead — power-hold required). Treat as
  device-wedging, not a cheap userland crash.
- **NEW: DMA-master quiesce = --dmaquiet (payload)**: `system("slay -f
  devb-mmcsd-winchester")` after the last file read (rc → bc[14] =
  0xD1EBxxxx; rc 0x100 but devb demonstrably dead); DISPC killed at bc
  34 + register-verified (bc[16]/bc[17] = 0/0); WiFi SDIO never brought
  up (DTS). The randomness SURVIVED the quiesce — the source is the
  stale-view class (#4), not a rogue DMA master.
- **NEW: the placement guard**: buf_placement_bad reserves
  [0xa0000000, 0xa1000000) for the zreladdr inflation region (W-35: the
  only overlapping placement ever → head.S-tail death, bc[1]=142).
- The payload requires root + `ThreadCtl(_NTO_TCTL_IO_PRIV)`; all device
  work assumes the rooted 2.0.0.4869 WiFi-only unit.
- The eMMC rootfs path (`root=/dev/mmcblk0p2`) is in the cmdline but the
  kernel has no rootfs yet (T4, not started).
- `qnx-env.sh` hardcodes `/home/psyden` paths (machine-specific, benign).

## 9. The TTBR0 walk-attribute strip in head.S (session 3)

Unchanged (standing, untested).

## 10. The MMU-off delivery paradox (session 8)

Unchanged as a mechanism mystery; the INLINE-don't-call rule stands.

## 11. D-side stale lines after the decompressor (session 8)

**SESSION-9 UPDATE: generalized and confirmed as THE root class.** The
stale lines bite (a) the pv variables — CURED by the W-32c direct store
(build #106), and (b) the swapper_pg_dir's last pair (4b). The pbmark
CIPA machinery remains dead weight for the bc page (W-16, session 8).

## Historical detail (pre-session-9 era notes and backfills)

### 1. bc=171 wall — taskstats_init_early / kmem_cache_create

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

### 2. DTB delivery broken on the uncompressed-Image path (parked)

`Warning: Neither atags nor dtb found` ×2 despite a verified-correct
`params[1]` (W-4: address math closed exactly; probe validated magic;
REVERIFY passed). Somewhere between the cont's `r2 = params[1]` and the
kernel's `__vet_atags` the value is lost. The 1 GB probe loop is
exonerated (W-5 lost it too). Parked in favor of the zImage path
(DECISIONS D6); investigate only if zImage ever fails.

### 3. l2x0_of_init hazards (will fire when the boot reaches init_IRQ)

- `l2c_enable` (arch/arm/mm/cache-l2x0.c) writes `L2X0_INV_WAY` (0x7FC) —
  the by-way op that DEADLOCKS this machine from NS.
- `l2c_wait_mask` = a poll loop on the sync register.
- The DTB latency writes are safe (routed through `omap4_l2c310_write_sec`
  → default WARN+skip; the old "guard the driver against NS latency
  SIGBUS" advice is a no-op).
Fix: patch `l2c_enable` to skip the by-way op on winchester, or
`CONFIG_CACHE_L2X0=n` (keep QNX's L2 setup). Current config has L2X0 on.

### 4. The unpatched-pv-stub signature (observed once, mechanism unknown)

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

### 5. Console output stops mid-boot (secondary)

In W-4/W-5 the ring1 capture ends right after the `PB-ADJ` print while the
bc ladder continued (126/171). Something silences earlycon/printascii after
the memblock prints. Not blocking yet (breadcrumbs still work); investigate
when the boot goes deeper.

### 6. SMC 0x112 — undocumented monitor service

The QNX initial loader (eMMC secure-boot chain) contains NS-side SMC
wrappers for 0x112 (two shapes). Semantics unknown; two candidate readings:
an L2-adjacent service (the observed QNX latency value 0x111 is suspicious)
or something else entirely. RE of `bootblob_arm9000.bin` pending.

### 7. W-4/W-2 "corrupted" bc tails after reboot

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

### 8. Miscellaneous (pre-session-9)

- The payload requires root + `ThreadCtl(_NTO_TCTL_IO_PRIV)`; all device
  work assumes the rooted 2.0.0.4869 WiFi-only unit.
- The eMMC rootfs path (`root=/dev/mmcblk0p2`) is in the cmdline but the
  kernel has no rootfs yet (T4, not started).
- `qnx-env.sh` hardcodes `/home/psyden` paths (machine-specific, benign).

### 9. The TTBR0 walk-attribute strip in head.S is standing and untested (session 3)

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

### 10. The MMU-off delivery paradox (session 8 — mechanism UNKNOWN)

The called `__fixup_pv_table` never executed a single instruction on the
zImage path: not via `bl`, not via a computed `blx` with a
post-mortem-verified target (bc[13]), invariant to L2 on/off, SCTLR.I,
probe presence, smoke test, and CIPA ops — with byte-correct DRAM
(W-13's dump) and the fixup's first I-cache line shared with
`__vet_atags`'s executed tail. Bypassed by INLINING the fixup into
head.S's streamed region (W-17). The inlined copy is itself
build-luck-dependent (#95/#96 worked untouched; #97's pv went dead until
W-24's C-world invalidate). RULE: never CALL MMU-off helpers on this
path — INLINE them. Mechanism: open.

### 11. D-side stale lines after the decompressor (session 8 — mechanism
identified, fix in place, generalization untested)

The decompressor writes the image cached; its cache_clean_flush cleans
L1→L2 leaving VALID L2 lines with image-era content. The kernel's
MMU-off SO stores (the inline pv fixup) reach DRAM (post-mortem-verified,
W-23) but the C world's CACHED reads hit the stale lines (pv_off=0,
W-21/22/23). The head.S L2-only CIPA did not cure it; the C-world
dual-level invalidate (L1 DCCIMVAC by VA + the monitor SMC 0x101 L2
clean+inv by PA — the W-24 block in map_lowmem) DOES (W-24's console =
all-correct pv values, no BUG). OPEN: the fix currently covers only the
two pv variables — if more decompressor-era stale D-side lines bite
(any structure the fixup or early C writes that the C world reads
cached), extend the W-24 block or consider a wider L2 invalidate.
NOTE: the earlier "SO stores only dirty the L2 / QNX discards them"
model (the W-8-era pbmark CIPA rationale) is FALSIFIED for the bc page
(W-16: bc markers survive with no flush); the pbmark CIPA machinery is
dead weight there.
