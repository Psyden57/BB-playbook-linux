# BOOTSTRAP SESSION 21 (written 2026-10-06, session 20's wrap-up product;
# revision 2, same day — the session-21 feedback folds: the SOURCES map,
# boundary markers, the standing-set note, the crosscheck-file home)

## ★ NOTE FROM THE USER — THE PREVIOUS SESSIONS' AGENTS ARE REACHABLE

You can ask the previous sessions' agents (the session-19 agent is alive)
any questions — write the question down; the user relays it and brings the
answer back. **THE F1 CROSS-CHECK LANDED (session-19, 2026-10-06 — folded at
wrap; FULL TEXT: `~/agent-runs/w104-f1-crosscheck.md`):** F1 stands
cross-checked (the record verified; the chain sharpened to the no-fault
link; the blob = INERT — pointer-driven, not blob-driven; C3 parked as a
diagnostic only, never a route). **The agreed next step = the (M)
MECHANISM/LEVER STUDY FIRST** (offline): (i) when/how [A08] is consumed at
reset (the ROM-RE 0x103/0x107 sites; the r1-vs-F1 deltas); (ii) the exact
release/re-hold register+service sequences (+ the SMC 0x103 AUX_CORE_BOOT0
verify reads — the hold state is verifiable in-flight); (iii) stall =
value-driven vs target-driven (the mainline kexec DRAM-target lead — anchor:
`omap-mpuss-lowpower.c:463-501`). Then the F2 delta on (M)'s answers. **HARD
GATE: no A08-touching flight without the re-hold/verify discipline** (F2's
only design-clean landing: release → park → RE-HOLD → verify → cont-restore
→ reset; an abort after a release must retry the hold + flag
"release-not-reheld" as a recover-before-anything state — the pen bit
persists across warm resets. Config matrix: held+canonical = clean [F1, all
history]; held+diverted = stall [r1]; released+canonical = the W-73
resurrection mode; released+diverted = untested). Route NEW subagent briefs
through the relay for a pre-spawn review when offered, and send any report
verdict that gates a build/draw back through it for a cross-check.

## THE ONE-PARAGRAPH STATE

