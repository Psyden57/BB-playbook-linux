# Project State (as of session 11, 2026-09-11)

This file tracks the *current technical state* precisely. Older docs
(`docs/03`, `SESSION-HANDOFF/`) record how we got here; where they disagree
with this file, this file wins (and any unresolved disagreement is listed in
[contradictions/](contradictions/)). Per-session summaries live in
`newdocs/session-notes/session-NN.md`; the historical recaps are kept below
with dated headers.

## Where the boot stands

Mainline Linux 6.15.11 (omap2plus, non-LPAE, patched, **CONFIG_SMP=n as of
W-80**) is jumped from QNX via the **zImage path**, **--l2on (the L2
stays ON)** as of W-78:

1. The zImage decompressor delivers the appended DTB natively; the setup.c
   FDT-recovery chain works end-to-end (the machine-model line prints).
2. **THE CONSOLE IS ALIVE**: the earlycon ring carries the full early log
   (the machine model, the memory policy, the cma reservation, the PB
   prints) — alive since the session-4 era, silenced by the session-10
   L2-off era, restored by the W-78 L2-ON run.
3. **The CMA block passes**: the TLBIALL is SKIPPED (the W-72/W-83-proven
   pass) and the boot reaches dma_contiguous_remap-done (bc[1]=126).
4. **THE WEDGE FAMILY (the session-11 discovery)**: the machine wedges on
   SCU-routed global ops with CPU1 held — (a) the TLB maintenance ops
   (deterministic with the L2 off; flaky with the L2 on), and (b) the
   ldrex/strex exclusives (the spinlocks — deterministic with the L2 on +
   SMP=y; the first printk was the kill site; harmless with !SMP's plain
   spinlocks). RULED OUT with direct evidence: the barrier domain/encoding
   (the session-10 "dsb nosh" chain never existed on the hardware — GNU as
   rejects the name; GCC's IAS silently emitted the same full-system mcr;
   the W-68 f57ff062 literal = an ISB-class encoding with an invalid
   option), ACTLR.SMP/FW (all three states wedged), CPU1 parked vs held
   for the TLB ops (the W-73 park failed AND broke the post-reset
   recovery — the PRCM hold bit persists across warm resets and the
   released CPU1 resurrects QNX via the SAR path; THE PARK IS FORBIDDEN
   until the SAR neutralization + the kernel-side re-hold exist), the pgd
   content (the W-74 zeroing), the descriptor cacheability (the W-75
   strip — live and proven, and the "poison pair" = actually valid section
   descs in both shapes: the W-37 reading was an attribute-bit misread),
   the SCTLR cache state (the W-76 C/I=0), and the CP13 TLS writes (the
   W-63 class — the W-81 skip changed nothing at 150; the set_my_cpu_offset
   skip is KEPT as harmless).
5. **The current front = the first TLB op after the CMA (clear_fixmap in
   early_fixmap_shutdown)**, death bc[1]=126→125. The TLB ops need the
   CPU1 release (the bequest's SMP bring-up) or a payload-flow fix — the
   runs 23-32 era (the old --t3 payload) is the only era where TLB ops
   ever completed; the era's payload + the current kernel = the next
   session's opening bisect (the git archaeology).
6. The DMA masters are quiesced in --dmaquiet; **--l2on is the run mode
   now** (the L2 stays on — --dmaquiet's L2-off = the deterministic TLB-op
   wedge).
7. Marker-number discipline: setup.c's pb_bc(130-136) pairs COLLIDE with
   mmu.c's PB_MMU_BC numbers — discriminate via the mirror channel (rule
   17 in docs/README). Extended forensics bc[16]-bc[27] + the W-72 pgd-dump
   slots bc[28..31] (0x90000070-7C) via `memdump3 90000040 0x40`.

## What was fixed, by session

- **Session 7 (2026-09-03) — the bc=127 wall**: the runtime-PHYS_OFFSET/
  DTS-bank mismatch (bottom-up memblock allocs below PHYS_OFFSET →
  `__phys_to_virt` under TASK_SIZE → the "in user region" BUG). Fixes:
  the 129-slot placement sweep, `kern_off` placement,
  `fdt_patch_memory`, the PB-CMA print, and the DTS bank baked to
  match zreladdr. [UPDATE 2026-09-04 (W-21): the bank = 0xa0000000 +
  512MB — the old 0xa4000000+448MB default put the kernel outside its
  own memory.]
