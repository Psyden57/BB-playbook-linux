# Session 3 Notes (2026-08-31 evening — the 17-run marathon, the M=1 wall, the diagnostic-channel rebuild)

Backfilled at the start of session-03 documentation from the session-3
conversation context. Session 3 = the evening of 2026-08-31: took the
handoff's "dies between bc=108 and 110" at face value, discovered that most
of the diagnostic channel itself was lying, rebuilt it, pivoted Image→zImage,
and hit the M=1 wall at bc=122. docs/03 §"Session 2026-08-31 Evening" holds
the conclusions; this file holds the run-by-run narrative, the confounded
experiments, and the micro-facts that only this context can supply.

Caveat on numbering: this session did not use canonical run numbers. The
runs below are in execution order; later sessions number from 19+ (session
4's memtest onward).

## THE RUN MAP (session 3, in order, with the actual readbacks)

All jumps used jump.sh; bc readback via memdump3 @0x90000000. Kernel was the
Image for runs 1-3, zImage from run 4 on.

1. **Image + markers 114/115 (ring-block entry / cpt return), ring maps
   KEPT**: bc=**102** (0x66), ring1 count=0x14 = 19 smoke chars + 'K' — the
   smoke test provably ran, so bc=102 is *below* proven execution → the bc
   channel itself is unreliable (unflushed-L2 theory born).
2. **Same build, retried placement variance**: bc=115 pattern established
   across placements (see run map of the era: a0200000/a0c00000 → 115,
   a0f00000 → 102).
3. **DSB-before-CIPA fix in all marker macros + rebuild**: bc=**115** —
   `__create_page_tables` completes. Ring maps "exonerated" (store-time
   only — see CONFIRMED #12 for the nuance session 4 later resolved).
4. **Early-VA rebase (UART 0xFEB20000→0xFED20000, PL310
   0xFEB21000→0xFEB42000) + markers 117/118**: bc=**115** again — dies
   before 117 even with the console/PL310 VAs fixed. This killed the
   earlyprintk-abort theory *for the Image path* and triggered the
   PHYS_OFFSET analysis (see CONFIRMED #2) → **zImage pivot**.
5. **zImage + markers 119/120**: bc=**119** (0x77) — the decompressor works,
   the kernel enters, cpt returns, death before mmap_switched. Kernel[0] in
   bc[5] = e1a00000 (zImage word 0) confirms which image ran.
6. **zImage + pbmark3 121/122 inside __enable_mmu/__turn_mmu_on**: bc=**102**
   — first nondeterminism (user noted display was ON this run, OFF for run
   5). Display-corruption hypothesis born; jump.sh display-quiesce added.
7. **zImage + quiesce**: bc=**122** (0x7a) — `__turn_mmu_on` entered. The
   M=1 wall in its final form.
8. **zImage + marker 123 post-M**: bc=**102** — the nondeterminism returns
   on the same build → disassembled head.o → **the DSB fix was never in the
   built object** (the earlier rebuild had silently produced a stale Image
   for the packed zImage; mkkernel.sh had packed before the object
   actually rebuilt). Lesson: verify fixes in the binary, not the build log.
9. **DSB verified in disassembly + rebuild**: bc=**122** — real ceiling
   confirmed; the pre-122 markers are now trustworthy.
10. **+ marker 124 at VA 0x90000004 (stale-TLB identity trick)**: bc=**122**
    — no 124. Stale-TLB theory born here, later killed (CONFIRMED #7).
11. **ASID experiment (CONTEXTIDR=0x42 in __enable_mmu) + 124 repointed to
    VA 0xD0000004**: payload died pre-jump (bc=30) twice; retry reached the
    jump → bc=**122**. ASID reverted (it broke the then-stale-TLB marker and
    never got a clean test).
12. **WDT2 timing analysis**: the early-death points (102/107/115/119) only
    occur when the payload's setup is slow; do_t3 never kicked WDT2
    (wdt2_kick was T2-only) → early kick at bc-armed + late bare-register
    kick before enter_stub added. jump.sh timeout 30→120 discovered the
    same evening (the "silent payload deaths" were the ssh timeout
    SIGHUP-killing the payload mid-setup — see ANALYSES #3).
13. **Abort trap built** (vector page VA 0x000/0xFFF → 0x9FE00000, handler
    writes 0xAB, PL310-flushes, spins; flag first at 0x900000A0, moved to
    bc[1] after a QNX reboot churned 0xA0): fired **never** across all
    remaining runs → the M=1 death is a silent stall, not a precise abort.
14. **Uncached early descriptors (C/B/S stripped from mm_mmuflags) +
    ACTLR.SMP cleared + TTBR0 flag strip (bic r4, #0x6A)**: bc=**122** —
    every variant hangs (see CONFIRMED #9-11 for what each strips).
15. **Power-cycled device + full clean run**: payload died post-"kernel
    copied" (bc=31); readback showed **bc[6] = adfe0dd0 — the DTB magic
    written as edfe0dd0 flipped one bit (bit30)** → the DRAM-integrity lead
    (see ANALYSES #1).
16. **Memtest sweep added to do_t3** (4 patterns through the NOCACHE view):
    run ended inconclusively (device dropped SSH mid-run); the standalone
    memtest became session 4's first task.

## CONFIRMED (facts from this context, not recorded elsewhere)

1. **The 1 MB-section collision that forced the VA rebase**: omap4bc's
   addruart originally returned 0xFEB20000 and debug_ll_io_init mapped PL310
   at 0xFEB21000 — same VA 1 MB section (0xFEB), but UART3's PA base is
   0x48000000 and PL310's is 0x48200000. A section descriptor encodes ONE PA
   base, so both devices could not be early-mapped in the same section. The
   rebase preserved PA-offset-in-section == VA-offset-in-section: UART →
   0xFED20000 (PA base 0x480), PL310 → 0xFEB42000 (PA base 0x482, PL310's
   0x42000 offset preserved). This is why the UART VA is 0xFED at all; see
   the addendum in contradictions/0xFED-mapping-history.md.
2. **The Image-path PHYS_OFFSET mechanism** (why zImage was non-negotiable):
   `KERNEL_OFFSET = PAGE_OFFSET` (asm/memory.h:35) and head.S derives
   PHYS_OFFSET from the load address (`adr_l r8, _text; sub r8,
   #TEXT_OFFSET`). Loading an Image at 0xa0c08000 makes PHYS_OFFSET
   0xa0c00000, so the linear map becomes VA = PA + 0x1F400000: (a) the early
   bc/ring section VAs (0xC88/0xD00/0xD40) get overwritten by map_lowmem
   with wrong-PA linear entries (VA 0xD0000000 → PA 0xB0C00000); (b) all RAM
   below the load address (≥512 MB, incl. bank 1) gets linear VAs *below
   PAGE_OFFSET* (user-segment addresses) — memblock/pfn handling is
   structurally unsound. The decompressor (zImage) is the only path that
   yields PHYS_OFFSET = true RAM base. Session 7's placement fix
   (D5: kern_off + DTB patch) later re-legitimized a *controlled* placement,
   but the session-3 conclusion — an *arbitrary* Image address is broken —
   stands.
3. **The marker macros' DSB-before-CIPA ordering**: the original pbmark
   stored the marker and immediately wrote the PL310 CIPA register — no DSB
   between data access and cache-maintenance-by-PA (PL310 TRM requirement).
   A CIPA that outraces the store invalidates the line without the new
   value, and the readback shows an *older* marker. Every marker macro now
   dsb's between store and CIPA. Distinct from session 4's later (bigger)
   finding that the flush machinery itself costs device-op cliff budget —
   that one led to flush-free markers; the DSB ordering remains required
   wherever CIPA is still used.
4. **The stale-object trap**: run 8's "the fix didn't work" was a build that
   reported success while head.o never rebuilt into the packed zImage
   (mkkernel.sh packed a stale Image). The disassembly of head.o showed the
   DSB absent. Always objdump the shipped binary; build success ≠ fix
   present. (Session 4's build-size tracking, their CONFIRMED #12, is the
   systematic version of this.)
5. **Duplicate marker values make old trails ambiguous**: in the session-3
   stext, 107 and 108 each appear at TWO sites (the addruart/senduart/
   busyuart bisect AND the fixup_smp/fixup_pv pair), so a readback of bc=107
   or 108 alone cannot distinguish which site was last. The later marker
   ladders (sessions 5-7) use unique values; when reading the 2026-08-31-era
   trails, treat 102/107/108 as ambiguous.
6. **pbmark3/pbmarkv exist because of live-register hazards**: at
   __enable_mmu/__turn_mmu_on, r0 carries the SCTLR value and r1 the machine
   ID — the standard pbmark (clobbers r0+ip) would corrupt both. pbmark3
   uses r3+ip only (MMU-off sites); the post-M variants use VA-based stores.
   Anyone adding markers in that window must not clobber r0/r1/r2/r4/r9/
   r10/r13.
7. **The zImage decompressor issues TLBIALL at least 6 times** (arch/arm/
   boot/compressed/head.S, "flush I,D TLBs" sites) before the kernel runs.
   Every stale-TLB trick (identity-VA stores via cont-era TLB entries,
   CONTEXTIDR ASID rewrites) is structurally dead — there is nothing stale
   left to exploit. This also independently corroborates session 4's run 10
   finding (ACTLR at kernel entry = 0x1, decompressor already cleared the
   SMP bit: the decompressor runs the CPU far from QNX's configuration).
8. **The IRAM-table experiment design** (built, run once, confounded): the
   only free 16 KB of OCMC above cont/probe/params is 0x4030A000-0x4030E000
   (exactly; OCMC ends at 0x4030E000). cpt was pointed there via three
   immediate-forming instructions (0x4030A000 is not an encodable mov
   immediate; use mov+orr+orr). With the ring-map bug still present the run
   was confounded; post-fix it is pointless (the walk source was never the
   problem).
9. **The TTBR0 walk-attribute strip (`bic r4, r4, #0x6A`) is still in
   head.S and its necessity is UNTESTED**: v7_ttb_setup ORs
   TTB_FLAGS_SMP (S|NOS|RGN|IRGN = 0x6A) into r4 *in place*, and
   __enable_mmu writes TTBR0 = r4 with the flags baked in — making the
   first PTW reads shareable+cacheable-WBWA. Session 3's shareable-walk
   deadlock theory (PTW read snoops CPU1, held in reset, no response) was
   never disproven — the session-3 tests of it were all confounded by the
   ring-map bug. The strip survives in the shipped head.S (2026-09-02
   lineage); nobody has booted without it since the ring-map fix. Removing
   it is a cheap experiment if walk-attribute behavior ever matters (e.g.
   for walk-cache performance questions).
10. **The abort-trap never fired, which is itself evidence**: with the
    vector page mapped (VA 0x000/0xFFF → 0x9FE00000, executable, io_mmuflags
    attrs) and the handler PL310-flushing its flag, a *precise* abort after
    MMU-on would have stamped bc[1]=0xAB. Across all post-trap runs the flag
    stayed clear while bc=122 — the M=1 death is a stall/deadlock or a
    wild-execution (corrupted-code) event, not a precise data/prefetch
    abort. Sessions 5-7's later findings (device-op cliff, placement
    corruption) are consistent with this.
11. **QNX's CP15 state at jump time** (payload mrc in System mode, printed
    at "System mode entered"): **ACTLR = 0x41** (bit6 SMP/nAMP + bit0
    cache/TLB-op broadcasting) and **diagnostic c15,c0,1 = 0x810** (errata
    742230 bit4 + 751472 bit11 workarounds — i.e. the kernel's own errata
    code had nothing left to set). proc-v7's setup ORs its bits onto this
    and preserves the rest.
12. **"Ring maps exonerated" (session-3 wording) was store-time-only**:
    run 2's bc=115 proved cpt *completes* with the ring-map block present —
    the poisoned entries (the shift bug wrote 0x88000<<20 = 0, i.e. table
    index 0x000) could only fault at walk/fetch time, which is exactly where
    session 4 found the M=1 unlock. Both sessions are right; the boundary is
    store-time vs translation-time. Corollary trivia: the shift bug's stray
    entry landed in tt[0x000] — the same slot the session-3 abort trap later
    used for the vector page; the trap entry overwrote the stray.
13. **Payload death taxonomy from this session** (user-confirmed cases):
    (a) ssh `timeout` SIGHUP-kills the payload mid-setup → *no* reboot, bc
    frozen at the last armed step (the "silent deaths"); (b) a payload crash
    that takes procnto down → WDT2 → *full* reboot, bc survives from
    pre-crash; (c) a successful jump → WDT2 → reboot with kernel markers in
    bc. Distinguishing them: reboot + bc=armed-step ⇒ class (b); no reboot +
    frozen bc ⇒ (a); reboot + kernel markers ⇒ (c). jump.sh's wait loop
    exits immediately in class (a) because the device never goes down.

## ANALYSES (formed this session; dispositions noted)

1. **DRAM single-bit flip (kept, caveated)**: bc[6] written 0xedfe0dd0 by
   the payload (DTB magic), read back post-reboot as 0xadfe0dd0 — bit 30
   flipped, in the bc page at 0x90000018. Caveats: (a) the bc page was
   OUTSIDE session 4's memtest coverage (which swept the fallback buffer
   region only); (b) the write path was NOCACHE and the read path memdump3
   after a WDT2 warm reset + QNX reboot — a lost write or partial churn
   could mimic a flip, though the observed value is not the 0xAA churn
   pattern; (c) single observation. Feed into the
   machine-corruption-vs-code-bugs.md ledger as the earliest corruption-era
   datum (see the addendum there).
2. **"WDT2 15 s window exhaustion causes the random early deaths"** — the
   *fix* (early + late kicks) remains correct and load-bearing, but the
   *mechanism* was wrong: the window is 58.6 s (session 5's register
   measurement), not 15 s, and several of the "random death points" were
   later re-attributed to the ssh-timeout SIGHUP (ANALYSES #3) and to the
   ring-map bug. Disposition: fix kept, theory superseded.
3. **"The silent payload deaths are the ssh timeout SIGHUP"** — proven by
   construction: raising jump.sh's jump-step timeout 30→120 s made them stop
   (the payload's setup with the re-verify takes ~40-60 s). The mechanism:
   ssh timeout → SIGHUP to the remote shell → payload dies mid-setup with no
   output, no reboot, bc frozen at the last armed step. Disposition:
   confirmed; timeout now 120 s (session 5 later hit the same wall at 120 s
   with eMMC sync stalls — run 15 of their map).
4. **"Display state correlates with the deaths"** (runs 5/7 display-ON →
   bc=102 vs display-OFF → 119/122) — never confirmed; every later run ran
   quiesced and the variance was explained by infra bugs. The quiesce stays
   in jump.sh as free insurance (screen is a 66 MB live DMA agent).
5. **"Shareable-walk deadlock" (the final session-3 theory)** — TTB_FLAGS_SMP
   make the first PTW reads shareable; a shareable PTW read snoops CPU1;
   CPU1 held in reset cannot respond; deadlock. Never disproven — all tests
   predate the ring-map fix — and the 0x6A strip is still standing in head.S
   (CONFIRMED #9). Status: untested against the fixed baseline; cheap to
   test by removing the strip for one run.

## DEAD ENDS (do not retest)

- **CONTEXTIDR ASID rewrite (0x42) to neutralize stale TLB entries** —
  pointless (decompressor TLBIALLs) and it broke markers that relied on
  cont-era entries.
- **Stale-TLB identity-VA stores as post-M markers** — no stale entries
  survive the decompressor; use mapped VAs only.
- **IRAM-resident kernel table** (0x4030A000) — the walk source was never
  the problem; also paging_init's swapper_pg_dir PA would mismatch (fatal
  later, diagnostic-only by design).
- **ACTLR.SMP clear + uncached early descriptors as M=1 fixes** — both
  tested pre-ring-map-fix (confounded); both later removed by sessions 5-7
  (head.S annotations "the C/B/S strip is REMOVED" / "the ACTLR.SMP clear is
  REMOVED"). The 0x6A walk-flag strip is the ONE session-3 patch still
  standing (CONFIRMED #9).
- **Reading the flag from 0x900000A0** — QNX reboots churn that word
  (0xAAAAAAAA); the flag moved into bc[1].

## WHAT SESSION 3 PASSED FORWARD (the bequest)

- The trustworthy bc channel (DSB-ordered markers) — everything downstream
  reads bc through it.
- The zImage pivot and the PHYS_OFFSET reasoning behind it — D6's
  foundation, and the reason session 7's D5 (kern_off + DTB patch) was
  needed at all.
- The VA topology: UART 0xFED20000 / PL310 0xFEB42000 / PL310 section map
  0xFEB00000→0x48200000 — omap4bc.S still returns these, and the 0xFED
  mapping-history contradiction traces back to this rebase.
- The marker ladder's shape (pbmark / pbmark3 / pbmarkv register budgets,
  post-M VA markers) — sessions 5-7 extended it to 84+ with the flush-free
  principle.
- The WDT2 kick discipline and the 120 s ssh timeout — still in jump.sh.
- The eliminated list of this session (store-time vs translation-time ring
  map, walk source, descriptor attrs, ACTLR.SMP, fills, stale-TLB) — folded
  into docs/03 and superseded where session 4's ring-map fix reframed them.