Session 20 = THE W-104 ARC, START TO FINISH: the redesign (the reset-safe
neutralization = the W-103 instrument + the cont PRE-KERNEL RESTORE, armed
only by --sarrep) → relay review (GO + folds) → TASK-013 preflight
(GO-WITH-FIXES, folded) → build #165 (29,108 B) → **★ F1 (flown 23:09:07Z) =
CLEAN RECOVERY: the W-103 r1 reset-cycle stall did NOT reproduce with the
restore in place — restore-before-reset VALIDATED at N=1.** The run itself =
a flipped-bucket 126-family draw (3rd instance; 126/0x17e/0x27e; inside
early_fixmap_shutdown) with a NEW CMA-reserve-failure sub-shape (empty
dma_mmu_remap; console 1181 chars ending at PB-ADJ#2), the fixup instrument
healthy again (272/2), and — new — **the park blob SURVIVED the cycle at
40309A00** (contra the r1 post-button read). PRM_RSTST (0x4A307B04) is
NS-readable (0→0, inconclusive — the boot consumes the sticky bits; the
USB/LED stays the primary reset discriminator). The USB calibration is
settled: normal cycles produce host USB events (bootloader/RNDIS/devmode).
The F1 cross-check (session-19) landed at wrap: the record verified + the
chain sharpened (the no-fault link); the ladder now begins with the (M)
mechanism study (see the user-note section). **Repository state at wrap:
`f0a8ed4` (clean, pushed) + this revision's commit (top of `git log`).**

## THE MANDATORY READ ORDER (before any work) — TIGHTENED v3

1. **This bootstrap** (whole — it is the state).
2. `newdocs/PROJECT_STATE.md` — the **SESSION-20 block only** (it ends at
   the SESSION-19 header); older blocks on demand.
3. `newdocs/session-notes/session-20.md` (the session's findings; the
   OPEN/NEXT section).
4. `docs/03_DEBUGGING_SESSIONS.md` — **TAIL ONLY** (~the last 120 lines: the
   W-104 F1 note + the r1 corrections). NEVER whole-file (235 K+).
5. `docs/README.md` — rules 1–17 (short; the standing ruleset).
6. `newdocs/KNOWN_ISSUES.md` — the **current-era headers only (SESSION-20 →
   SESSION-17)**; stop before the older ones. `newdocs/DECISIONS.md` — D13
   (+ the earlier list if needed).
7. **The skill** `playbook-dev-orchestration` + its references (the durable
   operating manual; session-20 folds landed: the SAR/reset line updated
   with the F1 validation, the payload build-stamp rule, the battery spec).
8. On demand: `~/agent-runs/W-104-design.md` (§8 = the ladder; §5 = the
   readback semantics), `w104-run1-record.md` + the w104 files,
   **`w104-f1-crosscheck.md` (the full F1 cross-check text — the
   load-bearing fold home)**, TASK-013 files, the payload sources.

Standing-set note: this v3 order SUPERSEDES the older standing set
(HANDOFF/DEVELOPMENT/SETUP, PROJECT_OVERVIEW/ARCHITECTURE,
PLAYBOOK-REFERENCE §5/§8/§10, COMMANDS.md) for boot — those are on-demand
now; the safety core (NVRAM/RPMB) is inline in the standing rules below;
HANDOFF.md RECEIVES fold commits (f0a8ed4 touched it) — when a pointer
matters, check `git log -p newdocs/HANDOFF.md`.
Reading discipline: targeted windows, batch reads, python for every hex
value, never re-ingest what this bootstrap/skill already digest.

## SOURCES & TREES (the map — inline, so no hunts)

- kernel build tree: `~/kernel/linux` (SMP=n, maxcpus=1) · reference copy:
  `~/kernel/pristine`
- payload: `~/playbook-dev/kexec/` (source `~/playbook-dev/qnx-env.sh`
  BEFORE payload builds; kernel builds = a clean shell)
- QNX sources: `~/qnx660-master` · TI TRM text: `~/playbook-dev/swpu231ap.pdf.txt`
- boot ROM RE: `~/playbook-dev/bootdumps-2026-09-11/` (`BOOTROM-RE.md` +
  `bootrom-lower-0x40020000.bin` + `omap4430-bootrom-0x40028000.bin` + the
  `.memdump3.txt` companions)
- device binaries: `~/playbook-dev/device-binaries/` (`devpm-omap4.dis/.so`)
  · agent records: `~/agent-runs/` (`TASK-*`, `W-*`, `wNNN-*`)
- ssh option set (inline): `ssh -o StrictHostKeyChecking=no -o
  HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o
  MACs=+hmac-sha1 -o ConnectTimeout=8 -i ~/playbook-dev/rsa
  root@169.254.0.1` (full reference: `newdocs/COMMANDS.md` §Device connection)

## THE DEVICE LINK (unique to this VM — not in the repo docs)

- The PlayBook = USB RNDIS passthrough (169.254.0.1). The door keeper
  (`python3 ~/BerryShell-V4.py hold --log ~/agent-runs/berryshell-door.log`,
  background) must hold the door: verify LIVE before device work
  (`pgrep -af BerryShell-V4` + a tail line + the inline ssh set above,
  `"echo up"`). One keeper only. The user sometimes UNPLUGS the tablet — no
  RNDIS = plug it back first.
- **Dark-run triage:** check the USER'S HOST USB events — a real reset
  produces the bootloader/RNDIS/devmode chain; a stall produces NONE (the
  r1 signature; CALIBRATED 2026-10-06). Recovery from a stall: power-button
  hold (W-73 class); battery pull last.
- **The post-jump window (measured):** off→red ≈ 68 s (the band; 13
  instances); [red → network-back] 131–272 s; readback 7–9 s; total clean =
  4–6.5 min. F1's numbers: off→red 68; reboot-wait 163 s; total 4 m 07.
- **Readback battery (the standard):** `memdump3 90000000 0x40` +
  `90000040 0x40` + `88000080 0xf80` + `90000080 0x8` + `94000080 0x8` +
  `94000000 0x8` + the SAR set (`4A326A00 0x10`, `4A326B00 0x20`), `40309A00
  0x8`, gates (`4A326C2C 0x4`, `4A326C40 0x4`), **`4A307B04 0x4` (PRM_RSTST;
  readable — read PRE and POST, never write-clear)**. Decode with
  `~/agent-runs/w101-artifacts/decode_readbacks_generic.py` (text at
  raw[0x79:]; check BOTH ends; the skill's `scripts/decode_readback.py` =
  the same lineage — the w101 generic decoder is the repo's run-era
  authority for these battery files). Capture with a plain redirect.
- **Fire runs with a FILE redirect** (`=== FIRING ===` header + command +
  log path into the log; no `| tee`). The reader-delegate or root authors
  the record afterwards.

## THE ARTIFACTS / LEDGERS

- **Kernel = #163 UNCHANGED** (5,223,233 B sha `ee303813…`; UTS banner
  "#162"; kexec/kernel/zImage). Backups in w102-artifacts/.
- **PAYLOAD = #165 (the W-104 build): `kexec/qnx2linux` 29,108 B sha
  `08fc7bbf…`.** Normal `--l2on` flights are RESET-SAFE (the restore is
  params[4]-gated; skipped); the instrument = `PAYLOAD_MODE=--sarrep`.
  Backups (LOCAL-ONLY): `~/agent-runs/w104-artifacts/qnx2linux.pre-w104`
  (28,892 = the gated pre-W-104) and `.../qnx2linux.w104` (the flown build).
  ⚠ The payload LINK carries a **~6-byte build-varying stamp** (near EOF):
  rule-16 via size + disasm + byte patterns, NOT cross-build sha.
