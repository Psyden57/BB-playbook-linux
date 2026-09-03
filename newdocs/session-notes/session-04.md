# Session 4 Notes (2026-09-01 evening — memtest, the ring-map fix, the PL310 secure-filtering campaign)

Backfilled at the start of session-04 documentation from the session-4
conversation context. Session 4 = the evening of 2026-09-01: built and ran
the standalone memtest (DRAM clean), instrumented the payload setup
(heartbeats 42-50), **found and fixed the head.S ring-map shift bug** (the
M=1 wall), chased the early-C wedge with progressively finer markers, and
mapped the PL310 secure-filtering by experiment. docs/03 §"Session 2026-09-01
Evening" and SESSION-RECORD_2026-09-01_ring-map-fix_early-C-wedge.md hold the
conclusions; this file holds the run-by-run narrative, the dead-end reasoning
chains, and the micro-decodes that only this context can supply.

## THE RUN MAP (session 4, in order, with the actual readbacks)

All jumps used the zImage + probe chain; all bc readbacks via memdump3
(0x90000000 primary, mirrors, 0x9FE00000 flag). Flag = 0 every run.

1. **Memtest** (`memtest 2`, payload-only, no jump): requested placements
   0xA4000000/0xA8000000/0xAC000000 were **all rejected every attempt**
   (mmap succeeded but mem_offset64 gave non-contig or a different PA) — the
   24 MB run came from the fallback loop at **0xa0600000**. Verdict CLEAN
   (2 rounds + 4 canary pages, 0 errors). Caveat now obvious in hindsight:
   the clean verdict covers the fallback region only — which is also the
   region every later kernel run landed in, so the conclusion stands.
2. **Heartbeat jump, ring-map UNFIXED kernel**: bc=122 (the old ceiling),
   setup heartbeats 42-50 all passed (setup proven clean). The wedge was in
   the kernel, not the payload.
3. **Ring-map FIXED kernel**: **bc=118** (start_kernel) — first time past M=1
   ever. bc[2]=0x60540100, ring1=20 chars, flag=0.
4. **+markers 136-135 (setup_arch bisect)**: bc=118 again.
5. **+dual-channel bc[2]**: bc=118, bc[2] stale (0x60540100) → died before
   the first pb_bc store.
6. **+triple-channel (SMC mirror)**: **bc=142**, bc[2]=0x18E, mirror=0x28E —
   first crossing of 142. Died before 143.
