# BOOTSTRAP SESSION 25 (written 2026-10-11, session 24's wrap product)

## ★ NOTE FROM THE USER — THE RELAY IS QUESTION-DRIVEN (AND THE SESSION-23 AGENT)

The session-19/20/21/22/23/24 agents' contexts are aged: **relay packets are
QUESTION-DRIVEN only** — route one for a genuine unknown the current session
cannot resolve, and only after the user judges the target's context health. No
standing review packets. Session 24's agent can still answer for the W-107
flight chain (the draw decodes, the W-107i census reasoning, the pre-SMC clean's
instrumentation logic, and the three corrections it received — it is the only
holder of that in-session reasoning). No cross-check is pending at this wrap.

## ★ THE SUBAGENT MODEL (the user's standing instruction, 2026-10-09)

**Subagents spawn with the SAME model/provider as the root agent:
stepfun/step-5-preview-free via provider kilo, `--reasoning high` RETAINED.**
The skill `playbook-dev-orchestration` + `references/subagent-delegation.md`
still carry the old `deepseek/deepseek-v4.1-flash` constant — **leave them as-is
(the user's call: the change is temporary)**; the spawn deviates deliberately and
the deviation is recorded in W-107i-design.md §11. The rest of the recipe
(`--oneshot --query-file`, the bracket-pgrep discipline, the post-spawn
`sessions.model` audit) is unchanged. One caution: TASK-027's run ended in an
HTTP 429 mid-report — harvest the agent log and root-verify any claim that never
made it into a report file.

## THE ONE-PARAGRAPH STATE

Session 24 = ★★★ **THE W-107 ARC CLOSED: the parked-alive release LANDED**
(`bc[31]=0x5A52F2F2` = CPU1 executed the parked blob, the first kernel-era
landing in project history) and the kernel booted with CPU1 alive — the arc's
goal state, reached on the first `--parkjump` flight. Getting there required
crossing a wall the preflight hadn't identified: a pre-existing payload killer in
the `51→48` sweep window, which the session instrumented to the instruction
(W-107e's `bc[20]=0x100` = the pre-SMC clean's globals `l2c_ns_clean_range()`
call, page 0, GTC off — the `mem_offset64()` calls themselves are fine), then
bypassed under the park modes with the confound assessed EVIDENCED (W-107h: the
clean protects a path `g_l2on` flights never take). **Four parked-alive draws:
142 (placement) / 126 (SCU/TLB, the landing OPEN) / 142 / 111 (`setup_arch()`
returned — the family's deepest, ~73 markers short of the era's record 184/185
which ran in the held shape). 3 of 4 draws are NOT the 126 family ⇒ the
prediction (the SCU/TLB class vanishes under parked-alive) holds so far.** The
landing question's instrument went through a relocation (W-107i: the kernel
WRITES bc[31] — the W-94 snapshot — so the durable marker moved to 0x9FE000A4)
and a realized flaw (it is not cleared by a warm reset ⇒ uninformative on repeat
landings — W-107j's one-line fix is the next step). Builds #174-#177 (40,489 /
40,869 / 40,869 / 40,917 B); kernel #163 unchanged. **Ten flights, ten clean
recoveries, zero writes in every recovery, no release-class casualty.**

## THE MANDATORY READ ORDER (before any work) — v6

1. **This bootstrap** (whole — it is the state).
2. `newdocs/PROJECT_STATE.md` — **THE SESSION-24 BLOCK ONLY** (it is the top of
   the "Where the boot stands" section; read only that paragraph).
