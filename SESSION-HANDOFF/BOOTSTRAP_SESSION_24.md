# BOOTSTRAP SESSION 24 (written 2026-10-08, session 23's wrap product)

## ★ NOTE FROM THE USER — THE RELAY IS QUESTION-DRIVEN

The session-19/20/21/22 agents' contexts are aged (2026-10-08): **relay packets
are QUESTION-DRIVEN only** — route one only for a genuine unknown the current
session cannot resolve, and only after the user judges the target's context
health. No standing review packets.

**★★ SESSION-23'S AGENT (this session's wrapper) REMAINS REACHABLE TO
SESSION-24 — relay any questions session 23 alone can answer to IT.** It wrote
the w108b/w108c records, the TASK-020/022 mechanism chain, and the W-107
design; only it knows the in-session reasoning behind the design choices (the params[5] arm-gate vs its alternatives, the poll budget sizing,
the exposure accounting). The user relays; write questions down. No cross-check
is pending at this wrap.

## THE ONE-PARAGRAPH STATE

Session 23 = the wedge-mechanism arc END-TO-END: **TASK-020** (the offline
wedge study: verdict CPU1-YANK) → **TASK-022** (the 08-30 #1 differential +
the timer-delivery RE; store predates the era; PPI-per-CPU established) →
**W-108** (`--holdwin`, the hold-only control: **THE MERE-LOSS WEDGE — the
strand at window index 1781/2000 with NO release**; H1 favored; the WDT
recovery chain 6-for-6) → **W-108b** (`--holdslow`, the traffic-shape control:
**the strand at n=0 — the first delay(500)**; the TIMEOUT-DURATION lead; the
WDT anchor unified: fire = wdtkick's cadence + 58.6, the raw kick = a
measured no-op) → **W-108c** (`--holdidle`, the ZERO-kernel-entry spin: **the
strand WITHOUT a kernel entry at n≈2 after 64.5 ms** — the mechanism is
event-driven below the syscall layer (T5/tick/ISR); 08-30's survival = draw
luck; the latency is a lottery spanning 64 ms → 40 s → 1781 calls) → **W-107
DESIGNED + BUILT + PREFLIGHTED (GO)**: `--parkjump` (ledger **#173**, 39,197 B
sha f25d9c5cc60c…) = the release moved into the CONTINUATION (repoint →
release(+SEV) → 0x10000-budget landing poll → restore-on-every-path) leaving
CPU1 PARKED-ALIVE = SCU-coherent — the TLB-test regime; flight PENDING
(user-gated; carries the release-class risk line). Records: `w108-run1`,
`w108b-run1`, `w108c-run1` (+ their led files, skeletons, recoveries,
preruns) + W-108/108b/108c/107 designs + TASK-020/022/021/023/024/026 chains.
Commits: d9820531→5c8e4c5b→3e8eb29b→f25d9c5c (#170-#173) + the wrap commit.
Device: fully healthy post-every-cycle; **the keeper is holding** (post-w108c).

## THE MANDATORY READ ORDER (before any work) — v5

1. **This bootstrap** (whole — it is the state).
2. `newdocs/PROJECT_STATE.md` — the **SESSION-23 block only**.
3. `newdocs/session-notes/session-23.md` (the session's findings; the
   OPEN/NEXT section) — session 24 READS ITS OWN session-notes/session-23.md
   first; the file currently on disk carries the session-22-era note.
4. `docs/03_DEBUGGING_SESSIONS.md` — TAIL ONLY (~the last 60 lines: the
   W-105/106/108/108b/108c notes). NEVER whole-file.
5. `docs/README.md` — rules 1–17.
6. `newdocs/KNOWN_ISSUES.md` — the TOP two headers. `newdocs/DECISIONS.md` —
   D13 + any later.
7. The skill `playbook-dev-orchestration` + `references/subagent-delegation.md`
   (session-23 folds: the CRR up-counter, the kick-no-op, the exposure
   accounting; **the subagent route was 503-down all session — subagent work
   may need to run root-side with probe evidence until the provider
   recovers**).
8. On demand: the w108*/w107 records + designs (the arc), TASK-020/022 (the
   mechanism chain + its dated corrections), TASK-026 (the W-107 preflight
   verdict + probe evidence).

## SOURCES & TREES (paths)

- Kernel tree: `~/kernel/linux` (SMP=n, maxcpus=1); pristine: `~/kernel/pristine`.
- Payload: `~/playbook-dev/kexec/` — `bash build.sh` self-sources
  `../qnx-env.sh` and builds ALL tools + qnx2linux; **smctest is NOT in
  build.sh** (same toolchain, manual one-liner). stub3.S's cont is the
  W-107 instrument's second edit target (`$AS` = the qnx-env assembler).
- QNX install: `~/qnx660-master` (binaries only — no QNX kernel source; the
  device-kernel evidence = `~/playbook-dev/device-binaries/*.dis`). TRM text:
  `~/playbook-dev/swpu231ap.pdf.txt`.
- Boot ROM RE: `~/playbook-dev/bootdumps-2026-09-11/`. Device binaries:
  `~/playbook-dev/device-binaries/`.
- Agent records: `~/agent-runs/` (TASK-*, W-*, wNNN-*).
- SSH option set: `newdocs/COMMANDS.md` ~L11-15. Toolchain objdump/as:
  `~/toolchains/armv7-eabihf/bin/...` and the qnx-env-prefixed
  `arm-unknown-nto-qnx6.6.0eabi-*` (source `~/playbook-dev/qnx-env.sh`).
- **Hermes message store** (forensic sweeps): `~/.hermes/state.db` — starts
  2026-10-03; the 08-30-era text is NOT there (the era predates the store).

## THE DEVICE LINK

- The PlayBook = USB RNDIS passthrough (169.254.0.1). The door keeper
  (`python3 ~/BerryShell-V4.py hold --log ~/agent-runs/berryshell-door.log`,
  background) must hold the door: verify LIVE before device work (pgrep with
  the bracket form `[B]erryShell` + a fresh tail line + `ssh … "echo up"`);
  ONE keeper only; it re-handshakes automatically across resets. After a
  VM/host restart: restart `hold` ONCE when the device is present.
- **Dark-run triage:** the host's USB events = the reset discriminator (a real
  reset produces one; a stalled one produces none). Recovery: power button
  (W-73 class); battery pull last.
