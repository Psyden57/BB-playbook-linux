# BOOTSTRAP SESSION 20 (written 2026-10-06, session 19's wrap-up product)

## ★ NOTE FROM THE USER — THE PREVIOUS SESSIONS' AGENTS ARE REACHABLE

You can ask the previous sessions' agents (the session-19 agent is alive)
any questions — write the question down; the user relays it and brings the
answer back. Use this early rather than guessing (the relay has settled:
the W-101 REPLACE directive, the W-102 scoping, the TASK-008/011 brief
reviews, and the W-103 design review). Route NEW subagent briefs through
the relay for a pre-spawn review when offered, and send any report verdict
that gates a build/draw back through it.

## THE ONE-PARAGRAPH STATE

Session 19 = the CONTEXT OVERHAUL + TASK-008/009/010/011/012 + W-102 r2 +
W-103 (built + r1) + the wrap. HEADLINES: (1) **TASK-008 closed the 0x3E7
thread** (writer = probe.S:157, the probe's pass counter, terminal 999 —
BENIGN; controls + instance table in the report). (2) **W-102 r2 = THE
FIXUP REGION PASSED** — the instrument's first healthy reading
(bc[17]=2 + bc[16]=272); a FLIPPED-bucket 126-family death
(early_fixmap_shutdown; the D13 class; the triad streak broke at 2). (3)
**The run-latency model CORRECTED** (readback = 7–9 s constant; total
4:08–6:27; variance = the [reset → network-back] boot phase; jump.sh
stamps the reboot-wait; the keeper log = door-back times). (4) **The SAR
wake path mapped + probed live** (A08 = 0x4A326B00; the blob byte-verified
vs the devpm .so; gates armed; the SAR bank NS-readable; the reset/boot
flow CONSUMES the wake path — hard finding). (5) **W-103 r1 = the
neutralization flight: the write succeeded, the jump happened, THEN THE
POST-JUMP WDT CYCLE STALLED (no reset, device powered, 8 m 20 s) until the
user's power-button hold; recovery clean.** (6) **The payload is GATED:**
`--l2on` = reset-safe (the W-103 block is opt-in `--sarrep`). (7) The
standing discipline now lives mostly in the SKILL
(playbook-dev-orchestration + references) — read it as the operating
manual.

## THE MANDATORY READ ORDER (before any work) — TIGHTENED v2 (context-shaped)

1. **This bootstrap** (whole — it is the state).
2. `newdocs/PROJECT_STATE.md` — the **SESSION-19 block only** (top of
   "Where the boot stands", ~35 lines); older blocks on demand.
3. `newdocs/session-notes/session-19.md` (the session's findings; the
   OPEN/NEXT section).
4. `docs/03_DEBUGGING_SESSIONS.md` — **TAIL ONLY** (offset-read/grep the
   last ~150 lines: the TASK-008 note, W-102 r2, W-103 r1, the latency
   NOTE). NEVER whole-file (it is 235 K+).
5. `docs/README.md` — rules 1–17 (short; it is the standing ruleset).
6. `newdocs/KNOWN_ISSUES.md` — the TOP only (the session-19 + session-18
   headers). `newdocs/DECISIONS.md` — D13 (+ the earlier list if needed).
7. **The skill** `playbook-dev-orchestration` + its references (the run
   protocol, the reader-delegate, the latency model, the SAR/reset rules
   — the durable operating manual).
8. On demand: `~/agent-runs/` records (w102-run2-record.md,
   w103-run1-record.md, TASK-008/010/011/012 files); the kernel tree +
   payload sources for any build; `PLAYBOOK-REFERENCE.md` §5/§8/§10 only
   when touching the safety questions.
Reading discipline: targeted windows, batch reads, python for every hex
value, never re-ingest what this bootstrap/skill already digest.

## THE DEVICE LINK (unique to this VM — not in the repo docs)

- The PlayBook = USB RNDIS passthrough (169.254.0.1). The door keeper
  (`python3 ~/BerryShell-V4.py hold --log ~/agent-runs/berryshell-door.log`,
  background) must hold the door: verify LIVE before device work
  (`pgrep -af BerryShell-V4` + a tail line + `ssh -i ~/playbook-dev/rsa
  <the COMMANDS.md option set> root@169.254.0.1 "echo up"`). If refused:
  start ONE keeper (never two). The user sometimes UNPLUGS the tablet
  (overnight) — no RNDIS = plug it back first.
- **Dark-run triage (NEW):** check the USER'S HOST USB events — a real
  reset produces USB events (CALIBRATED 2026-10-06: bootloader re-enum +
  RNDIS + devmode reconnects on normal cycles); a stall produces NONE
  (the W-103 r1 signature). Recovery from a stall: power-button hold (the
  W-73-class fallback; battery pull = last resort).
- **The post-jump window:** link drop → ~4–5 min ARP-dark → "refused"
  seconds (keeper re-handshake) → SSH. A run's wall-clock: deploy ~40 s +
  payload ~15–25 s + WDT wait ~68 s + [reset → network-back] 2:10–4:30
  (the variance) + readback 7–9 s. jump.sh now stamps the reboot-wait.
- **Readback battery (the standard):** `memdump3 90000000 0x40` +
  `90000040 0x40` + `88000080 0xf80` + `90000080 0x8` + `94000080 0x8` +
  `94000000 0x8` (+ the RUN-SPECIFIC extras the records name). Decode with
  `~/agent-runs/w101-artifacts/decode_readbacks_generic.py` — text at
  `raw[0x79:]`; check BOTH ends (first fresh char "[", last "\n" vs
  count). Capture with a plain redirect.