3. `newdocs/session-notes/session-24.md` (the arc's findings; the OPEN/NEXT).
4. `docs/03_DEBUGGING_SESSIONS.md` — **TAIL ONLY** (~the last 250 lines: the
   W-107 r1/r2, W-107d/e/h runs, the four PARKJUMP draws, and the W-107i design +
   the corrections). NEVER whole-file.
5. `docs/README.md` — rules 1–17.
6. `newdocs/KNOWN_ISSUES.md` — the TOP TWO headers. `newdocs/DECISIONS.md` —
   D13 + any later.
7. The skill `playbook-dev-orchestration` + `references/subagent-delegation.md`
   (see the ★ subagent note at the top — the model constant vs the standing
   instruction).
8. On demand: the W-107d/e/h/i designs, the TASK-027 preflight verdict, the four
   PARKJUMP draw records, and the three corrections (see §"THE CORRECTIONS").

## SOURCES & TREES (paths)

- Kernel tree: `~/kernel/linux` (SMP=n, maxcpus=1); pristine: `~/kernel/pristine`.
- Payload: `~/playbook-dev/kexec/` — `bash build.sh` self-sources `../qnx-env.sh`
  and builds ALL tools + qnx2linux; **smctest is NOT in build.sh** (same
  toolchain, manual one-liner). **stub3.S is now the W-107 cont's ONLY home** —
  the W-107i marker stores live there (the landed and expired branches, writing
  0x5A52F7F2 / 0x5A52F7F1 to 0x9FE000A4).
- QNX install: `~/qnx660-master` (binaries only — no QNX kernel source; the
  device-kernel evidence = `~/playbook-dev/device-binaries/*.dis`). TRM text:
  `~/playbook-dev/swpu231ap.pdf.txt`.
- Boot ROM RE: `~/playbook-dev/bootdumps-2026-09-11/`. Device binaries:
  `~/playbook-dev/device-binaries/`.
- Agent records: `~/agent-runs/` (TASK-*, W-*, w107*, w107d/e/h/i,
  w107-parkjump-draw*).
- SSH option set: `newdocs/COMMANDS.md` ~L11-15. Toolchain objdump/as:
  `~/toolchains/armv7-eabihf/bin/...` and the qnx-env-prefixed
  `arm-unknown-nto-qnx6.6.0eabi-*` (source `~/playbook-dev/qnx-env.sh`).
- **Hermes message store** (forensic sweeps): `~/.hermes/state.db` — starts
  2026-10-03; the 08-30-era text is NOT there (the era predates the store).

## THE DEVICE LINK

- The PlayBook = USB RNDIS passthrough (169.254.0.1). The door keeper
  (`python3 ~/BerryShell-V4.py hold --log ~/agent-runs/berryshell-door.log`, run
  background=true) must hold the door: verify LIVE before device work (`pgrep`
  with the bracket form `[B]erryShell` + a fresh tail line + `ssh … "echo up"`);
  ONE keeper only; it re-handshakes automatically across resets. **The keeper
  died twice mid-session (a host restart and later) and was restarted cleanly
  both times** — if it is dead, restart it; the device itself was healthy each
  time.
- **Dark-run triage:** the host's USB events = the reset discriminator (a real
  reset produces one; a stalled one produces none). Recovery: power button
  (W-73 class); battery pull LAST (it costs the DRAM evidence).
- The post-jump window (measured): off→red ≈ 68 s (the jump band — CONFIRMED on
  four parked-alive flights; the WDT anchor is the kick cadence + 58.6 s, NOT a
  function of the kernel's runtime); [red → network-back] 131–272 s; readback
  7–9 s; total clean 4–6.5 min.
- Readback battery (the standard): `90000000 0x40`; `90000040 0x40` (★ this
  covers 0x90000050/0x54 — the W-107e instrument slots — but NOT the durable
  marker); `88000080 0xf80`; `90000080 0x8`; `94000080 0x8`; `94000000 0x8`; the
  SAR set (`4A326A00 0x10` incl. **`4A326A08`**; `4A326B00 0x20`); gates
  (`4A326C2C`, `4A326C40`); `4824380C 0x4` (RSTCTRL — write-echo); `48281800 0x8`
  (AUX); PRM_RSTST `4A307B04 0x4`; PWRSTST `48243404/48243804 0x4`; the WDT2 pair
  `4a314028 0x8` (CRR+LDR — **the CRR is UP-counting: elapsed = CRR − WLDR**) +
  `4a314030 0x4` + `4a314034 0x4`; `cat /accounts/devuser/kexec-*.log`;
  `./smctest --103`. ★ **ADDED: `9FE000A0 0x8` — the durable landing marker at
  0x9FE000A4 and the abort flag at 0x9FE000A0 (BOTH words, never a partial view).**
  Decode: `~/agent-runs/w101-artifacts/decode_readbacks_generic.py`; capture with
  a plain redirect.
- **Fire runs with a FILE redirect** (`=== FIRING ===` header + the command +
  the log path into the log; no `| tee`).
- **THE RECOVERY-SCRIPT TRAP (hit in draw 4)**: a `sed`-generated capture script
  can silently mangle an inserted line (the `== BLOB ==` replacement collided
  with the path slashes and the DURABLE_MARKER section never ran). **After
  generating a capture script with sed, GREP the inserted lines and confirm the
  previous battery's sections still parse — or just read the missing slot
  directly with ssh (the device is up at that point anyway).**
- **FRESH capture path for every run** (the shared-path lesson: two runs
  appended into one recovery file and the first grep read the WRONG section).

## THE ARTIFACTS / LEDGERS

- Kernel = #163 UNCHANGED (5,223,233 B sha ee303813…; banner "#162").
- PAYLOAD = **#177 (`--parkjump`/`--parkabort` + the W-107i durable marker)**:
  `kexec/qnx2linux` 40,917 B sha **e93185babfbbc70401bfe353cd247f7cf03dc0f4756601bc8ea94f3d4a940b7c**
  (snapshotted at `~/agent-runs/qnx2linux.w107i`).
  - #176 (W-107h, the pre-SMC clean skipped; 40,869 B sha bd43ef2c12606543…;
    backup `~/agent-runs/w104-artifacts/qnx2linux.pre-w107i`).
  - #175 (W-107e, the window instrument; 40,869 B sha 1151dac3b223682…;
    backup `…qnx2linux.pre-w107h`).
  - #174 (W-107d, the no-jump control; 40,489 B sha bd43ef2c12606543…; backup
    `…qnx2linux.pre-w107e`).
  - smctest: #167 9,401 B sha 5d217e0b798d.
