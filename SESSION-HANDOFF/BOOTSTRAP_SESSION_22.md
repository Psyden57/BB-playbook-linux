# BOOTSTRAP SESSION 22 (written 2026-10-07, session 21's wrap-up product)

## ★ NOTE FROM THE USER — THE PREVIOUS SESSIONS' AGENTS ARE REACHABLE, BUT AGED

The session-19/20 agents' contexts are >65% (2026-10-07): **relay packets are
QUESTION-DRIVEN only** — route one only for a genuine unknown the current
session cannot resolve, and only after the user judges the target's context
health. No standing review packets; fresh subagent audits replace aged-relay
reviews (precedent: the W-104-F2 design skipped its relay pass — the
TASK-015 preflight was the sole gate). The user relays; write questions down.

**No cross-check is pending at this wrap.**

## THE ONE-PARAGRAPH STATE

Session 21 = the (M) study → F2 design → preflight → **F2 r1 FLOWN (a real
find) → fix #167**. TASK-014: the (M) mechanism/lever study — ACCEPTED
(verdict MOSTLY; `~/agent-runs/TASK-014-m-mechanism-study-report.md`; five
folds + R1-first, cross-checked by session-19, folded). Key answers: release
= RSTCTRL←0 alone; verify = SMC 0x103 (bit9) + RSTCTRL readback
(NS-readable, returns the last write); NO released configuration is
*proven*-safe ⇒ the re-hold is REQUIRED; the [A08] reset-consumer is NOT in
the dumped ROM (hidden 4KB); the mainline kexec lead is weak. Then the F2
design (`~/agent-runs/W-104-F2-design.md`) → TASK-015 preflight
(GO-WITH-FIXES, folded) → build #166 → **F2 r1 (2026-10-07 ≈02:25Z): the F2
block ran; the payload SIGSEGV'd ONE INSTRUCTION after `mon_call(0x103)`
returned r0=2 — the monitor DESTROYS callee-saved registers (r4 left = the
service id 0x103); `mon_call`'s asm never declared it; the marker-poll's
`ldr r2,[r4]` aborted at ref=0x103.** QNX survived (rule 1). **The release
HAD executed pre-crash: 6+ min with A08=blob and NO marker ⇒ the
payload-time release does NOT land CPU1 on [A08]'s target** (a hard datum
for the wake-target question; F3 input). Recovery executed (A08 restored
FIRST, then the pen — readback-verified; RSTCTRL 0→1; smctest r0=2). **The
crash-era WDT2 window then fired ~8 min later INTO the restored safe config
→ a CLEAN warm cycle** (user: RED + the USB chain; keeper re-handshake
02:36:34) — safe-cell clean cycles now N=2 (1 flown, 1 accidental). **Fix
#167 SHIPPED: `mon_call` = `push {r4-r11} … pop {r4-r11}` around the SMC
(both qnx2linux + smctest; disasm-verified; gcc rejects r7 clobbers — the
in-asm save is THE pattern); qnx2linux 30,584 B sha 827b1c0f5061; smctest
9,401 B sha 5d217e0b798d; commit 1317e32.** Full run record:
`~/agent-runs/w104-run2-record.md`. Device now: fresh boot post-cycle; A08
canonical; RSTCTRL=0 (fresh-boot state); blob v2 inert in IRAM; **the crash
forensics still live in the bc page — the next payload run overwrites them;
capture first if needed.**

## THE MANDATORY READ ORDER (before any work) — v4

