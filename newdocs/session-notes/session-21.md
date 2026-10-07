# Session 21 Notes (2026-10-07 — the (M) study → F2 design → preflight → F2 r1: THE SMC-CLOBBER FIND → fix #167)

## THE SESSION'S SHAPE

Boot from BOOTSTRAP_SESSION_21 (read order done; keeper verified; ledger
#163/#165). The (M) mechanism/lever study (TASK-014) → root review +
session-19 cross-check (folded) → the F2 delta design (relay review SKIPPED
— aged relay, user call; the fresh preflight = the gate) → TASK-015
preflight (GO-WITH-FIXES, folded) → build #166 → **F2 r1 FLOWN: the payload
SIGSEGV'd one instruction after mon_call(0x103) — the monitor-clobbers-
registers bug; the release had executed with NO blob landing; a crash-era
WDT2 cycle fired ~8 min later INTO the restored safe config (clean). Then
fix #167.** Also this session: the user's two standing corrections folded
(wrap timing = user-owned; relay = question-driven — skill v1.7.7); the
boot-friction feedback folded (bootstrap rev2); the USB/LED calibration
reconfirmed live.

## THE (M) STUDY (TASK-014) — ACCEPTED, MOSTLY

- Verdict MOSTLY: (ii) release/re-hold = decision-grade (RSTCTRL←0 release;
  SMC 0x103 verify; the re-hold is payload-side achievable); (a)
  decision-grade in the negative (NO *proven*-safe released cell ⇒ re-hold
  REQUIRED); (i) NOT determinable (zero SAR-pointer full words in either
  bootrom dump; the [A08] consumer is in the hidden 4KB); (iii) no offline
  split (the kexec lead = weak — a wake, not the warm-reset flow).
- Named undecidables: L1 (the consumer), L2 (does a payload re-hold restore
  the exact F1 state — the RSTCTRL-vs-AUX bit9 reconcile; device-only).
- Root re-verification (own dump scans, objdump, raws) documented in the
  record. Session-19 cross-check: ACCEPTED; folds applied (§1 proven-safe
  precision; R1-first ordering; the hand-back cite; §5 alignment) + the five
  DESIGN folds (a-e) carried into the F2 design.
- Files: `TASK-014-m-mechanism-study-report.md` (revised), `TASK-014-m-mechanism-study.md`
  (reviews + folds), `TASK-014-crosscheck-s19.md` (the fold home).

## THE F2 DESIGN + TASK-015

- `W-104-F2-design.md`: blob v2 (7-word marker store → bc[31]=0x5A52F2F2);
  the `--f2rel` mode; the F2 block (kick → release → bounded poll → 0x103 →
  re-hold ≤3(+3) → the outcome table with the NEVER-JUMP abort matrix + A08
  restore + the persistent kexec-f2.log); channels bc[25]/bc[31] + the file;
  the census (bc[25]: head.S W-56 + mmu W-94; bc[31]: mmu.c:1669; [19]/[18]
  rejected with writers). Relay review skipped (aged; user call).
- TASK-015 preflight: GO-WITH-FIXES; four fixes (the printf text; the
  ≤3+≤3 alignment; the build path; the §9(6) residual relabel + the 58.6 s
  sourcing) + the Edit-4b WDT-kick guard + notes (likely-first-result, the
  fold-a mapping, stale-smctest, /accounts persistence). All folded;
  root-verified (blob pattern 0→1; sizes; own disasm matched).

## F2 r1 (W-104 run 2) — THE FLIGHT THAT FOUND THE BUG

- Timeline: fired ≈02:25:2xZ; every phase clean (placement 0xa3500000; blob
  v2 deployed + verified; A08 repointed; restore armed); the F2 block
  entered (bc[25]=5A52F201); **pre-103 = r0=2 (the SMC WORKS from the
  payload); then SIGSEGV: `Process … SIGSEGV code=2 fltno=11 ip=0804a804
  ref=00000103`** — the marker-poll's `ldr r2,[r4]`, r4 = 0x103 (the monitor
  left the service id in a callee-saved register; `mon_call`'s asm never
  declared it; GCC kept `mkf` in r4). QNX survived (rule 1).
- **The release HAD executed** (disasm: store + dsb before the crash;
  RSTCTRL readback 0) — and produced **NO blob landing in 6+ min** ([31]
  unchanged) ⇒ the payload-time release does NOT wake CPU1 onto [A08]'s
  target (a hard datum; F3 input).
- Recovery 02:33:19Z: A08 → 4a326b00 (readback ✓); RSTCTRL 0→1 (readback ✓
  — the register is NS-readable and returns the last write: NEW channel);
  smctest --103 r0=2.
- **The deferred WDT2 cycle ≈02:34:5x-02:35:0x** (probe saw the link drop;
  keeper re-handshake 02:36:33-34; user: RED + the usual USB chain) — it
  fired INTO the restored safe config ⇒ **CLEAN warm cycle** (bc page +
  blob v2 intact; A08 canonical) ⇒ safe-cell clean cycles N=2 (1 flown + 1
  accidental). OPEN THREAD: the window length (~8-8.5 min vs the 58.6 s
  house figure) + the counter model (said "not running" — not an all-clear).
- Record: `w104-run2-record.md` (+ jump.log / recovery.txt / prerun.txt /
  session.log / the poller's live log).

## THE FIX (#167)

`mon_call` (both `qnx2linux.c` + `smctest.c`): `push {r4-r11}; dsb; smc #0;
dmb; pop {r4-r11}` — the mon_call_full / SAR-blob convention; the payload
gcc accepts a clobber list but smctest's gcc rejects `r7` in clobber lists ⇒
the in-asm save is THE pattern. Disasm-verified in both binaries.
qnx2linux 30,584 B sha 827b1c0f5061; smctest 9,401 B sha 5d217e0b798d;
commit 1317e32; backups `w104-artifacts/{qnx2linux.f2 (crashed #166 — do not
fly), qnx2linux.167, smctest.167}`.

## NEW DATUMS / N-COUNTS

- Payload-side SMC 0x103: WORKS (r0=2; both eras). The monitor destroys
  callee-saved registers (r4 observed). RSTCTRL_CPU1 readable (returns the
  last write). The release → no [A08] landing. Safe-cell clean cycles: 2. A
  crashed-run WDT cycle: 1st of the class. LED: blue→magenta, frozen (the
  crash; no off/red) until the cycle's red (user). USB: none at the crash;
  the chain at the cycle.

## OPEN / NEXT (for session 22)

1. **F2 r2 — the re-fly on #167 (user-gated)**; first action = the
   pre-flight + the go-ask; capture the bc-page forensics before the flight.
2. The WDT-window calibration thread (wtgr/wps decode; the counter model;
   the wdtkick lifetime).
3. The wake-target followups → the F3 design input.
4. Draws/spectrum; the 126-family; the every-flight USB/LED logging.
5. Parked smalls: `mon_call_full` save-set audit (same class as #167);
   bc[27] datum; TASK-005 follow-ups.
6. Strategic: F3; the TLB test with CPU1 parked-alive.

## THE WRAP

- Docs: docs/03 (the r2/F2-r1 note), PROJECT_STATE (s21 block), KNOWN_ISSUES
  (s21 header), HANDOFF, this file, BOOTSTRAP_SESSION_22. Commits: 8a27280
  (#166), 1317e32 (#167) + the wrap commit.
- Mid-session corrections folded: wrap timing = user-owned; relay =
  question-driven (skill v1.7.7 + the next bootstrap).