- ⚠ The payload LINK per-build metadata: build-id + `.strtab` temp-names +
  symtab drifts (rule-16 via the diff-group method, never cross-build sha).
- **THE CONT GREW: 308 → 352 bytes** (W-107i's two marker stores + 3 pool words;
  cont_end 0x164 → 0x190; the pool lands INSIDE the range — verified). Any
  future cont edit must re-verify the length AND the pool-in-range constraint.

## THE READBACK KEYS (compact)

- **The W-102 marker table** (bc[17] = 0x90000044): 0+0 = walk-1 death;
  1+0 = the CTRL-check gap; 1+0xFFFFFFFF = the L2-off skip; 2+0 = the inv
  loop; **2+272 = PASS COMPLETED (healthy)**. bc[16] = the count (272 = 0x110).
- **The kernel ladder (bc[1])**: 100-124 head.S → 121/122 MMU-on pair → 110
  (paging_init done) → **111 (setup_arch RETURNED — main.c:2095)** → 118 (setup
  ·c:1949) → 130-136 setup.c pairs → 140-147 (the fixup/inv region) → 150-166
  (the svm region) → 156/157 (alloc) → **163 (W-32c)** → 171-187 (rest_init →
  do_basic_setup) → **184/185 (the initcall loop — THE ERA'S DEPTH RECORD,
  W-100 r3, held shape)**.
- **The F2 ladder** (bc[25] = 0x5A52F2xx): 213 clean-continue / 283
  release-never-took / 28F no-marker+released ABORT / 2FE marker+not-held ABORT;
  bc[31] = 0x5A52F2F2 the landing marker.
- **The W-107 family (payload-era)**: bc[25] = 0x5A52F701 armed / **702 aborted
  alive**; bc[1] = 80 armed / **79 abort** / **214 parked-alive landed / 215
  no-landing re-held** (both markers are unreadable on a DEEP draw — the kernel
  ladder overwrites bc[1]).
- **THE W-107e INSTRUMENT (bc[20]/bc[21], gated to the park modes)**: bc[20] =
  0xF0 (the pre-SMC clean SKIPPED — the W-107h gate) / E1/E2 (the clean block's
  entry / both mem_offset64 returned) / 0x100+page (the globals loop) / 0x200+page
  (the stack loop) / EE done / EF the offset lookup failed; bc[21] = E1 dest / E2
  dest done / E3 buffer / E4 buffer done / E5 tramp / EE all sweeps done.
- **THE W-107i DURABLE MARKER**: **0x9FE000A4 = 0x5A52F7F2 landed / 0x5A52F7F1
  expired** (any depth — the kernel never writes it); 0x9FE000A0 = 0xAB = the
  abort flag. **It is not cleared by a warm reset and a repeat landing writes the
  SAME value ⇒ uninformative on repeat landings (W-107j fixes this).**
- **bc[31] IS A DEPTH CHANNEL (the W-107i census)**: the W-94 snapshot writes
  `memblock.memory.cnt` there (1 on a single-bank boot) ⇒ **0x5A52F2F2 = died
  pre-memblock; 1 = reached memblock init**. Payload-era: 1 = no-landing init.
- **[A08]** (0x4A326A08): canonical `4a326b00`; diverted `40309a00`. **A diverted
  A08 across a reset = the stall class — restore first.**