1. **This bootstrap** (whole — it is the state).
2. `newdocs/PROJECT_STATE.md` — the **SESSION-21 block only** (top of "Where
   the boot stands"; it ends at the next `**SESSION-` header).
3. `newdocs/session-notes/session-21.md` (the session's findings; the
   OPEN/NEXT section).
4. `docs/03_DEBUGGING_SESSIONS.md` — **TAIL ONLY** (~the last 60 lines: the
   W-104 r2/F2 r1 note + the fix). NEVER whole-file (242 K+).
5. `docs/README.md` — rules 1–17 (short; the standing ruleset).
6. `newdocs/KNOWN_ISSUES.md` — the TOP only (the session-21 + session-20
   headers). `newdocs/DECISIONS.md` — D13 (+ the earlier list if needed).
7. **The skill** `playbook-dev-orchestration` + its references (the durable
   operating manual; session-21 folds landed: the SMC-helper rule, the
   crashed-payload WDT-window rule).
8. On demand: `~/agent-runs/W-104-F2-design.md` (§11 = the fold record; the
   status line = the run outcome), `w104-run2-record.md` + the w104-run2
   files (jump.log / recovery.txt / prerun.txt — the crash forensics),
   `TASK-014-*`, `TASK-015-*`, `W-104-design.md`, the payload sources.
Reading discipline: targeted windows, batch reads, python for every hex
value, never re-ingest what this bootstrap/skill already digest.

## SOURCES & TREES (paths)

- Kernel build tree: `~/kernel/linux` (SMP=n; maxcpus=1) — pristine ref:
  `~/kernel/pristine`.
- Payload: `~/playbook-dev/kexec/` — `bash build.sh` self-sources
  `../qnx-env.sh` and builds ALL tools + qnx2linux; **smctest is NOT in
  build.sh** (same toolchain, manual one-liner).
- QNX source: `~/qnx660-master` | TRM text: `~/playbook-dev/swpu231ap.pdf.txt`.
- Boot ROM RE: `~/playbook-dev/bootdumps-2026-09-11/` (BOOTROM-RE.md + *.bin
  + memdump3 companions) | device binaries: `~/playbook-dev/device-binaries/`.
- Agent records: `~/agent-runs/` (TASK-*, W-*, wNNN-*, fold homes:
  `w104-f1-crosscheck.md`, `TASK-014-crosscheck-s19.md`).
- SSH option set: `newdocs/COMMANDS.md` ~L11-15. Toolchain objdump:
  `~/toolchains/armv7-eabihf/bin/arm-buildroot-linux-gnueabihf-objdump`
  (`arm-linux-objdump` symlinks to it).

## THE DEVICE LINK (unique to this VM — not in the repo docs)

- The PlayBook = USB RNDIS passthrough (169.254.0.1). The door keeper
  (`python3 ~/BerryShell-V4.py hold --log ~/agent-runs/berryshell-door.log`,
  background) must hold the door: verify LIVE before device work (pgrep + a
  fresh tail line + `ssh … "echo up"`); ONE keeper only; it re-handshakes
  automatically across resets (log pair ~1 s apart).
- **Dark-run triage:** the user's host USB events = the reset discriminator
  (a real cycle produces the chain; a stall produces NONE — calibrated).
  Recovery: power button (W-73 class); battery pull last.
- **A crashed payload is NOT a stable end-state (new, session 21):** a
  user-mode abort leaves QNX alive AND a possibly-live WDT situation — the
  crash-era window can fire MINUTES later. After capturing the forensics,
  execute the staged restore IMMEDIATELY (A08 first, then the pen); a cycle
  that arrives post-restore lands in the safe cell (clean). RED at the
  eventual cycle = the WDT2 signature; the counter readback is not an
  all-clear.
- **The post-jump window (measured):** off→red ≈ 68 s band (13 instances);
  [red → network-back] 131–272 s; readback 7–9 s; total clean 4–6.5 min.
- **Readback battery (the standard):** `90000000 0x40`; `90000040 0x40`;
  `88000080 0xf80`; `90000080 0x8` (+ full ring2 `90000080 0xf80`);
  `94000080 0x8`; `94000000 0x8`; the SAR set (`4A326A00 0x10`, `4A326B00
  0x20`); `40309A00 0x8`; gates (`4A326C2C 0x4`, `4A326C40 0x4`); **NEW:
  `4824380C 0x4` (RSTCTRL_CPU1 — NS-readable, returns the last write; 1 =
  held)**; PRM_RSTST `4A307B04 0x4` (PRE/POST; never W1C). F2 flights add:
  `cat /accounts/devuser/kexec-f2.log`; `cat /accounts/devuser/kexec-bc.log`;
  `./smctest --103` (deploy the fresh smctest at recovery — /tmp is wiped).
  Decode: `~/agent-runs/w101-artifacts/decode_readbacks_generic.py`; capture
  with a plain redirect.
- **Fire runs with a FILE redirect** (`=== FIRING ===` header + the command
  + the log path into the log; no `| tee`).

## THE ARTIFACTS / LEDGERS

- **Kernel = #163 UNCHANGED** (5,223,233 B sha ee303813…; UTS banner "#162";
  kexec/kernel/zImage).
