# Session 5 Notes (2026-09-02, earlier — the L2-disable campaign, monitor RE, docs pass)

Backfilled at the start of the documentation pass (session-05 context). This
session ran jumps 23–32, RE'd the monitor service table, achieved the first
L2-off boot deep into start_kernel (the "171 wall"), overhauled console
observability (and got it wrong twice), and rewrote the docs/ folder. Sessions
6–7 superseded parts of it (L2-off retired, the console redesigned); this file
holds what only the session-5 context can supply, plus retractions of its own
claims. docs/03 §"Session 2026-09-02 Late" has the summary; the run map and
the SMC hang matrix are there — not repeated here.

## CONFIRMED (verified this session, not recorded elsewhere)

1. **The exact working L2-disable recipe** (historical — the mode is retired
   per D4, but it worked and may be needed again): SMC #0, r12=0x102, r0=0,
   called via `mon_call()` from do_t3's C flow, positioned AFTER the CPU1
   hold + L1 clean + GICD-off (markers 32→51/52 bracket it), with a pre-clean
   of the payload's globals+stack by PA (`l2c_ns_clean_range`, NS by-PA
   0x768/0x730) so no post-disable read hits a stranded-dirty line, and
   readbacks into bc[2] (CTRL, expect 0) / bc[4] (dlat) / bc[6] (SMC status).
   Verified 4/4 in this shape; bc[8]=0 re-verified by the probe each run.
2. **The SMC-shape discovery narrative** (the matrix in docs/03 is the
   summary; this is how each branch died):
   - Runs 5–9 (enter_stub-inline): killed one theory per run — garbage r1
     (runs 5, 6), r0–r3 zeroed (runs 7–9), a 10 ms PL310 "queue-drain" delay
     before the SMC (run 13-era, no effect), GICD on/off (no correlation),
     MMU on/off (run 4: MMU-off SMC never returns). The mirror0 70/71 marker
     pair (pre/post-SMC) was built specifically to localize the hang and
     proved the SMC itself never returned.
   - The "WDT kick in the pre-SMC window" correlation (runs 5–9 vs 2/3) was
     tested by moving the kick and REJECTED (run 12/13: still hung).
   - Runs 6/7's "died before bc=70" was a SECOND bug hiding behind the first:
     the GICD-off was done as `str` to constant PA 0x48241000 used as a VA —
     unmapped in QNX's tables → silent abort before the marker. Rule: any
     device access from the payload goes through a mapdev view; a PA used as
     a VA aborts silently.
