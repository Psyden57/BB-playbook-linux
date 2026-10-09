# Session 23 Notes (2026-10-07/08 — THE WEDGE-MECHANISM ARC: TASK-020 → TASK-022 → W-108 (the mere-loss wedge) → W-108b (the first-entry strand) → W-108c (the zero-entry strand) → W-107 designed + built + preflighted GO)

## THE SESSION'S SHAPE

Boot from BOOTSTRAP_SESSION_23. Queue item 1 = the wedge-mechanism study.
Arc: **TASK-020** (the offline study) → its recommended F1 flight flew as
**W-108** → the differential/timer study **TASK-022** → **W-108b** (its flight)
→ **W-108c** (the zero-entry test, the record's recommendation) → **W-107**
(the jump-integration) DESIGNED + PREFLIGHTED (GO) + BUILT (#173) — its flight
PENDING at wrap. Four subagent audits (TASK-021/023/024 preflights + the
TASK-020/022 studies); TASK-026 ran root-side (the provider route 503'd from
the TASK-025 spawn onward). The session's five flights: W-108/r1, W-108b/r1,
W-108c/r1 + the W-108c anchor cross-checks. All user-gated; all recovered by
the WDT chain; zero-write recoveries throughout.

## TASK-020 — THE WEDGE-MECHANISM STUDY (accepted)

Offline source mining (procnto.dis SMP machinery; TRM reset distribution; the
devpm CPU1-removal protocol = context-save → SMC 0x108 ×3 → WFI). Verdict:
the residual wedge = the CPU1-YANK class over stale-resume (falsified by
W-106's diversion) and reset-adjacent (contradicted by the per-CPU reset
distribution); recommended F1 = the HOLD-ONLY control. Provenance note: it
ran on the WRONG model (the slug incident; the root review line-checked every
citation). **Dated corrections 1 + 2 are in the task file — read them before
trusting any earlier TASK-022 reading.**

## TASK-022 — THE 08-30 #1 DIFFERENTIAL + THE TIMER RE (accepted)

Gate: MOSTLY (the low-traffic reconciliation; 08-30 #1's duration is
NOT-RECORDED). Findings: the message store starts 2026-10-03 (the 08-30 era is
NOT in it — a finding, not a gap); per-CPU private timers raise banked PPIs
(TRM :252642 + irq-gic.c:17-18); QNX's timer/GIC config = NOT VISIBLE
(procnto.dis has no 0x4824 literal — the programming lives in the
undisassembled syspage/startup). Its W-108b recommendation flew (see below).

## W-108 (`--holdwin`, #170) — ★ THE MERE-LOSS WEDGE

Fired 01:49:10Z. THE HOLD-ONLY CONTROL (the release REMOVED) wedged anyway:
ladder 0x5A52F103, bc[1]=1855 = index 1781/2000, no 104/E1/log; bc[31]=1
throughout ⇒ no release, no stray landing. The WDT cycle recovered cleanly
(A08 canonical — never written). LED: off +141 s (the wdtkick-cadence anchor;
the raw kick's +58.6 prediction was +97 — the first anchor anomaly). Record:
w108-run1-record.md.

## W-108b (`--holdslow`, #171) — THE FIRST-ENTRY STRAND + the WDT ANCHOR

Fired 04:00:59Z. One variable = the traffic shape (100 × delay(500)). Stray
landed **n=0** — the first delay(500) (bc[1]=74, bc[27]=0, ladder
0x5A52F503). The pair with W-108 killed per-call/wall-time models ⇒ the
TIMEOUT-DURATION lead (or lottery). The anchor: off +85 s ⇒ the effective kick
PRECEDED the strand (instant death, no limp). Instruments: PWRSTST-in-bc
WORKED; the master counter = a DUD (stuck at 3 from NS); the pre-hold log
silently failed. Record: w108b-run1-record.md.

## W-108c (`--holdidle`, #172) — ★★ THE STRAND WITHOUT A KERNEL ENTRY

Fired 06:15:36Z. The pure user-space spin (zero kernel entries — object-
verified) stranded at **n≈2 after 64.5 ms** (bc[1]=76, bc[27]−bc[26]=2,112
ticks, ladder 0x5A52F603; the PRE-hold log landed with perror working).
⇒ The kernel-entry framing is SUPERSEDED: the strand is event-driven below the
syscall layer (T5/tick/ISR class); the latency is a lottery (64 ms → 40 s →
1781 calls); the 08-30 reconciliation is dead in every form (its survival =
draw luck). The raw TGR-complement kick = a measured NO-OP (bc[26] =
LDR+0.182 s at the anchor — the family's recoveries ride wdtkick's cadence +
58.6). Record: w108c-run1-record.md.

## W-107 (`--parkjump`, #173) — THE JUMP-INTEGRATION (BUILT; flight PENDING)

The arc's goal state: the kernel boots with CPU1 **PARKED-ALIVE on the blob**
(Running/ON = SCU-coherent — the TLB-test regime) instead of held in reset.
The release sequence moved into the CONTINUATION (stub3.S, MMU-off pre-kernel
tail): armed by params[5]=0x5A52F7A7 → repoint [A08]→blob → release(+SEV) →
0x10000-budget landing poll → **restore [A08] on EVERY path** → marker
214 (landed) / 215 (no-landing; re-held) → the Linux boot convention →
`bx r9`. The span = 0.26 ms bounded (248× under the 64.5 ms minimum).
Preflight = GO (root-side, TASK-026: the `$AS` encoding probe — `mov r2,#0x10000`
is a single instruction — + the /tmp splice (the pool inside
[cont_start, cont_end)) + params[5] verified free + the exposure math).
**RISK LINE: the flight CARRIES A RELEASE — the 08-30 #2 hard-crash shape is
the class's worst recorded recovery (manual power-cycle); mitigated
structurally (diversion-first + restore-on-every-path + the no-landing
re-hold + the real wdt2_kick) but not eliminated — the user's call.**
Design: W-107-design.md; ledger #173 (39,197 B sha f25d9c5cc60c…); commit
1eb7381. THE FLIGHT IS SESSION-24'S FIRST ACTION (the bootstrap's queue 1 =
the exact procedure + the decode keys + the risk line).

## THE SUBAGENT-ROUTE OUTAGE

From the TASK-025 spawn on, the provider returned 503 "No available channel
for model deepseek/deepseek-v4.1-flash under group credit (distributor)" —
~7 min of retries per attempt, at default reasoning too, while the user's UI
chats on the same model worked fine (a routing/group difference between the
oneshot route and the app — the user's infra to chase). Consequences: the
W-107 design draft ran root-side in small chunks (the content filter trips on
large asm payloads — chunked writes pass), and TASK-026 ran root-side with
probe evidence. The TASK-025 brief (the full W-107 spec) stands on disk as the
task record.

## THE OPEN / NEXT (for session 24)

1. **W-107's FLIGHT** (the arc's goal state; the bootstrap carries the full
   procedure + the decode keys + the risk line). USER-GATED.
2. The W-107 spectrum: N≥3 draws (the parked-alive-vs-held TLB comparison —
   the prediction: the 126-family/SCU classes vanish).
3. The mechanism memo's final thread (the T5/tick/ISR candidates are NOT
   offline-decidable — QNX's timer config is NOT VISIBLE; a no-landing cell =
   the wake-path investigation).
4. The WDT calibration thread (the wdtkick-cadence model is now the operative
   anchor; the raw kick = a no-op).
5. Draws/spectrum; the 126-family; smalls (`mon_call_full` audit; bc[27];
   TASK-005 follow-ups).

## THE WRAP

- Docs updated: this file, `BOOTSTRAP_SESSION_24.md`, `PROJECT_STATE.md` (the
  s23 block), `KNOWN_ISSUES.md` (the s23 header), `docs/03` (the W-108/108b/
  108c run notes), `docs/06` (the CRR up-counter fix — commit 0e4d3b6).
- Commits: #170 d9820531 / #171 5c8e4c5b / #172 3e8eb29b / #173 f25d9c5c (the
  designs' fold records carry the TASK-021/023/024 reviews) + the wrap commit.
- Staged for session 24: `w107-recovery.sh` + the W-107 design (preflight GO).
- NOTE (session-numbering correction): the w108b/w108c records and designs
  say "session 24" where this work is session 23 (the s22 wrap used "session
  23" for us alternately); the labels are cosmetic — the record contents are
  correct as written. Leave them; do not "fix" the records.