- **The W-104 instrument:** blob + repoint (as W-103) + **the cont restore**
  (stub3.S; params[4] = 0x4A326B00 armed only by the --sarrep block;
  restores A08 microseconds before the kernel). F1 = flown + clean.
- **F1 records:** `~/agent-runs/w104-run1-record.md` (+ skeleton, session
  logs, battery, led, ring-decode files); the cross-check:
  `w104-f1-crosscheck.md`.
- Records inventory: W-104-design.md; TASK-013 files; w104-run1-*; the W-103
  set (design, r1 record, recovery); TASK-008/010/011/012; the payload
  backups.

## THE READBACK KEYS (compact — details in the records)

- **The W-102 marker table** (bc[17] = 0x90000044): 0+0 = walk-1 death;
  1+0 = the CTRL-check gap; 1+0xFFFFFFFF = the L2-off skip; 2+0 = the inv
  loop; **2+272 = PASS COMPLETED (healthy)**. bc[16] = the count (272).
- **0x3E7 in bc[2] = the probe's pass counter (BENIGN, expected pre-C).**
- The mirror triple (bc[1]/bc[2]/mirror) discriminates the PB_MMU_BC ladders;
  the 126-family death window = mmu.c:2059→2061 (early_fixmap_shutdown).
- ring3.count = 0x5A01 + ring1.count (7/7 exact); ring text both-ends
  verified, text at raw[0x79:].
- The nonce bc[15] = the freshness chain (interim arm value → final; the
  final lands with the jump-phase steps).
- **PRM_RSTST 0x4A307B04** (bits: COLD 0 / WARM_SW 1 / MPU_WDT 3 / EXTERNAL
  5): readable; PRE=POST=0 observed (CLOSED as-methoded — the boot consumes
  the sticky bits before SSH returns; do not re-chase without a new
  timing/method). USB/LED remains the primary reset discriminator.
- The 126-family trio for F1: bc[1]=126 + bc[2]=0x17e + mirror=0x27e, with
  the flipped placement bc[18]=aac00000 / bc[6]=e8000000.

## THE DECISION QUEUE FOR SESSION 21 (ranked)

1. **THE (M) MECHANISM/LEVER STUDY FIRST** (offline; per the landed F1
   cross-check — full text: `~/agent-runs/w104-f1-crosscheck.md`): the
   reset-time [A08] consumption (ROM 0x103/0x107 roles; the r1-vs-F1
   deltas); the release/re-hold sequences + the SMC 0x103 verify reads;
   value-vs-target (the kexec DRAM lead — anchor `omap-mpuss-lowpower.c:463-501`).
   Then the F2 delta design (the re-hold centerpiece; the config matrix —
   released+canonical = W-73 mode, released+diverted = untested; the
   abort-exposure design; the pen-bit-persists hazard). **HARD GATE: no
   A08-touching flight without the re-hold/verify discipline.** Design →
   relay → preflight, as always.
2. **F1-repeat: no dedicated flight** — the USB/LED signature captured on
   every future flight supplies the N=2 datapoint if a stall recurs.
3. Draws remain available (--l2on; W-102 r3+) for the spectrum.
4. The 126-family + the CMA-failure sub-shape tracking (watch-item).
5. Parked smalls: the TASK-005 follow-ups (the early_write breadcrumb + F1 —
   see the TASK-005 files); bc[27]=0xf5000002 (datum recorded).
6. Strategic: F2/F3; the 126-family wedge; the 185-path.

FIRST ACTION (session 21, as recorded): the (M) study — brief (TASK-014)
→ relay pre-spawn review → spawn on GO. Nothing A08-touching fires until
(M) lands; queue items 3–6 are parked behind it.

## THE STANDING RULES (the short list — the skill has the full discipline)

- ASK the user before every RUN (jumps/reboots/payload launches); read-only
  probes need no confirmation; hands-off the device during runs; the user
  records; ASK for the LED timings. **Nothing QNX-side after the GICD-off;
  NVRAM/RPMB untouchable.**
- python for every hex value; READ the actual logs (rule 13/14; never
  pattern-fill a timeline). The reader-delegate for run outputs;
  file-redirect fires; context-shaped reads.
- **The restore-before-reset discipline: any A08-touching work carries it**
  (F1 validated it; the abort paths must restore).
- Kernel = a clean shell; payload = `source qnx-env.sh`; rule-16 the SHIPPED
  artifact (payloads: size + disasm + patterns — the build-stamp caveat);
  back up before repacking.
- git commit + push every state change; the bootstrap = the wrap product;
  wrap at 40–60% context (the HALLUCINATION-drift guard — offer the wrap at
  natural breaks; respect a decline).