- **A wedged/crashed payload run is NOT a stable end-state (s23):** the
  release-on-live wedge / the strand class takes the box down within seconds;
  the WDT2 window recovers it. The 08-30 #2 shape (hard crash, no watchdog,
  MANUAL power-cycle) is the release-class worst recorded shape — W-107
  carries that class (risk line of record).
- The post-jump window (measured): off→red ≈ 68 s band; [red → network-back]
  131–272 s; readback 7–9 s; total clean 4–6.5 min.
- Readback battery (the standard): `90000000 0x40`; `90000040 0x40`;
  `88000080 0xf80`; `90000080 0x8` (+ full ring2 `90000080 0xf80`);
  `94000080 0x8`; `94000000 0x8`; the SAR set (`4A326A00 0x10` incl.
  **`4A326A08`**; `4A326B00 0x20`); `40309A00 0x8`; gates (`4A326C2C`,
  `4A326C40`); `4824380C 0x4` (RSTCTRL — write-echo); `48281800 0x8` (AUX);
  PRM_RSTST `4A307B04 0x4` (PRE/POST; never W1C); PWRSTST
  `48243404/48243804 0x4` (real state reads; TRM Table 4-26 = an UP-counted
  32-bit counter per field decode; rest baseline CPU0=02000037, CPU1=00000037
  fresh-boot); the WDT2 pair `4a314028 0x8` (CRR+LDR — **the CRR is UP-
  counting: elapsed = CRR − WLDR; docs/06 was corrected 2026-10-08**) +
  `4a314030 0x4` + `4a314034 0x4`; `cat /accounts/devuser/kexec-*.log`;
  `./smctest --103` (deploy fresh at recovery — /tmp is wiped). Decode:
  `~/agent-runs/w101-artifacts/decode_readbacks_generic.py`; capture with a
  plain redirect. The staged recovery scripts (`w107/108/108b/108c-recovery.sh`)
  = the adapt templates.
- **Fire runs with a FILE redirect** (`=== FIRING ===` header + the command +
  the log path into the log; no `| tee`).

## THE ARTIFACTS / LEDGERS

- Kernel = #163 UNCHANGED (5,223,233 B sha ee303813…; banner "#162").
- PAYLOAD = **#173 (`--parkjump`)**: `kexec/qnx2linux` 39,197 B sha
  f25d9c5cc60c1a7848ea3d0ca06ebec1838420af93ab6d0330aedb58b736ab5f.
  - #172 (`--holdidle`; 38,587 B sha 3e8eb29b…; backup
    `w104-artifacts/qnx2linux.pre-w108c`).
  - #171 (`--holdslow`; 36,808 B sha 5c8e4c5b…; `.pre-w108b`).
  - #170 (`--holdwin`; 35,133 B sha d9820531…; `.pre-w108`).
  - smctest: #167 9,401 B sha 5d217e0b798d.
