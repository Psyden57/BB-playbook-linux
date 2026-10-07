# BOOTSTRAP SESSION 23 (written 2026-10-07, session 22's wrap-up product)

## ★ NOTE FROM THE USER — THE PREVIOUS SESSIONS' AGENTS ARE REACHABLE, BUT AGED

The session-19/20/21 agents' contexts are aged (2026-10-07): **relay packets are
QUESTION-DRIVEN only** — route one only for a genuine unknown the current
session cannot resolve, and only after the user judges the target's context
health. No standing review packets; fresh subagent audits replace aged-relay
reviews. The user relays; write questions down.

**No cross-check is pending at this wrap.**

## THE ONE-PARAGRAPH STATE

Session 22 = F2 r2 → TASK-016/017 → W-105 → the ROUND-3 archaeology → **W-106 =
★ THE [A08] LANDING** → wrap. **F2 r2 (W-104 run 2b; `w104-run2b-record.md`):
RELEASE-NEVER-TOOK** (bc[25]=5A52F283; 0x103 unchanged; sp=0) + NO SIGSEGV
(#167 held; the poll executed) + a **CLEAN cycle**; the draw died **pre-stext**;
the LED "off" = the reset-dark (the lit-freeze variant, N=1); the payload
**stamp family refined** (cross-build payload deltas = build-id + `.strtab`
temp-names + symtab drifts — code bytes identical; verify via the diff-group
method, NEVER cross-build sha). **TASK-017** (the release-mechanism re-audit:
RSTCTRL = a level + the 1→0 transition need; the 0x103/RSTCTRL category error;
the write-echo caveat) → **W-105** (`--engage`, AUX-armed; build #168;
`w105-record.md`): the block executed through the RELEASE (bc[25]=5A52F303 +
bc[1]=58 = one statement) and **the box WEDGED at the first post-release kernel
entry** (no re-hold; no marker); **the fresh-kick WDT2 window fired at +111 s
(= kick+58.6 ±1; the user's LED OFF)** → a CLEAN cycle; **A08 never touched**;
the staged recovery = zero writes. **THE ROUND-3 ARCHAEOLOGY**
(SESSION-HANDOFF.md:295–313 + session-01.md:104) corrected the mechanism:
**the CPU1 WARM release follows `[A08]` (the restoration chain → the
trampoline; "always resumes QNX" when canonical) — AUX_CORE_BOOT_0/1 are the
cold-boot pen ONLY**; hold-alone = survivable (08-30 #1); release-on-live = the
recorded danger (08-30 #2: hard crash, no watchdog, MANUAL power-cycle; the
"never release without a valid target + restore" lesson). The correction chain
landed (TASK-017 dated note; TASK-014 supra-note; W-105 §13; r2b §12). →
**W-106** (`--sarrel`, build #169; `w106-record.md`): **★ THE LANDING — the
warm release DELIVERS CPU1 to the blob: `bc[31]=0x5A52F2F2` (the FIRST landing
in project history; `spinleft=109/10,000,000` ≈ µs — near-instant)** — with the
FULL hygiene (re-hold + **`[A08]`→canonical restored + readback** +
`kexec-w106.log` = `W106 seen=1 reheld=1 rst=00000001 a08=4a326b00 ladder=
5a52f4e1 spinleft=109`) completing BEFORE the wedge fired again
(post-completion; `bc_snapshot_file(65)` never landed; kexec-bc.log unchanged;
`wdt2_disable` never ran) → the still-armed WDT window (kick≈+28–30;
fire≈+88–90) → **a CLEAN cycle (canonical — the W-103-r1 STALL CLASS DEFEATED
by the restore ordering)**; the device returned fully healthy; zero-write
recovery. **The stale-resume hypothesis = defeated by the diversion (CPU1 ran
the blob); the residual RELEASE-ON-LIVE WEDGE = the top open problem (3rd
instance; variable timing) — it GATES W-107 (the jump-integration).** Records:
`w105-record.md` / `w106-record.md` + the file sets; TASK-016/017/018/019
chains; builds #168 (32,376 B sha 2963f47b2c58) / #169 (33,804 B sha
5fa43583ec3e); commits 409e464 / 0b42a9b + the wrap commit. **Device now: fully
healthy post-W-106-cycle (the boot owns everything — A08 canonical, AUX
normal, RSTCTRL=0); keeper holding.**

## THE MANDATORY READ ORDER (before any work) — v4

1. **This bootstrap** (whole — it is the state).
2. `newdocs/PROJECT_STATE.md` — the **SESSION-22 block only** (top of "Where
   the boot stands"; it ends at the next `**SESSION-` header).
3. `newdocs/session-notes/session-22.md` (the session's findings; the
   OPEN/NEXT section).
4. `docs/03_DEBUGGING_SESSIONS.md` — **TAIL ONLY** (~the last 80 lines: the
   W-105 + W-106 run notes). NEVER whole-file (242 K+).
5. `docs/README.md` — rules 1–17 (short; the standing ruleset).
6. `newdocs/KNOWN_ISSUES.md` — the TOP only (the session-22 + session-21
   headers). `newdocs/DECISIONS.md` — D13 (+ the earlier list if needed).
7. **The skill** `playbook-dev-orchestration` + its references (the durable
   operating manual; session-22 folds landed: the warm/cold path + the wedge
   classes, the syscall-free window, the restore-ordering, the stamp family,
   the write-echo).
8. On demand: `~/agent-runs/w106-record.md` + the w106 files (the landing
   forensics), `W-106-design.md` (§13), `w105-record.md` + the w105 files,
   `W-105-design.md`, `w104-run2b-record.md` (+ the r2b files/skeleton),
   `TASK-016/017/018/019-*`, `W-104-design.md`, `W-104-F2-design.md`, the
   payload sources.
Reading discipline: targeted windows, batch reads, python for every hex
value, never re-ingest what this bootstrap/skill already digest.

## SOURCES & TREES (paths)

- Kernel build tree: `~/kernel/linux` (SMP=n; maxcpus=1) — pristine ref:
  `~/kernel/pristine`.
- Payload: `~/playbook-dev/kexec/` — `bash build.sh` self-sources
  `../qnx-env.sh` and builds ALL tools + qnx2linux; **smctest is NOT in
  build.sh** (same toolchain, manual one-liner).
- QNX source: `~/qnx660-master` | TRM text: `~/playbook-dev/swpu231ap.pdf.txt`.
- Boot ROM RE: `~/playbook-dev/bootdumps-2026-09-11/` (BOOTROM-RE.md + *.bin)
  | device binaries: `~/playbook-dev/device-binaries/`.
- Agent records: `~/agent-runs/` (TASK-*, W-*, wNNN-*; the s22 sets:
  `w104-run2b-*`, `w105-*`, `w106-*`; `W-105-design.md`, `W-106-design.md`).
- The 08-30-era release-crash records: `SESSION-HANDOFF.md:295–313` (the
  ROUND-3 detail — a queue-1 input).
- SSH option set: `newdocs/COMMANDS.md` ~L11-15. Toolchain objdump:
  `~/toolchains/armv7-eabihf/bin/arm-buildroot-linux-gnueabihf-objdump`.

## THE DEVICE LINK (unique to this VM — not in the repo docs)

- The PlayBook = USB RNDIS passthrough (169.254.0.1). The door keeper
  (`python3 ~/BerryShell-V4.py hold --log ~/agent-runs/berryshell-door.log`,
  background) must hold the door: verify LIVE before device work (pgrep + a
  fresh tail line + `ssh … "echo up"`); ONE keeper only; it re-handshakes
  automatically across resets. **After a VM/ host restart: restart `hold`
  ONCE when the device is present** (it does not survive a dev-machine
  reboot).
- **Dark-run triage:** the user's host USB events = the reset discriminator.
  Recovery: power button (W-73 class); battery pull last.
- **A wedged/crashed payload run is NOT a stable end-state (s22):** the
  release-on-live wedge takes the box down within seconds; the WDT2 window
  (the instrument's fresh kick; +58.6 s canonical) recovers it. **IF the
  wedge leaves `[A08]` DIVERTED (`40309a00`), the subsequent cycle = the
  W-103-r1 STALL class — the staged recovery must restore `[A08]`→`4a326b00`
  FIRST on contact and NEVER hand the pen back (no re-release: releasing onto
  canonical = the stale-resume crash class).** W-106's ordering (restore
  BEFORE the wedge) pre-empted this — keep that discipline in every
  A08-touching instrument. RED at the eventual cycle = the WDT2 signature.
- **The post-jump window (measured):** off→red ≈ 68 s band (13 instances);
  [red → network-back] 131–272 s; readback 7–9 s; total clean 4–6.5 min.
- **Readback battery (the standard):** `90000000 0x40`; `90000040 0x40`;
  `88000080 0xf80`; `90000080 0x8` (+ full ring2 `90000080 0xf80`);
  `94000080 0x8`; `94000000 0x8`; the SAR set (`4A326A00 0x10` — **incl.
  `4A326A08` = THE A08 KEY READ**; `4A326B00 0x20`); `40309A00 0x8`; gates
  (`4A326C2C 0x4`, `4A326C40 0x4`); `4824380C 0x4` (RSTCTRL — write-echo);
  `48281800 0x8` (AUX — informational); PRM_RSTST `4A307B04 0x4` (PRE/POST;
  never W1C). F2/W-105/W-106 runs add: `cat
  /accounts/devuser/kexec-{f2,w105,w106}.log`; `./smctest --103` (deploy the
  fresh smctest at recovery — /tmp is wiped). Decode:
  `~/agent-runs/w101-artifacts/decode_readbacks_generic.py`; capture with a
  plain redirect. **The staged recovery scripts** (`w105-recovery.sh`,
  `w106-recovery.sh`) = the adapt templates.
- **Fire runs with a FILE redirect** (`=== FIRING ===` header + the command +
  the log path into the log; no `| tee`).

## THE ARTIFACTS / LEDGERS

- **Kernel = #163 UNCHANGED** (5,223,233 B sha ee303813…; UTS banner "#162";
  kexec/kernel/zImage).
- **PAYLOAD = #169 (`--sarrel`; THE [A08]-RELAY RELEASE):
  `kexec/qnx2linux` 33,804 B sha 5fa43583ec3e.**
  - #168 (`--engage`; 32,376 B sha 2963f47b2c58; backup
    `w104-artifacts/qnx2linux.pre-w106`).
  - #167 (30,584 B; backup `qnx2linux.pre-w105`); #166 crashed
    (`qnx2linux.f2` — DO NOT FLY).
  - smctest: #167 9,401 B sha 5d217e0b798d.
  - ⚠ The payload LINK per-build metadata: build-id + `.strtab` (per-link
    random temp-object names + paths) + symtab name drifts — ~5–11 B size
    deltas + diff groups in THOSE zones only; code bytes identical (the
    diff-group method; NEVER cross-build sha; `W-105-design.md` §12).
- Records: the s22 sets + skeletons + the designs' §12/§13 fold records; the
  TASK-016 (r2b record) / 017 (the re-audit) / 018+019 (the preflights)
  chains.

## THE READBACK KEYS (compact — details in the records)

- **The W-102 marker table** (bc[17] = 0x90000044): 0+0 = walk-1 death;
  1+0 = the CTRL-check gap; 1+0xFFFFFFFF = the L2-off skip; 2+0 = the inv
  loop; **2+272 = PASS COMPLETED (healthy)**. bc[16] = the count (272).
- **The F2 ladder (bc[25] = 0x5A52F2xx):** finals 0x5A52F213 = clean-continue /
  0x5A52F283 = release-never-took / 0x5A52F28F = no-marker+released ABORT /
  0x5A52F2FE = marker+not-held ABORT; bc[31] = 0x5A52F2F2 = the landing marker.
- **The W-105 ladder (0x5A52F3xx):** 301 entered / 302 held / 303 released /
  304 reheld / finals E1/E2/EF (the AUX-armed instrument — SUPERSEDED shape;
  the record is the data).
- **The W-106 ladder (0x5A52F4xx):** 401 entered / 402 held / 403 repointed /
  404 released / 405 reheld+restored / B0 repoint-fail / **E1 = ENGAGED (seen
  — THE MARKER)** / E2 = no-engagement / EF = paper.
- **`[A08]` (0x4A326A08):** canonical = `4a326b00` (the safe value); diverted
  = `40309a00` (the blob). **The warm release follows `[A08]`** (W-106-
  proven). A diverted A08 across a reset = the stall class — restore first.
- **The crash signatures:** a payload SIGSEGV prints ONE line to
  /tmp/jump.log (ref = the faulting DATA address); **the WEDGE (s22): no
  line; the box dies within seconds of the release; ssh gone; the WDT cycle
  (kick+58.6) recovers.** The LED off-vs-jump variants: a lit-freeze through
  the jump = possible (s22 N=1); cross-read the LED against the host
  timeline.
- **RSTCTRL / 0x103 semantics:** RSTCTRL readbacks = WRITE-ECHOES (not
  independent state reads); smctest bit9 = contested; prefer cross-channel
  agreement; the real state channel for the SAR/release questions = the
  register being written + the marker.

## THE DECISION QUEUE FOR SESSION 23 (ranked)

1. **THE WEDGE-MECHANISM STUDY (the top item; it GATES W-107). FIRST
   ACTION: write the study brief (TASK-020 — offline-source mining: what a
   live-CPU1 hold/reset/release does on this QNX SMP — the yank-class
   candidates (inter-core locks/SCU/GIC/timer interactions), the
   reset-adjacent side effects, the 08-30-era records
   (`SESSION-HANDOFF.md:295–313`), omap-smp/QNX sources; deliverable = a
   mechanism memo + a DISCRIMINATION-FLIGHT design (candidates: a hold-only
   control; the reset-timing variants; the syscall-free-window limits; the
   no-cycle vs cycle expectations per outcome)).**
2. **W-107 = the jump-integration** (the corrected release in the continue
   path) — GATED on queue 1; then the CPU1-parked-alive TLB test returns to
   scope.
3. The WDT-window calibration thread (2 canonical 58.6 s instances: W-105
   +111 s / W-106 ≈+88–90; the counter model; the TGR-family values).
4. **The LED stamps for W-105 + W-106 (ASK the user — both records carry
   "pending").**
5. Draws/spectrum (`--l2on`; W-102 r3+); the 126-family + CMA tracking; the
   every-flight USB/LED logging.
6. Parked smalls: `mon_call_full` save-set audit; bc[27] datum; TASK-005
   follow-ups.
7. Strategic: the 185-path; the F3-family; the memory-tree threads.

## THE STANDING RULES (the short list)

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
- **The SMC rule:** any inline-asm SMC helper must save/restore r4-r11 IN the
  asm (the monitor destroys them; observed r4=0x103). gcc rejects r7 in
  clobber lists — use the push/pop form.
- **THE A08 DISCIPLINES (s22, hardened):** every A08-touching instrument
  carries (a) the in-instrument restore + a staged recovery that restores
  `[A08]`-FIRST and NEVER hands the pen back; (b) **never release CPU1
  without a valid target + an armed restore**; (c) after ANY wedge/crash:
  capture → restore → verify → battery; (d) **prefer SYSCALL-FREE windows**
  around release-class operations (W-105's lesson; the minimization caveat:
  timer ticks still enter the kernel).
- Kernel = a clean shell; payload = `bash build.sh` (self-sources); rule-16
  via size + disasm + byte patterns (+ the metadata family); back up before
  repacking.
- git commit + push every state change; the bootstrap = the wrap product.
