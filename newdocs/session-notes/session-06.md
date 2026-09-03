# Session 6 Notes (2026-09-02 evening → night)

Written at the start of session 7 from the session-6 conversation context.
Session 6 ran ~20 device jumps and pivoted the whole project (L2-off retired,
console solved, the corruption root cause pinned — then partially retracted
by session 7, see contradictions/). The docs/03 "2026-09-02 Evening" section
and HANDOFF_2026-09-02_night have the conclusions; this file holds the
run-by-run narrative, the numbers, and three corrections that only the
session-6 context can make.

## The run-by-run narrative (numbers from the actual readbacks)

1. **Console-visibility run** (the era's task 1): the earlyprintk handler
   never ran (bc[10]=0xaaaaaaaa), boot sailed to 171. ring3's monitor count
   frozen at 23061 (run 23's total). First hint that something was
   placement/state-dependent, not deterministic-code.
2. **Parse-probe run after a HARD reboot** (user power-cycled; DRAM wiped):
   totally different behavior — handler RAN, cmdline length 73 (bc[13]),
   died INSIDE the first console write (bc[12] junk, ring1 count still 20).
   Lesson: stale DRAM was masking/skewing entire code paths between runs.
3. **The ring1-count=20 identification**: 0x14 = exactly the head.S smoke
   test's char count ("PLAYBOOK-HEAD-101\r\n" = 19 + 'K' = 20). Counting
   characters against known writers identified stale vs fresh content more
   reliably than the nonce.
4. **The mirror0=70 alarm was false**: grep of qnx2linux.c showed `mov r0,
   #70; str; blx r6` = "jump started", and the 71 writer no longer exists.
   70-without-71 is NORMAL (documented in docs/04 after this).
5. **--l2test A/B (3 payload-only runs, no jump)**: phase A (L2 on, 2048
   device stores + verify) clean; phase C (L2 off) wedged INSIDE the loop
   (bc[6] step 6 landed, 7 never). First run hung at the 0x102 disable
   itself — root cause found in do_t3's comments: **0x102 hangs correlate
   with IRQs live mid-SMC; the GICD-off before the SMC is the guard**. One
   attempt SIGSEGV'd at ref=0x768 = the PL310 CIPA offset with a NULL base
   (`pl310_ns` never set — do_t3 sets it, the first l2test didn't).
6. **The L2-on pivot** ("the endgame gets promoted"): every --l2on boot then
   died at bc[1]=120 with the identical 125 marker right behind it never
   landing — two identical macros, back to back. Root cause: the head.S
   C/B/S strip (an M=1-era diagnostic never reverted) left the whole kernel
   UNCACHED; the `__mmap_switched` bss clear = megabytes of SO stores through
   the L2 = wedge. Removing ACTLR.SMP clears did NOT fix it (tested); removing
   the strip did — but then the boot died at 132 (the UART3 THR store: a
   posted store to the dead/gated UART3 stalls the store buffer; the next
   ring load hangs). **Rings-only console** (senduart + LSR drain removed)
   got the boot to 133 (console registered, cmdline 102 via
   CONFIG_CMDLINE_FORCE), then the memblock limit-0 panic (adjust_lowmem_bounds
   computed 0; forced in arm_memblock_init), then bc=127 with the
   `0x1f7f0000` BUG (see the correction below), then the l2lat SIGBUS proof.
7. **The l2lat SIGBUS asymmetry**: reads of PL310 0x108/0x10C work (0x0 /
   0x111 — the data latency = three 3-bit fields, 1 cycle each); the WRITE =
   SIGBUS with ref = the payload's mapdev VA + 0x108 (QNX converting the
   external abort). Precise: reads fine, writes abort, secure-filtered.
8. **The corruption signatures seen mid-session** (later reinterpreted, see
   the correction below): ring1 console text with |0x80-stuck bytes
   ('.'→0xAE, ' '→0xA0 ...) while ring2 carried DIFFERENT damage of the same
   text (a cleared bit, a substituted char) — read at the time as "corruption
   after the stores, in the memory subsystem". The 0x1f7f0000 mapping-BUG
   value and the 999-in-bc[2] pattern also fed this. Session 7 resolved most
   of it (see contradictions/).
9. **The pristine-tree diff session**: extracted `/home/psyden/kernel/
   pristine` and diffed — 12 modified + 2 new hand-written files vs upstream,
   and nothing in the diff explained the dead earlyprintk handler (which led
   to the parse-probe design instead of more guessing).
10. **Session end**: CONFIG_CACHE_L2X0=y + the pl310 DTS node added; the
    --l2lat proof closed the NS fix path; docs overhauled; the bootstrap
    written.

## CORRECTIONS to session 6's own records (made with native context)

1. **"23 KB boot log" is misleading.** Run 23's recovered ring3 content =
   ~15 real lines ending at the `OMAP4: Map ... dram barrier` print + a
   partial "r." — the other ~22 KB = 0x55/0xaa filler. There is NO "Kernel
   command line:" banner in it. So run 23's console died mid-setup_arch
   while its bc ladder reached 171 — the console death and the kernel death
   were DIFFERENT events, and session 6 conflated them.
2. **The barrier-stack-struct attribution was WRONG.** Session 6's records
   (docs/03 night, the omap4-common.c comment) say the dram-barrier mapping's
   stack struct was corrupted (0xfe600000 → 0x1f7f0000). But the run with
   `omap_barriers_init` DISABLED reproduced the IDENTICAL BUG — proving the
   mapping was never the barrier: it is `dma_contiguous_remap` (which sits
   exactly between markers 127 and 126). The barrier was disabled after the
   BUG was already there. Session 7's unpatched-pv-stub reading of the
   0x1f7f0000 value is consistent with this; the barrier comment in
   omap4-common.c and docs/03 should be read as a misattribution.
3. **The 171-era DTB/memory state, per era** (this matters for
   contradictions/171-wall-analyses.md):
   - Run 23 (kernel #51, zImage, L2-off, UNCACHED kernel): DTB present
     ("Machine model" printed), memory trimmed to 0xa0000000-0xC0000000,
     CMA reserved at 0xbe800000 — **a zImage + DTB + full-memory boot that
     died at 171 already happened in session 6**. The difference vs session
     7's W-4: run 23 ran the UNCACHED (stripped) kernel.
   - Runs 24-32 (L2-off, UNCACHED): DTB state unknown (console dead).
   - Session-6 L2-on runs: never reached 171 (died at 127/132).
   So the era matrix is: {uncached, L2-off, DTB+full-mem} → 171 (run 23);
   {cacheable, L2-on, DTB+full-mem} → 127 (session 6 late, pre-fix);
   {cacheable, L2-on, 16 MB no-DTB} → 171 (W-4); {cacheable, L2-off,
   16 MB no-DTB} → 126 (W-5). The untested cell = {cacheable, L2-on,
   DTB+full-mem} — exactly the session-7 zImage run.

## The device-op cliff calibration (session 6 measurements)

With the L2 ON (QNX's config), sustained DEVICE (SO) stores wedge the
machine at roughly 4-5k operations: the smoke test (~480 ops) fine;
--l2test phase C wedged inside a 2048-word loop (~4k ops); the device-era
console died at ~890 chars (~5k ring ops). Cached stores do NOT show this
(the bss clear = megabytes, fine). Relevant when sizing any future device-
access loop; the l2c_enable by-way hazard (KNOWN_ISSUES #3) is a DIFFERENT
mechanism (one op, a deadlock — not a traffic cliff).

## The theory-progression map (what was tested, in order, and why)

slab_mutex/merge-scan theories (session 6 start) → console-visibility test
→ parse-probes (the pv/stale-cmdline question) → the corruption signatures
(|0x80, independent ring damage) → the monitor-RE detour (trustzone-omap4
found already on disk in device-binaries/) → --l2test A/B (the bypass
retired) → the L2-on pivot → the 120-wall (the C/B/S strip) → the SMP-clear
theory (tested, wrong) → the strip removal → the 132-wall (the UART3 store)
→ rings-only → the memblock limit-0 → 127 → the 0x1f7f0000 BUG → the barrier
disable (no effect) → --l2lat (SIGBUS proof) → session end. Do not re-tread:
every step has a recorded readback in docs/03.

## Operational micro-facts (from living the session)

- jump.sh defines `SSHARGS` with the host-key/cipher options; bare `ssh`
  fails with "no matching host key type" — reuse SSHARGS or the same options.
- The kernel toolchain prefix `arm-linux-` exists as symlinks to
  `arm-buildroot-linux-gnueabihf-*` in `/home/psyden/toolchains/armv7-eabihf/bin/`.
- mkkernel.sh packs zImage + pad + DTB into kexec/kernel/zImage (~5.56 MB;
  the size changes are a useful "did the build actually change" check).
- GPIO1: 0x194 = SETDATAOUT (LED on), 0x190 = CLEARDATAOUT (LED off), bit 13
  = the FAN5702 enable. The user corrected: this is an indicator LED, NOT
  the backlight.
- The user video-records runs; always ask for the LED timings (blue-on
  duration, blue-off → red gap) — the timings decoded several walls (the
  blue-off at 00:40 = the setup_arch LED-off block = post-paging_init proof).
- The user asked to avoid python heredocs for file edits (a past session
  lost code to one) — prefer the Edit tool.
- The user's display backlight timeout is a run-variable: note whether the
  screen was on at jump time (DISPC scanout = DRAM contention; "display
  quiesced" does not actually blank it).

## Bequeathed open questions

- The dma_mmu_remap[0].base corruption (0xbe800000 → 0xa1000000) was never
  explained even after the pv-stub reading (the base AND the virtual were
  both wrong in the same struct — two anomalies, or one wild write). The
  PB-CMA print (session 7) will catch the next occurrence.
- Why run 23's console died at the dram-barrier print while its bc ladder
  continued to 171 (the console and the kernel died at different points —
  the two death mechanisms were never separately root-caused in that era).
- Whether the |0x80 / independent-ring-damage signatures ever recur on the
  fixed (cacheable + SMC-flush) console — if they do, the corruption theory
  revives with much better evidence quality.
