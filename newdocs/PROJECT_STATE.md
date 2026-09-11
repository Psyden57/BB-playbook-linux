# Project State (as of session 11, 2026-09-11)

This file tracks the *current technical state* precisely. Older docs
(`docs/03`, `SESSION-HANDOFF/`) record how we got here; where they disagree
with this file, this file wins (and any unresolved disagreement is listed in
[contradictions/](contradictions/)). Per-session summaries live in
`newdocs/session-notes/session-NN.md`; the historical recaps are kept below
with dated headers.

## Where the boot stands

Mainline Linux 6.15.11 (omap2plus, non-LPAE, patched for the PlayBook) is
jumped from QNX via the **zImage path** (the L2 is turned OFF by the
payload inside the proven --dmaquiet shape — mon_call(0x102), verified
by bc[8]=0 in W-69):

1. The zImage decompressor inflates the kernel to zreladdr 0xa0008000,
   delivers the appended DTB natively (bc[20..23] markers), and the
   payload's placement sweep reserves [0xa0000000, 0xa1000000).
2. head.S runs (the inline pv fixup, markers 143/144), MMU-on completes,
   and the boot reaches the C world with correct pv state (the W-32c
   block, tries=0).
3. **The setup.c FDT-recovery chain works end-to-end (W-69, bc[27]=
   0xF5000002 "post fdt call")**: setup.c recovers the FDT pointer from
   bc[20] when __atags_pointer=0 — the W-64 cure. The image-region CIPA
   sweep self-verify is clean (bc[26]=0xBEEF0000, zero mismatches).
4. **The front (W-69) = the CMA remap's TLBIALL block** — bc[1]=145, the
   death between marker 145 and 146. SESSION-11's rule-16 audit
   DISPROVED the session-10 "dsb nosh" chain (see the W-69 record in
   docs/03): GNU as rejects `dsb nosh` (the valid name = `nsh`); GCC's
   integrated assembler silently accepted it and emitted the same
   full-system mcr encoding — the conversions were NO-OPS on the
   hardware; and the W-68 literal f57ff062 = an ISB-class encoding with
   an invalid option = UNPREDICTABLE on the A9 = the W-69 death site
   (c0f07e40). **The mechanism: the A9 ACTLR bit 0 = FW = "cache and
   TLB maintenance broadcast" — every prior clear touched only bit 6,
   so every TLB op broadcast to the HELD CPU1 via the SCU regardless of
   encoding (W-44 re-explained).**
5. **Build #140 (W-70) = the batch**: all nosh reverted to plain dsb sy
   (restore-to-known-good), the f57ff062 literals fixed to f57ff04f,
   and BOTH ACTLR sites clear ~0x41 (SMP+FW) — bc[3]=0x0 = the
   discriminator. The L2-off run mode keeps the old SMP=0 cached-write
   wedge (the 2026-09-02 L2-ON-era class) out of the picture.
6. The DMA masters are quiesced (--dmaquiet: devb slain, DISPC killed +
   register-verified 0/0; WiFi SDIO never brought up).
7. Marker-number discipline: setup.c's pb_bc(130-136) pairs COLLIDE
   with mmu.c's PB_MMU_BC numbers — discriminate via the mirror channel
   (rule 17 in docs/README). Extended forensics slots bc[16]-bc[27]
   (0x90000040+, `memdump3 90000040 0x30`).

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