- **The ring discipline**: ring1 (0x90000100, count at 0x90000080 or 0x88000080)
  is a SHARED page across boots — its kernel-console text is NOT evidence about
  the current run unless the count is fresh. (This cost a wrong claim once; the
  lesson is in the w107h record's retraction.)
- **The bc-page residue trap**: the device often stays in the SAME live boot
  across runs (check the nonce). A pre-fire battery that matches the previous
  run's recovery byte-for-byte = the page is RESIDUE; the run's own frames are
  the base, and only a FRESH post-run battery can be trusted for the run's
  markers.
- **The crash signatures**: a payload SIGSEGV prints ONE line to /tmp/jump.log
  (ref = the faulting DATA address); a strand/wedge = NO line; the box dies
  within seconds; the WDT cycle recovers. The LED lit-freeze variant = N=6.

## THE CORRECTIONS (three, all received from the user in-session — the pattern)

1. **"~25 min dark"** — asserted without checking the clock; the real window was
   ~8 min. 2. **The flags-in-DRAM hex ordering** (twice) — they live in IRAM
   (0x8051…), outside the DRAM sweep. 3. **"the project's deepest draw"** — the
   era's record is bc[1]=184/185 (W-100 r3); draw 4 is the FAMILY's deepest.
   **★ The pattern to avoid: superlatives and durations asserted from a narrow
   window instead of the repo's own records. Claim the scoped truth and cite the
   record beside it.** Each is retracted IN-PLACE (the run records + docs/03),
   not deleted — a future session can see the reasoning and the correction.

## THE DECISION QUEUE FOR SESSION 25 (ranked)

1. **★ W-107j (the fresh-slot fix — my recommendation FIRST).** ONE line either
   way: the cont writes a per-run nonce into 0x9FE000A8 alongside the marker, OR
   the C block zeros 0x9FE000A4 at setup. It makes the landing question decidable
   on every subsequent draw (currently uninformative on repeat landings — the one
   realized flaw in #177).
2. **The parked-alive spectrum: draws 5+ for N≥5.** The era's depth record
   (184/185, the initcall loop, held shape) is the target; the draw-2 question
   (126 + the landing OPEN) is settled outright by W-107j's marker on the next
   126 draw (landed ⇒ the prediction fails; expired ⇒ the held shape, and the
   wake path is the finding).
3. The pre-SMC-clean mechanism memo (the exact sub-position: the line-op loop vs
   the sync poll vs the return — W-107g if the memo needs it; it is a curiosity,
   not a blocker).
4. The W-107h gap (the invisible mid-flight window on its 8-min dark) — a known
   instrument limitation, explicitly NOT to be re-chased.
5. Smalls: the 215 (poll-expired) path is still unflown; `mon_call_full`'s
   {r4,r5,r7,lr} stack audit; TASK-005 follow-ups.
6. Strategic: the 185-path; the F3-family; the memory-tree threads.

## THE STANDING RULES (the short list)

- ASK the user before every RUN (jumps/reboots/payload launches); read-only
  probes need no confirmation; hands-off during runs; the user records; ASK for
  the LED timings. **Nothing QNX-side after the GICD-off; NVRAM/RPMB
  untouchable.**
- **The user owns wrap timing:** the agent does NOT track or manage its own
  context budget, makes no proactive wrap offers; the user announces the wrap.
- **The relay is question-driven** (aged sessions): genuine unknowns only,
  health-check with the user first. Session 24's agent is reachable for the
  W-107 flight chain.
- python for every hex value; READ the actual logs (rule 13/14; never
  pattern-fill a timeline). The reader-delegate for run outputs; file-redirect
  fires; context-shaped reads.
- **The SMC rule:** any inline-asm SMC helper must save/restore r4-r11 IN the
  asm (the monitor destroys them; observed r4=0x103). gcc rejects r7 in clobber
  lists — use the push/pop form.
- **THE A08 DISCIPLINES:** every A08-touching instrument carries (a) the
  in-instrument restore + a staged recovery that restores `[A08]`-FIRST and NEVER
  hands the pen back; (b) **never release CPU1 without a valid target + an armed
  restore**; (c) after ANY wedge/crash: capture → restore → verify → battery; (d)
  prefer SYSCALL-FREE windows around release-class operations (a minimization,
  not an immunization — the 64.5 ms zero-entry strand).
- **THE GTC-RE-ENABLE RULE (TASK-027's F1, now flight-proven): any abort or
  return-to-QNX path in the pre-kernel tail must re-enable the GTC FIRST (the
  do_l2test phase-D shape) before any QNX-side I/O — otherwise a successful write
  + `wdt2_disable()` leaves a silent zombie (GTC off + WDT2 off = the manual
  power-cycle class).**
- Kernel = a clean shell; payload = `bash build.sh` (self-sources); rule-16 via
  size + disasm + byte patterns (+ the metadata family); back up before
  repacking; **the cont's length and pool-in-range constraints change with every
  cont edit**.
- git commit + push every state change; the bootstrap = the wrap product.
- **`pgrep` self-match discipline**: run stray-wrapper checks with the bracket
  form (`[j]ump\.sh`, `[B]erryShell`).
- **Subagent spawn**: the ROOT's model/provider (stepfun/step-5-preview-free via
  kilo) with `--reasoning high` — see the ★ note at the top; audit
  `state.db sessions.model` after spawning; if the route fails, run the work
  root-side with probe evidence.