7. **+panic notifier + 144/145**: **bc=145**, bc[2]=0x191.
8. **+cgroup bisect markers**: **bc=168** (post init_and_link_css inside
   cpuset's cgroup_init_subsys).
9. **Determinism re-run, SAME binary**: **bc=150**. Wandering confirmed →
   machine/state event, not code. (168→150 on identical bits.)
10. **v5 (ACTLR dump+clear, phase mirror)**: bc=118, **bc[2]=0x1 landed
    flush-free** (the ACTLR block's bare store+dsb), **bc[3]=0x1 = ACTLR at
    kernel entry** (decompressor already cleared SMP bit 6). This run proved
    the flush-free channel and killed the broadcast theory in one shot.
11. **v6 (all fine markers flush-free, 150/151)**: **bc=145** — the flush
    machinery in the marker path was itself a wedge source; removing it let
    the kernel reach 145 repeatably.
12. **cgroup markers rebuilt**: **bc=168** again (repeatable with the clean
    marker path).
13. **Determinism re-run again**: **bc=150**. Wandering across {145,168,150}
    on identical binaries.
14. **DISPC-blank + latency-bump payload**: **SIGBUS at the PL310 latency
    write** — `Process terminated SIGBUS code=3 fltno=5 ip=080494c8
    (/tmp/qnx2linux@_btext+0x8cc) ref=2802c10c`, bc=34 (DISPC blank landed).
    QNX caught the abort; device stayed up; NO reboot. The DISPC blank
    itself worked.
15. **DISPC-blank only (latency write removed)**: payload hung in
    bc_snapshot_file's `sync()` at bc=40 (eMMC stall; no SIGBUS, ssh timed
    out at 120 s, device did NOT reboot; jump.sh's wait loop exited on the
    first try because the device was still up). Immediate retry of the same
    binary ran clean to the jump → **bc=150** (with scanout blanked — the
    wedge unchanged → scanout contention ruled out).
16. **SMC 0x105 ("L2 disable" guess)**: payload ran to bc=41 (post-GICD) in
    ALL pages, but **bc=70 (enter_stub) never landed in any mirror** — the
    chain broke between enter_stub and the probe. User's LED: blue stayed on
    **~2 m 30 s** with NO probe toggle sequence, then off+red. The probe's
    canaries in bc[4] were absent (bc[4]=1 = my control readback) while
    bc[6]/bc[7] held STALE canaries from earlier runs (bc_arm does not
    sanitize those slots). Reverted.

## CONFIRMED (facts from this context, not recorded elsewhere)

1. **The ring decode of the first post-fix run** (run 3): ring1 = NUL +
   "PLAYBOOK-HEAD-101\r\n" + 'K', count=0x14. The initial read ("a second
   string died mid-print") was WRONG: head.S:209 is `mov r1, #'K'` — the
   marker-107/108/109 macro bisect (addruart/senduart/busyuart) — and
   omap4bc.S ring_put increments idx BEFORE the char store, so chars[0] is
   never written (the leading NUL) and count=20 = 19 smoke chars + 'K'
   fully committed. The 'K' is PRE-M (marker 109's busyuart mirror), not
   post-M evidence.
2. **bc[2]=0x60540100 = the probe's raw LE `ldr` of the BE DTB totalsize**
   (field bytes 00 01 54 60 read little-endian). Known red herring,
   confirmed live; it appears in every pre-probe bc dump and means nothing.
3. **ring3 header word 0x00005a15 decode**: bytes [0x15, 0x5A, 0, 0] =
   count 0x15 = 21 = 1 (the probe's own increment) + 20 kernel putcs, with
   the probe's 'Z' (0x5A) still at +0x81. The probe writes its test char at
   `[0x94000080 + count]` — INSIDE the header (probe bug: it bases the char
   off the count address, not +0x100). The kernel's ring3 word count-stores
   evidently never overwrote that byte in DRAM; never root-caused. Session
   5's monitor-capture work later relied on ring3 counts — this +1 offset
   from the probe is part of that history.
4. **bc[12] (0x90000030) holds dtb_phys after a probe run** — no writer was
   ever identified (the probe writes bc[13]=0x34, not 0x30; the payload
   never writes it). Combined with the bc[6]/bc[7] stale-canary trap
   (bc_arm sanitizes only bc[0..5] + ring headers): treat bc[6]+ as
   possibly stale across runs.
5. **Requested 24 MB placements were never honored** (0xA4/0xA8/0xAC
   rejected on every attempt, ~12 runs; fallbacks landed 0xa03-0xa25
   varying per run). This is pre-sweep evidence of QNX's pool fragmentation
   — session 7's W-3 sweep (129 frag hints, 12 unaligned blocks) quantified
   what this era observed anecdotally.
6. **The sync-poll-staleness theory (killed, with the reasoning)**: the
   first wedge theory was "the PL310 sync-poll (VA 0xFEB42730) reads a
   stale L1 line under the kernel's WT/B mapping → infinite poll". Killed
   by grepping proc-v7's io_mmuflags (0xC02 = Strongly-ordered, no C/B) and
   the ring-map block's `|0x200` on the PL310 section (0xE02 = Device) —
   device reads are never cached, so the poll re-reads every iteration.
   Recorded because it motivated the io_mmuflags audit that found the
   ring-map bug.
7. **The ring-map bug discovery path**: io_mmuflags audit → cross-checked
   the ring-map block's idiom against classic head.S (section NUMBERS via
   `lsr #SECTION_SHIFT` … `lsl #SECTION_SHIFT`) → spotted the PA>>12-vs-20
   mismatch → computed 0x88000<<20 = 0 → confirmed by disassembling the
   then-current vmlinux (`orr r3, r7, r3, lsl #20` at c00086f4 with
   r3=0x88000). **First fix attempt used section numbers (0x880/0x900/
   0x940/0x482/0x9FE) and FAILED to assemble**: `invalid constant (482)/
   (9fe) after fixup` — ARM immediates are rotated-8-bit, 0x482/0x9FE are
   unencodable. That is why the shipped fix keeps the PA>>12 values with
   `lsl #12`. Anyone re-attempting the section-number idiom hits the same
   gas error.
8. **Kernel-GAS pre-v7 baseline for generic files**: init/main.c and
   arch/arm/kernel/setup.c are assembled with a pre-v7 -march baseline —
   raw `dsb`/`dmb`/`smc` mnemonics fail ("selected processor does not
   support `dsb sy' in ARM mode"). The kernel's dsb(x) macro expands to a
   CP15 mcr and works; SMC needs .word encodings: dsb sy = 0xF57FF04F,
   smc #0 = 0xE320F000, dmb sy = 0xF57FF05F. Also: a `static` function
   defined inside start_kernel → "invalid storage class for function"
   (no nested functions in C) — the panic notifier lives at file scope; and
   `panic_notifier_list` needs `#include <linux/panic_notifier.h>`.
9. **Two distinct PL310 secure-filter failure modes** (both experienced):
   the latency-register write produced a SYNCHRONOUS abort that QNX
   delivered to the payload as SIGBUS (device stayed up, procnto alive);
   the control-register write produced a silent BUS HANG (no abort, frozen
   machine, 60 s WDT2). If a future experiment must touch PL310 config,
   expect one or the other depending on the register.
10. **Console output is lost, not unprinted, when the device reboots
    mid-run**: in run 16 the console showed only "System mode entered /
    QNX ACTLR" then jumped to "== waiting for reboot ==" while bc proved
    the payload ran to 41 — the prints were made (unbuffered stdout) but
    died in transit with the RNDIS/ssh stack at the reboot. Missing
    console lines ≠ missing payload steps; trust bc over console tails.
11. **The user's LED timelines (the WDT2 discovery source)**: v5-era run —
    blue ON 00:05, blue OFF 00:51 (setup ≈ 46 s), SSH died 01:10, red LED
    01:51 → ~60 s blue-off→reboot. Repeat run: 60 s exactly. The window≠
    period inference came from these timings; session 5 later measured
    58.6 s from the live registers (LDR 0xFFE2B400). The two agree.
12. **Build-size tracking as a change check**: payload 20516→20580→20636→
    20660→20716 B; kernel zImage 5476680→5481224→5479920→5478912→5478696→
    5480168→5475944→5478872→5472560 B (last = CONFIG_CACHE_L2X0=n). The
    delta is the fastest "did the build actually change" signal.

## ANALYSES (formed this session; dispositions noted)

1. **"The flush machinery is itself a wedge source"** — the single most
   consequential session-4 inference. Evidence: with SMC/CIPA blocks in the
   marker path the stops wandered (118/142-era variance); converting the
   fine markers to flush-free made 145/168 repeatable. Session 6 later
   calibrated the device-op cliff (~4-5k SO ops with L2 on) which explains
   the per-block CIPA+poll cost in aggregate. Disposition: confirmed.
2. **"The wandering is a machine/state event, not code"** — from the
   determinism re-runs (168→150 same binary). Session 6's placement/
   stale-DRAM discoveries later gave the mechanism candidates. Disposition:
   confirmed as a conclusion; mechanism superseded.
3. **"QNX's 1-cycle PL310 data latency is a plausible instability source"**
   — formed after reading 0x10C=0x111. Session 7 found no SMC service to
   change it (no latency service in the table; 0x112 lead open). Still
   open; also the suspected cause of QNX's own historic flakiness (sync()
   stalls, RNDIS timeouts — one of each observed live this session).
4. **"0x105 might be L2-disable"** — WRONG (returned 0, no effect, broke
   the jump chain). Lesson recorded in the rules: RE the dispatch before
   probing services; a wrong service can poison the whole jump.

## CONTRADICTION EVIDENCE (recorded where it belongs, not resolved here)

- **0xFED-mapping-history**: added a dated addendum to
  contradictions/0xFED-mapping-history.md with this session's direct
  testimony: on 2026-09-01 (pre-session-5) head.S's pristine DEBUG_LL block
  (then at ~lines 445-454) computed the UART section mapping FROM addruart's
  outputs (`addruart r7, r3, r0`; r3=phys>>20 → table index; r7=virt>>20 →
  section value). A grep for the literal "FED" cannot find it because the
  section value is computed from the macro's return. This supports session
  6's "always mapped via addruart" and explains session 5's grep failure.

## OPERATIONAL MICRO-FACTS

- jump.sh's `tail -12` hides everything before the last lines — early
  payload prints (System mode, ACTLR, bc armed) are invisible in the
  filtered view; run ssh unfiltered when pre-jump output matters.
- The kernel-side `scripts/config --disable CACHE_L2X0 && make olddefconfig`
  path works; the rebuild after it is large (mm/ + full relink).
- `git log` in this repo starts at session 7; the pre-repo era (sessions
  1-6) survives only in docs/, SESSION-HANDOFF/ and these notes.
- The 2026-09-01 session record
  (SESSION-HANDOFF/SESSION-RECORD_2026-09-01_ring-map-fix_early-C-wedge.md)
  and HANDOFF_2026-09-01_next-session.md were the session-5 bootstrap; both
  are historical now (session 6/7 supersede the L2 and console parts).

## WHAT SESSION 4 PASSED FORWARD (the bequest)

- The ring-map fix (the M=1 unlock) — everything after depends on it.
- The flush-free marker principle — sessions 5-7's entire observability
  model builds on it.
- The panic notifier (rings as the pre-console panic channel) — session 6's
  console redesign assumed it.
- The PL310 NS-access rules (by-PA only; config registers filtered) —
  session 5's SMC campaign and session 7's monitor RE started from them.
- The eliminated list (DRAM, scanout, broadcast, 0x105, by-way) — folded
  into docs/03.