- ⚠ The payload LINK per-build metadata: build-id + `.strtab` temp-names +
  symtab drifts (rule-16 via the diff-group method, never cross-build sha).
- Records: the s23 sets + skeletons + designs; the TASK chains 020-026.

## THE READBACK KEYS (compact)

- **The W-102 marker table** (bc[17] = 0x90000044): 0+0 = walk-1 death;
  1+0 = the CTRL-check gap; 1+0xFFFFFFFF = the L2-off skip; 2+0 = the inv
  loop; **2+272 = PASS COMPLETED (healthy)**. bc[16] = the count (272).
- **The F2 ladder (bc[25] = 0x5A52F2xx):** finals 0x5A52F213 = clean-continue
  / 0x5A52F283 = release-never-took / 0x5A52F28F = no-marker+released ABORT /
  0x5A52F2FE = marker+not-held ABORT; **bc[31] = 0x5A52F2F2 = the landing
  marker**.
- **The W-105 ladder (0x5A52F3xx):** 301/302/303/304 + E1/E2/EF (SUPERSEDED
  shape — the record is the data).
- **The W-106 ladder (0x5A52F4xx):** 401/402/403/404/405/B0 + **E1 = ENGAGED
  (seen) / E2 / EF**.
- **The W-108 family (payload-era, no release):** 108 = 0x5A52F1xx
  (101 entered / 102 held / 103 window / 104 done / **E1 = COMPLETE / EF = the
  marker moved**; bc[1] heartbeats 74.. and death index = bc[1]−74); 108b =
  0x5A52F5xx (501-504; bc[26] = the kick anchor, bc[27] = the master-counter
  DELTA — **the master counter is a DUD: stuck at 3 from NS**; bc[29/30] =
  PWRSTST (the g_l2on sweep writes them pre-hold — disambiguate on the pair));
  108c = 0x5A52F6xx (601-604; **bc[27] = the CRR-clock delta, bc[28] = the
  re-kick count; the PRE-hold log works with perror**).
- **THE W-107 KEYS (the next flight):** the C block writes bc[25]=0x5A52F701
  (armed) + bc[1]=80; the CONT writes **bc[1]=214 = parked-alive landed /
  215 = no-landing re-held**; bc[31]=1 (0x5A52F2F2 = landed but MISSED by the
  poll = the loud flag: read [A08] first); the params[5] (0x40309814) gate =
  0x5A52F7A7. THE DEEP-DRAW CAVEAT: the kernel ladder overwrites bc[1] as it
  climbs — 214/215 is readable only in the early marker / no-draw cases; a
  draw that ran = the regime entered.
- **`[A08]` (0x4A326A08):** canonical `4a326b00`; diverted `40309a00`. **The
  warm release follows `[A08]`** (W-106-proven). A diverted A08 across a
  reset = the stall class — restore first.
- **The wedge/strand family (s23):** the strand needs NO kernel entry
  (W-108c: a pure user-space spin died at n≈2 after 64.5 ms); the latency is
  a lottery (64 ms → ~1 s → 19-41 s → 1781 calls); the raw TGR-complement
  kick is a measured NO-OP (the family's recoveries = wdtkick's cadence +
  58.6 s; the WDT anchor = the LAST-EFFECTIVE kick + 58.6).
- **RSTCTRL / 0x103 semantics:** RSTCTRL readbacks = write-echoes (confirm the
  write landed, not the physical state); post-cycle RSTCTRL=0 = the boot's
  re-baseline (not a deviation).
- **The crash signatures:** a payload SIGSEGV prints ONE line to /tmp/jump.log
  (ref = the faulting DATA address); a strand/wedge = NO line; the box dies
  within seconds; the WDT cycle recovers. The LED lit-freeze variant = N=6
  (the LED can stay lit through the jump's freeze; the "off" can be the RESET
  dark, not the jump — cross-read the host anchors).

## THE DECISION QUEUE FOR SESSION 24 (ranked)