- **Session 8 (2026-09-04) — the fixup delivery paradox**: the called
  `__fixup_pv_table` never executed via ANY mechanism; INLINING it into
  head.S's streamed region (W-17) broke the wall — the boot reached the
  C world (bc=127/146-era). The paradox mechanism remains open
  (KNOWN_ISSUES #10); the rule stands: MMU-off helpers are INLINED,
  never called. Also session 8: the bank fix (W-21), the surviving-slot
  markers (W-22), the dual-level pv invalidate (W-24 — later superseded).
- **Session 9 (2026-09-04) — the pv cure + the stale pgd pair**: the
  "wandering" early-C deaths (svm alloc W-25/31/32a, FDT walk stack
  smash W-26, map_lowmem pte alloc W-27, MMU-enable W-29, head.S tail
  W-35) all resolved to (a) the stale-pv regime (CURED) and (b) the
  stale pgd pair (workaround in #110). The DMA quiesce (--dmaquiet)
  did NOT cure the randomness → the corruption class = stale-view
  (characterized in contradictions/), not a rogue DMA master. The
  swipe/tap interaction variables were exonerated (W-32a hands-off =
  byte-identical death). The placement-overlap guard came from W-35.

## The DTB delivery issue (Image path — PARKED)

On the uncompressed-Image path, the kernel reports
`Warning: Neither atags nor dtb found` twice and falls back to a 16 MB
memblock region — yet both the payload (REVERIFY) and the probe validate
the DTB at `params[1]` before the jump. The vet constant is exonerated
(`head-common.S` defines `OF_DT_MAGIC 0xedfe0dd0` for LE builds) and the
register chain was correct at probe time at least once. **Parked in
favor of the zImage path** (D6); investigate only if the zImage path
ever fails.

## Next run (W-39): build #110 — the 2MB allocator shave

W-38's pmd pattern: the pair [0xdfc/0xdfd] = IDENTICAL bogus table
pointers (0xbfc1141e, both halves = the __pmd_populate signature),
DETERMINISTIC across runs AND placements; [0xdf8] = a CORRECT section
desc (0xbf81141e — map_lowmem's mapping of the range is healthy, only
the LAST pair is stale); [0xdfe+]=0 ✓; [0xdf0/0xdf4]=0 (CMA-cleared ✓).
Verdict: the last pair of the linear map holds STALE QNX-era page-table
content (0xbfc11400 = a QNX-era pte table for that DRAM region) — the
write-back-loss class, same as the pv variables — NOT a random wild
write. Build #110: arm_lowmem_limit -= 2MB after map_kernel (nothing
allocated in the poisoned top; the svm lands at ~0xbfbfffd4, pair
[0xdfa/0xdfb] — the previously-untested middle pair), stores re-enabled
with markers 167 (dumps) / 0x567 (store 1) / 151 (memset done), bc[19]
= readback, bc[25] = VA 0xdfa pmd. Expectations: if the middle pair is
healthy → the memset completes → the boot advances (toward the 171
wall, KNOWN_ISSUES #3 l2x0 hazards ahead); if the middle pair is ALSO
stale → the stale region is wider → extend the shave or self-heal the
pgd (the W-32c store+clean pattern applied to the pmd). Marker-number
audit: setup.c's pb_bc(130-136) pairs COLLIDE with mmu.c's PB_MMU_BC
numbers — discriminate via the mirror channel. See docs/03 W-25..W-38
and SESSION-HANDOFF/BOOTSTRAP_SESSION_10.md.

## The secure monitor — RE closed (session 7)

- No QNX binary contains the SMC dispatch (`trustzone-omap4` = the
  /dev/trustzone crypto resmgr; `libsecure_dispatcher` = crypto/KDS;
  `procnto` = 0 SMCs). The monitor is the TI ROM monitor.
- The documented service table (0x100 L2X0 DBG_CTRL, 0x102 L2X0 CTRL,
  0x101 L2 clean+inv by PA, 0x108 SCU_PWR/suspend, 0x109 L2X0 AUXCTRL,
  0x113 L2X0 PREFETCH) — **ERRATUM 2026-09-11 (bootrom RE, TRM §27.5
  Table 27-61): the claim "no tag/data-latency service exists" was
  WRONG — 0x112 writes the PL310 Tag AND Data RAM Latency registers
  (r0 = tag, r1 = data). The ROM itself never touches PL310 directly
  (0 direct L2 constants in the dump); L2 work routes through these
  services. 0x112 = untested on this HS unit (rejection risk like the
  PPA 0x25/0x23 pair) but one mon_call + readback answers it. See
  bootdumps-2026-09-11/BOOTROM-RE.md and
  newdocs/audit-approach-2026-09-11.md.**
- PPA probe (run PPA-1): 0x25 and 0x23 rejected (0xFF02), 0x26/0x27
  (devpm's suspend pair) accepted with no PL310 readback change.
  Secure-side L2 reconfiguration is **exhausted** as a fix path.
- New lead: the eMMC secure-boot chain (user-area sectors 5-240) contains
  the QNX initial loader with NS-side SMC wrappers for **0x112** (×2) — an
  undocumented service. Not yet semantically identified. See
  `newdocs/session-notes/session-07.md`.

## Kernel-side known hazards (ahead of the boot)

When the boot first reaches `l2x0_of_init` (init_IRQ):
- `l2c_enable` does a by-way write to `L2X0_INV_WAY` (0x7FC) — the
  on-device DEADLOCK op (NS by-way ops wedge the machine).
- `l2c_wait_mask` introduces a poll loop.
- The DTB latency writes are safe (routed through
  `omap4_l2c310_write_sec` → default WARN+skip).
Patch `l2c_enable` or run `CONFIG_CACHE_L2X0=n` (current config has it on)
when the boot gets there.

## Operational state

- Payload in `kexec/` (see [../kexec/README.md](../kexec/README.md)):
  the placement sweep + guard, kern_off, DTB patching, the WDT2
  lifecycle, the DISPC kill, the devb slay; modes
  `--dmaquiet/--l2on/--t3/--probe/--ppa/--l2lat/--l2test` —
  **--dmaquiet is the run default (session 9)**.
- Kernel tree: `/home/psyden/kernel/linux` (6.15.11, config recoverable
  from any built Image via `scripts/extract-ikconfig`); the repo's
  kernel state = `kernel-patches/` (regenerate after every change).
- Device: SSH root@169.254.0.1, key at `playbook-dev/rsa` (local-only,
  never commit). WDT2 warm-reset cycle ~59 s. LED sequence observable
  (user video-records runs; the device clock = GMT-3, the host = UTC).