3. **The WDT2 PA-encoding bug discovery** (run 30's fix): found after the
   user observed "the device doesn't take longer to reboot" across runs with
   supposedly-working kicks. Root cause: the 0xFEB section-setup pattern's
   `orr r3, r3, #0x200` carries PA bits (0x48200 → PA 0x48200000); copying it
   into the new 0xFEC section decoded PA 0x4A500000 instead of 0x4A300000 —
   every VA-based kick (pb_bc/pbmarkv/PB_MMU_BC) wrote a harmless register.
   **Idiom rule**: in section setup, values orr'd BEFORE `lsl #12` are PA
   bits; r7 (mmuflags) and XN are orr'd AFTER. Verify the built descriptor by
   disassembling head.o (the immediates are visible: `mov #0x4a000; orr
   #0x300; orr r7,r3,lsl #12`).
4. **WDT2 live-measurement technique**: read WCRR (0x28) twice ~3 s apart —
   if the remaining time JUMPS UP, a kick occurred in between. Used to observe
   QNX's wdtkick live (45.6 s → 57.0 s remaining across a 3 s gap) and to
   prove the 58.6 s window (LDR 0xFFE2B400) without waiting for a reset.
5. **The device clock is FROZEN (~2021)**: time(NULL) on the device returns a
   constant (0x4EC2F743-era). The first run-nonce design (time(NULL) only)
   was therefore useless — fixed by mixing in the staging phys (varies per
   run): `nonce = time(NULL) ^ phys`. Anything time-based on this device is
   suspect; use counters/hardware state instead.
6. **The build-timestamp attribution technique**: the kernel banner's
   `#NN SMP <build date>` matched against the zImage/arch-file mtimes
   attributes ring3 content to a specific kernel build. Used to prove
   ring3's preserved log was run 23's kernel (#51, 08:21:30) and that runs
   24–32's kernels (#52+) emitted zero UART3 bytes.
7. **memdump3 decoding**: packing each printed word with little-endian
   `<I` (python struct, or reading the hex left-to-right as the true word
   value) decodes everything correctly — bc markers, addresses, and the
   char rings all matched. The "reverse each 4-byte group" instruction in
   newer docs describes the same operation for human hex reading; the two
   notes are equivalent, not contradictory.
8. **The payload's mirror mappings are 0x100 bytes only** (mapdev per mirror)
   — the ring HEADER sanitize (count/index at +0x80/+0x84) covers the
   headers, but the char area (mirror+0x100..+0x4FF) is outside the mapping
   and cannot be sanitized by the payload. Anything reading those chars must
   treat them as possibly stale from a previous boot.

## ANALYSES & HYPOTHESES (formed this session; dispositions noted)

1. **The 171-era console death (run 23-era, L2 off, UNCACHED kernel): the
   console died at "r." (~450 chars) while the bc ladder continued to 171 —
   two different death points.** Candidates formed in session 5:
   (a) the per-char PL310 by-PA CIPA+sync (6 ops/char) against the L2-OFF
   bypassed controller — ~2700 ops before the death; session 6 later
   calibrated the device-op cliff for L2-ON (~4–5k SO ops) — the L2-OFF
   CIPA-cliff is UNCALIBRATED and this data point (death at ~2700 PL310 ops)
   is consistent with a cliff on the by-PA path too;
   (b) the UART-drain LSR read at VA 0xFED20014 (unmapped then) — the L3
   swallows it silently and the bounded drain exits — survivable, ruled out
   as the killer;
   (c) the ring1 writes to 0xCC000080 — RETRACTED (session-5 arithmetic
   error; see the correction below — the alias was always 0xC8000080 ✓).
   Session 6 replaced the whole mechanism (rings-only, flushes removed), so
   (a) is historical — but it is the best L2-off cliff datum available.
2. **The bc[2]=0x3E7 (999) wild write**: session 5 tested and killed the
   pb_bc-pair theory (the macro writes bc[2]=427, never 999) and the
   PB_MMU_BC theory (would be v|0x100, no v fits). The console's ring2 char
   window (0xD0000100+idx → PA 0x90000100+) cannot reach bc[2] (0x90000008)
   with a bounded index. Remaining candidate: a wild strb from a corrupted
   ring index (the payload's own "wild strb" warning) or the monitor —
   UNRESOLVED. If it recurs with the current console, dump bc[0x80–0x8F]
   (the ring2 header region) alongside.
3. **The 'U' (0x55) filler beyond ring3's char window**: session 5
   hypothesized it was the splash/animation framebuffer (the DISPC scanout
   region the user sees at boot — dark logo pixels on 0x55-gray). Session 6
   called it "0x55/0xaa filler" without attributing the source. The splash-FB
   hypothesis is UNTESTED but would explain why the filler appears exactly
   beyond the console window and why the region survives reboots. Low
   confidence; recorded so the idea isn't lost.
4. **The monitor-capture theory (RETRACTED by session 6)**: session 5's
   model was "the monitor taps UART3's TX into ring3; the count = cumulative
   UART3 bytes". Session 6 proved the capture INACTIVE (the kernel never
   writes UART3 post-idle — a posted store to the dead THR stalls the store
   buffer) and the count frozen at run 23's total. The frozen count across
   runs 24–32 was the observation that led to the console-death discovery —
   the theory was wrong but productive.
5. **The addruart infinite-loop trap** (arch/arm/include/debug/omap2plus.S —
   NOT the built omap4bc.S): with no CONFIG_DEBUG_OMAP*/ZOOM_UART set, the
   addruart fallback branch is EMPTY (`b 10b`) — if omap_uart_phys/virt are
   zero, the first printascii loops forever. The built kernel uses
   CONFIG_DEBUG_LL_INCLUDE="debug/omap4bc.S" (unconditional constants), so
   this is a trap only for anyone switching the include back to omap2plus.S.
   UNRESOLVED puzzle from session 5: the kernel passed the stext smoke test
   (bc=171 > 109) even while I believed the UART vars were unconfigured —
   resolve by reading omap4bc.S (its addruart has no variable check at all).
6. **The 'r.' partial line** (the old log's last output before the gap):
   session-5 candidates were a mid-print death during a line starting with
   "r" ("rcu: ...", "random: ...") or a wrap artifact. Session 6's recovered-
   log reading treats it as the end of run 23's console. Never root-caused;
   listed in session-06.md's bequeathed questions.

## SELF-CORRECTIONS (session-5 claims superseded — where and why)

1. **"ring1's alias 0xCC000080 is wrong / writes silently dropped"** (was in
   docs/08 + docs/01): ARITHMETIC ERROR of session 5 — the omap4bc.S
   subtractions are cumulative (0xD4000080 → −0x4000000 → 0xD0000080 →
   −0x8000000 → 0xC8000080 ✓ = PA 0x88000000 + LOWMEM_DELTA). FIXED in
   docs/08 + docs/01 this pass (dated notes). The current omap4bc.S
   (session-6 rewrite) uses the same cumulative arithmetic with the flush
   machinery and the UART drain removed.
2. **"ring3 = the monitor's UART3 capture = the primary console readback"**
   (the session-5 handoff + docs/00/05/06/07): session 6 proved the capture
   INACTIVE (the kernel never writes UART3 post-idle) and moved the primary
   channel to ring1 (the SMC-batched flushes). The session-5-era statements
   in docs/05/06/07 were already amended by sessions 6/7; docs/00 carries
   the corrected console-truth paragraph. The HANDOFF_2026-09-02_late file
   is historical — read session-06/07 notes + PROJECT_STATE instead.
3. **"The 171 death is REAL (not the watchdog)"** (my handoff, after run
   30's kick fix): true for that run, but the FRAME ("the kernel dies at
   171") was later enriched by the era matrix
   (contradictions/171-wall-analyses.md): run 23-era = UNCACHED kernel +
   L2-off + full memory; the session-7 era = CACHEABLE + L2-on + no-DTB
   fallback. Same marker, different eras — read the matrix before assuming
   one cause.
4. **"23 KB of console output"** (my handoff/docs): session-06 correction
   #1 — the recovered ring3 = ~15 real lines (through the dram-barrier print
   + a partial "r.") + 0x55/0xaa filler. The count (23061) counts
   ring_puts, not distinct visible content.
5. **"0xFED was never mapped, I added it"** vs **docs/00 (session 6): "the
   old '0xFED was never mapped' diagnosis was WRONG — the pristine DEBUG_LL
   block in head.S always mapped it via addruart"**: a genuine unresolved
   discrepancy. Session 5's grep of the then-current head.S found no 0xFED
   section (that is why the section was added, and the added section is
   still present in the tree with its original comment). Session 6's note
   says the pristine DEBUG_LL always mapped it "via addruart" (a macro
   cannot create a mapping) and attributes the console death to the flush
   machinery + the UART3 store instead. Both agree the console is fixed by
   the session-6 rewrite; the mapping history is recorded here as
   UNRESOLVED rather than silently reconciled.

## OPERATIONAL MICRO-FACTS (from living the session)

- `retry-jump.sh`'s marker compare must use the memdump3 hex strings
  (`00000046`/`00000047` for decimal 70/71) — the first version compared
  decimal-as-hex strings and never matched.
- jump.sh's grep filtering hides timing data — run it unfiltered when
  reboot timing matters (the user's run-27 timeline came from an unfiltered
  view + their own SSH keepalive script).
- The payload's nonce edit originally landed in do_hello (a
  multiple-matches edit gone wrong) — after the WSL crash + the partial-file
  corruption incident, the recovery required rebuilding from the corrected
  lines. Rule (user-mandated): read files fully before editing; no python3
  for edits.
- WSL crashed twice mid-session; after each, verify: the payload builds
  clean, the kernel zImage mtime matches the last intended build, and the
  nonce/marker edits are present (grep for the distinctive comments).
- jump.sh's detached launch (>/tmp/jump.log &) means the payload's printf
  output is NOT on the jump console — read /tmp/jump.log via a second SSH
  while the payload runs (pre-jump only), or rely on the bc ladder.
- The stale `PROMPT_FOR_THE_NEXT_SESSION.md` was deleted from OpenViking at
  the user's request (superseded by HANDOFF_2026-09-02_late).

## WHAT SESSION 5 PASSED TO SESSION 6 (the bequest, from session 5's view)

- The working L2-disable recipe (above) — session 6 used it for the --l2test
  A/B and then retired the mode (D4).
- The console-visibility question (why does the console emit nothing?) —
  session 6's first task; answered by the parse-probe design.
- The 171 wall with the improved markers — session 6/7 took it through the
  era matrix; still the open wall at backfill time.
- The corrected WDT2/UART3/PRCM register facts — folded into docs/04/06.
- The recovered run-23 log — session 6's correction #1 re-read it with the
  filler attribution.