- **Fire runs with a FILE redirect** (`=== FIRING ===` header + command +
  log path into the log; no `| tee`). The reader-delegate drafts the
  record afterwards.

## THE ARTIFACTS / LEDGERS

- **Kernel = #163 UNCHANGED** (5,223,233 B sha `ee303813…`; UTS banner
  "#162"; kexec/kernel/zImage). Backups in w102-artifacts/.
- **PAYLOAD = THE GATED W-103 BUILD: `kexec/qnx2linux` 28,892 B sha
  `dcba1545…`.** `--l2on` runs are RESET-SAFE (the W-103 block is gated
  OFF by default; the disasm guard = `cmp/beq` past the block). Re-flights
  = `PAYLOAD_MODE=--sarrep`. Backups (LOCAL-ONLY):
  `~/agent-runs/w103-artifacts/qnx2linux.pre-w103` (28,291 = the
  pre-W-103 build) and `.../qnx2linux.w103-ungated` (28,759 = the flown
  r1 build). **Do NOT fly `--sarrep` without the stage-1 redesign + relay
  review — it WILL stall the reset cycle (the proven r1 behavior).**
- **The W-103 finding (the key datum):** the reset/boot flow consumes
  CPU1's wake path; a parked-IRAM landing is not reset-valid. Records:
  `w103-run1-record.md` (full), `w103-recovery-readbacks.txt`,
  `w103-run1-prerun-sar.txt`, `w103-recovery.sh` (the read-first/restore
  script).
- The SAR facts (from TASK-011 + the live probe): `0x4A326A08` =
  CPU1_WAKEUP_NS_PA_ADDR (armed value 0x4A326B00); the 0x150-B trampoline
  at 0x4A326B00 = the devpm .so blob (gates at B+0x12c armed 0x810 /
  B+0x140 = 0; the tables at B+0x150/+0x250); the SAR bank = NS-R/W;
  **IRAM does not self-clear on every reset — the park blob SURVIVED a
  WDT cycle (W-104 F1); cleared only across r1's BUTTON cycle —
  reset-class-dependent, never assume clearing**; A0C = boot-mutable
  across button cycles, stable across WDT cycles (F1).
- Records inventory: TASK-008 (0x3E7), TASK-009 (w102-run2 reader),
  TASK-010 (cure routes), TASK-011 (SAR pre-read), TASK-012 (W-103
  preflight), w102-run2-record, w103-run1-record, W-103-design.md (the
  relay-approved + preflight-folded design), the payload backups.

## THE READBACK KEYS (compact — details in the records)

- **The W-102 marker table** (bc[17] = 0x90000044): 0+0 = walk-1 death;
  1+0 = the CTRL-check gap; 1+0xFFFFFFFF = the L2-off skip; 2+0 = the inv
  loop; **2+272 = PASS COMPLETED (healthy)**.
- **0x3E7 in bc[2] = the probe's pass counter (BENIGN, expected pre-C).**
  C-era deaths show `step|0x100` pair values instead (0x17e-class).
- The mirror triple (bc[1]/bc[2]/mirror) discriminates PB_MMU_BC ladders;
  the 126-family death window = mmu.c:2059→2061 (early_fixmap_shutdown).
- ring3.count = 0x5A01 + ring1.count (6/6 exact); ring text = both-ends
  verified, text at raw[0x79:].
- The nonce bc[15] = the freshness chain (arm value → final; the handoff
  usually falls after the last successful poll).

## THE DECISION QUEUE FOR SESSION 20 (ranked)

1. **Route-(a) stage-1 redesign** (the reset-valid wake problem). Inputs:
   w103-run1-record + W-103-design + TASK-011. Think: reset-valid target
   options; kernel-side control (stage 2's shape); restore-before-reset
   timing; the "cannot forge" constraint. Design → relay review →
   preflight — NO A08-touching flight before that chain completes.
2. **Draws are available and safe again** (the payload gate) — W-102 r3+
   for spectrum; the 126-class tracking continues. Each = the usual
   per-run go + the standard protocol.
3. The parked smalls: bc[27] (leave parked unless it recurs), TASK-005
   follow-ups (the early_write breadcrumb; F1).
4. The user-side pending: the USB-calibration answer (normal runs show
   host USB events?).
5. The strategic fronts: the 126-family wedge proper (with the new
   reset-consumption datum); the CPU1 release (route (a)); the 185-path.

## THE STANDING RULES (the short list — the skill has the full discipline)

- ASK the user before every RUN (jumps/reboots/payload launches);
  read-only probes need no confirmation; hands-off the device during
  runs; the user records; ASK for the LED timings.
- python for every hex value; READ the actual logs (rule 13/14; never
  pattern-fill a timeline).
- The reader-delegate for run outputs; file-redirect fires; context-shaped
  reads; the restore-default when any A08-write flies.
- NEVER NVRAM/RPMB; nothing QNX-side after the GICD-off; the park ban
  stands (W-103 r1 now explains WHY at the reset level).
- Kernel = a clean shell; payload = `source qnx-env.sh`; rule-16 the
  SHIPPED artifact; back up before repacking.
- git commit + push every state change; the bootstrap = the wrap product;
  wrap at 40–60% context (the HALLUCINATION-drift guard — the user's
  stated why; offer the wrap at natural breaks, respect a decline).