1. **★ W-107's FLIGHT (the first action — it is the arc's goal state):**
   stage `w107-recovery.sh` (per the design's Edit 3 spec — the log name +
   decode keys + the pen look-only rule) → keeper + stray checks → pre-fire
   battery → `PAYLOAD_MODE=--parkjump ./poll-jump.sh zImage >
   ~/agent-runs/w107.session.log 2>&1` with the `=== FIRING ===` header
   (record the fired command + log path). USER-GATED (confirm the go
   IMMEDIATELY before firing). **RISK LINE: this flight CARRIES A RELEASE —
   the release class's worst RECORDED recovery shape is the 08-30 #2 hard
   crash (wdtkick dead, NO watchdog recovery, MANUAL POWER-CYCLE).** The
   mitigations (diversion-first, restore-on-every-path, no-landing re-hold,
   the real wdt2_kick) are structural but not eliminations — the user's call.
   LED map: blue ~+5, magenta ~+2x, **off = the jump** (the kernel era), red
   = the WDT cycle at the draw's death (~68 s band). ASK for the stamps.
   DECODE: bc[1] = 214 parked-alive / 215 no-landing / an early kernel
   marker (21/211/212/213/23) = a span death; bc[31] = the loud-flag check;
   [A08] must be canonical at recovery (the gate auto-restores); the draw's
   spectrum = the TLB-test data (the prediction: the 126-family/SCU classes
   vanish; placement-lottery classes remain) — reader-delegate the record.
2. **The W-107 spectrum: N≥3 draws** for the parked-alive-vs-held comparison
   (each draw = one user-gated run; the draws are the experiment).
3. **The mechanism memo's final thread**: the T5/tick/ISR candidates (offline-
   decidable only via the NOT-VISIBLE QNX timer config); the effective-kick
   model; the lamp family (a no-landing cell = the wake-path investigation
   with a longer poll + a pre-jump [A08]/RSTCTRL snapshot).
4. The WDT-window calibration thread (the wdtkick-cadence model; the effective
   kick = the anchor; the TGR-family values).
5. Draws/spectrum (`--l2on`; W-102 r3+); the 126-family + CMA tracking; the
   every-flight USB/LED logging.
6. Parked smalls: `mon_call_full` save-set audit; bc[27] datum; TASK-005
   follow-ups. The docs/06 up-counter fix was LANDED in commit 0e4d3b6.
7. Strategic: the 185-path; the F3-family; the memory-tree threads.

## THE STANDING RULES (the short list)

- ASK the user before every RUN (jumps/reboots/payload launches); read-only
  probes need no confirmation; hands-off during runs; the user records; ASK
  for the LED timings. **Nothing QNX-side after the GICD-off; NVRAM/RPMB
  untouchable.**
- **The user owns wrap timing:** the agent does NOT track or manage its own
  context budget, makes no proactive wrap offers; the user announces the wrap.
- **The relay is question-driven** (aged sessions): genuine unknowns only,
  health-check with the user first. **Session-23's agent is REACHABLE for
  session-24 — route W-107 design-intent questions to it (see the ★ note).**
- python for every hex value; READ the actual logs (rule 13/14; never
  pattern-fill a timeline). The reader-delegate for run outputs; file-redirect
  fires; context-shaped reads.
- **The SMC rule:** any inline-asm SMC helper must save/restore r4-r11 IN the
  asm (the monitor destroys them; observed r4=0x103). gcc rejects r7 in
  clobber lists — use the push/pop form.
- **THE A08 DISCIPLINES (s22/s23, hardened):** every A08-touching instrument
  carries (a) the in-instrument restore + a staged recovery that restores
  `[A08]`-FIRST and NEVER hands the pen back; (b) **never release CPU1
  without a valid target + an armed restore**; (c) after ANY wedge/crash:
  capture → restore → verify → battery; (d) **prefer SYSCALL-FREE windows**
  around release-class operations — NOTE (s23): the call-free window is a
  MINIMIZATION, not an immunization (the 64.5 ms zero-entry strand).
- Kernel = a clean shell; payload = `bash build.sh` (self-sources); rule-16
  via size + disasm + byte patterns (+ the metadata family); back up before
  repacking. The stub3.S cont = the `$AS` flags; the pool must stay inside
  [cont_start, cont_end).
- git commit + push every state change; the bootstrap = the wrap product.
- **`pgrep` self-match discipline**: run stray-wrapper checks with the bracket
  form (`[j]ump\.sh`, `[B]erryShell`) — a `pkill -f` from your own command
  line kills your own shell.
- **Subagent spawn**: the literal slug `deepseek/deepseek-v4.1-flash`
  (never a family filter — a first-match filter picked the wrong model on
  2026-10-07); audit `state.db sessions.model` after spawning; if the route
  503s, run the work root-side with probe evidence (s23's pattern).