- **PAYLOAD = #167 (THE SMC-CLOBBER FIX): `kexec/qnx2linux` 30,584 B sha
  827b1c0f5061.**
  - #166 (the crashed F2-r1 build; 30,576 B): backup
    `w104-artifacts/qnx2linux.f2` — **DO NOT RE-FLY**.
  - The fix: `mon_call`'s asm = `push {r4-r11}; dsb; smc #0; dmb; pop
    {r4-r11}` (the monitor destroys registers; gcc rejects r7 clobbers —
    the in-asm save is THE pattern).
  - smctest: 9,401 B sha 5d217e0b798d (backup `smctest.167`).
  - ⚠ The payload LINK carries a ~6-byte build-varying stamp: rule-16 via
    size + disasm + byte patterns, NOT cross-build sha.
- **The W-104-F2 instrument (in #167):** blob v2 (the 7-word marker store →
  bc[31]=0x5A52F2F2) + A08→blob + the cont restore (params[4], unchanged) +
  the F2 block (fresh kick → release → bounded marker poll → 0x103 →
  re-hold ≤3(+3) → the outcome table; NEVER-JUMP abort matrix with A08
  restore + the persistent `/accounts/devuser/kexec-f2.log`); channels
  bc[25] (ladder) / bc[31] (marker).
- Records: `W-104-F2-design.md` (status = the r2-next line); `w104-run2-record.md`
  + the w104-run2 file set; the `TASK-014-*` / `TASK-015-*` chains; the
  W-103 set; the F1 set (`w104-run1-*`).
- Repo state at the session-21 wrap: commits 8a27280 (#166) + 1317e32 (#167)
  + the wrap commit — clean.

## THE READBACK KEYS (compact — details in the records)

- **The W-102 marker table** (bc[17] = 0x90000044): 0+0 = walk-1 death;
  1+0 = the CTRL-check gap; 1+0xFFFFFFFF = the L2-off skip; 2+0 = the inv
  loop; **2+272 = PASS COMPLETED (healthy)**. bc[16] = the count (272).
- **The F2 ladder (bc[25] = 0x90000064):** transients 0x5A52F201 (entered) /
  0x202 (marker seen) / 0x280 (no-marker) / 0x203 (re-hold attempted);
  finals: **0x5A52F213 = CLEAN-continue; 0x5A52F283 = release-never-took
  (continue, F1-class); 0x5A52F28F = no-marker+released ABORT (NO JUMP);
  0x5A52F2FE = marker+not-held ABORT.** bc[31] = 0x5A52F2F2 = the landing
  marker (CPU1-written; live-poll + shallow-draw post-run).
- The mirror triple (bc[1]/bc[2]/mirror) discriminates the PB_MMU_BC ladders;
  the 126-family death window = mmu.c:2059→2061 (early_fixmap_shutdown);
  the trio 126/0x17e/0x27e (flipped draws: bc[18] ≥0xa3000000-class,
  bc[6]=e8000000).
- ring3.count = 0x5A01 + ring1.count (7/7 exact); ring text both-ends
  verified via the decoder.
- **The crash signature (new):** a payload SIGSEGV prints ONE line to
  /tmp/jump.log: `Process N (qnx2linux) terminated SIGSEGV code=2 fltno=11
  ip=… ref=…` — **ref = the faulting DATA address** (0x103-class = an
  SMC-clobbered register; READ IT). QNX stays up; /tmp/jump.log + the bc
  page survive for forensics.
- RSTCTRL_CPU1 (0x4824380C): readback 1 = held / 0 = released (returns the
  last write). smctest --103 r0: bit9 SET = released / CLEAR = held
  (contested semantics — prefer the RSTCTRL read; use both).

## THE DECISION QUEUE FOR SESSION 22 (ranked)

1. **F2 r2 — THE RE-FLY on #167 (user-gated). FIRST ACTION: verify the
   keeper live + the stray/wp check + the state battery, then present the
   go-ask and fire `PAYLOAD_MODE=--f2rel ./poll-jump.sh zImage` (file
   redirect; the skeleton `w104-run2-record-skeleton.md` is ready — pre-run
   baseline + the outcome checklist). Capture anything needed from the bc
   page BEFORE the flight (the crash forensics are still there — the run
   overwrites them).** Expected: the first F2-valid datapoints — the landing
   marker (yes/no) and the cycle outcome; a no-marker+released ABORT is
   LIKELY and safe (NO JUMP — handle per the design §5/§7).
2. **The WDT-window calibration thread (offline):** reconcile the F2-r1
   cycle (~8-8.5 min after the last recorded kick) vs the 58.6 s house
   figure; decode `wtgr=0000666e wps=00000008`; the counter-readback model
   (4a31402c = ffe2b400 uniform during a window-maybe-armed period); the
   QNX-side `omap4430-wdtkick` lifetime question. Inputs: `w104-run2-jump.log`,
   the keeper timeline, `device-binaries/omap4430-wdtkick`, docs/README rule 6.
3. **The wake-target followups:** the no-landing datum + the (M)-study
   Q4/L2 residuals → feed the F3 design (kernel-side re-hold) and the
   F2-r2 outcome read.
4. Draws remain available (--l2on; W-102 r3+) for the spectrum; the
   126-family + CMA-failure tracking; the every-flight USB/LED first-class
   logging.
5. Parked smalls: **`mon_call_full` (cacheops.S) pushes only {r4,r5,r7,lr} —
   audit/extend to the full set (the same class as #167)**;
   bc[27]=0xf5000002 datum; TASK-005 follow-ups.
6. Strategic: F3; the TLB test with CPU1 parked-alive; the 126-family wedge;
   the 185-path.

## THE STANDING RULES (the short list — the skill has the full discipline)

- ASK the user before every RUN (jumps/reboots/payload launches); read-only
  probes need no confirmation; hands-off during runs; the user records; ASK
  for the LED timings. **Nothing QNX-side after the GICD-off; NVRAM/RPMB
  untouchable.**
- **The user owns wrap timing:** the agent does NOT track or manage its own
  context budget, makes no proactive wrap offers; the user announces the
  wrap.
- **The relay is question-driven** (aged sessions): genuine unknowns only,
  health-check with the user first.
- python for every hex value; READ the actual logs (rule 13/14; never
  pattern-fill a timeline). The reader-delegate for run outputs;
  file-redirect fires; context-shaped reads.
- **The SMC rule (new):** any inline-asm SMC helper must save/restore
  r4-r11 IN the asm (the monitor destroys them; observed r4=0x103). gcc
  rejects r7 in clobber lists — use the push/pop form.
- **The restore-before-reset discipline: any A08-touching work carries it**
  (F1 validated it; #167's crash-recovery made an accidental WDT cycle land
  clean — A08 FIRST, then the pen).
- Kernel = a clean shell; payload = `bash build.sh` (self-sources); rule-16
  the SHIPPED artifact (payloads: size + disasm + patterns — the
  build-stamp caveat); back up before repacking.
- git commit + push every state change; the bootstrap = the wrap product.
