# BOOTSTRAP SESSION 18 (written 2026-10-05, session 17's wrap-up product)

## ★ NOTE FROM THE USER — THE PREVIOUS SESSIONS' AGENTS ARE REACHABLE

**Remember: you can ask the previous sessions' agents (16/17) any questions
you may have.** If anything in this bootstrap, the repo, or the device state
is missing context, unclear, or you cannot figure something out — write the
question down; the user will relay it to the session-17 agent (still alive)
and bring the answer back. Use this early rather than guessing (it worked
twice in session 17: the W-101 REPLACE directive came back through the
relay).

## THE ONE-PARAGRAPH STATE

Session 17 = THE W-101 BUILD (the fixup-side per-site inv, ledger #162, UTS
"#161", pack 5,223,425 sha 47c175cb) + its FIRST TWO FLIGHTS (r1, r2).
HEADLINES: (1) W-101 = a per-site PL310 inv-only (0x770) SECOND WALK inside
the head.S inline fixup (MMU-off direct PA, CTRL-guarded, bounded 0x730
syncs every 64 + final, bc[16] = count / 0xFFFFFFFF); the W-100
start_kernel pass is REMOVED (session-16 directive: replace). Build gotcha
fixed mid-build: head.o assembles at armv6k — NO movw/movt there; constants
via ldr= pools (post-stext .ltorg, behind the `b __enable_mmu`). Rule-16 +
snapshot + commit c1340b5 all clean. (2) THE PASS RUNS LIVE: bc[16] 0→272
watched across the jump BOTH runs (fresh beyond doubt). (3) THE DRAWS: r1 =
died EARLY at the PB-ADJ#1 record's CR→LF gap (bc[1]=133); r2 = **THE FIRST
BUCKET-FLIP DRAW EVER** — placement 0xaaf00000 ⇒ kernel base 0xa8000000
(bc[6]=0xe8000000; the FDT trims the bank; the stubs-vs-variable
split-brain: W-32c force-writes the build constant over the true runtime
delta) — went DEEP (map_lowmem + the W-94 memblock snapshot captured live
for the first time) and died at the PB-CMA record's CR→LF gap (bc[1]=167).
(4) **THE CR→LF MICRO-SIGNATURE: 3 datapoints (W-99 r1, W-101 r1, W-101
r2)** — deaths in the 1-char window after a record's final '\r', at three
depths; OPEN. (5) The 68 s off→red: 7th + 8th instances (68×8). (6) TASK-005
(console silence = F1 "stops being invoked", MEDIUM-HIGH) + TASK-006
(preflight GO) accepted; ring2/3 reads added to the battery; the decode-
offset bug fixed (raw[0x78]!). The class-vs-dice question for the W-101
draws = OPEN (N=2).

## THE MANDATORY READ ORDER (before any work)

1. `newdocs/HANDOFF.md`, `DEVELOPMENT.md`, `SETUP.md` (the standing rules)
2. `newdocs/PROJECT_STATE.md` (the session-17 block = the current state)
3. `newdocs/session-notes/session-17.md` (the W-101 build + r1/r2 + the new
   instrument lessons)
4. `docs/03_DEBUGGING_SESSIONS.md` (the session-17 note + W-101 r1/r2)
5. `docs/README.md` (rules 1-17) + `newdocs/KNOWN_ISSUES.md` (the
   session-17 header + the bucket-flip/CR-LF notes)
6. `PLAYBOOK-REFERENCE.md` §5/§8/§10 (the safety model)
7. The run records: `~/agent-runs/w101-run1-record.md`,
   `w101-run2-record.md` + the raw batteries (local-only).

## THE DEVICE LINK (unique to this VM — not in the repo docs)

- The PlayBook = USB RNDIS passthrough at 169.254.0.1. SSH is CLOSED until
  the qconn door authenticates. First: test
  `ssh -i ~/playbook-dev/rsa root@169.254.0.1 "echo up"` with the option
  set from newdocs/COMMANDS.md — if it works, the BerryShell-V4 hold daemon
  is alive (check `pgrep -af BerryShell-V4` and
  ~/agent-runs/berryshell-door.log; do NOT start a second one — the door
  is single-session). If refused: `python3 ~/BerryShell-V4.py hold --log
  ~/agent-runs/berryshell-door.log` (background, persist_on_release=true)
  and wait for "Authenticated" in the log.
- **The post-jump window, measured (session 17):** after the jump the VM
  sees the link drop; then a ~4-5 MINUTE window where the interface + route
  exist but ARP gets no answer ("No route to host" from ssh; TX-only on the
  NIC). Then "Connection refused" (the door-hole) for seconds while the
  keeper re-handshakes, then SSH. jump.sh's reboot-wait loop (80 × ~9 s)
  rides ALL of it — do NOT panic, do NOT touch the device, do NOT restart
  the wrapper. If the user says "the device is up" mid-window, the VM-side
  link may still be re-attaching — verify with ping/ssh before believing
  either side.
- Observation runs = `kexec/poll-jump.sh` (never bare jump.sh):
  `PAYLOAD_MODE=--l2on ./poll-jump.sh zImage`. The live log =
  ~/agent-runs/live-TIMESTAMP.log.
- **Readback battery (session-17 updated standard):** after every run —
  `memdump3 90000000 0x40` + `90000040 0x40` + `88000080 0xf80` +
  **`90000080 0x8` (ring2 count) + `94000080 0x8` (ring3 count, NOTE:
  not run-scoped — accumulates!)** + `94000000 0x8` (mirror0). Decode with
  `~/agent-runs/w101-artifacts/decode_readbacks_generic.py <raw> <prefix>`
  (text offset = raw+0x78 — the 8-byte slip class is FIXED, keep it fixed).
  Fresh-slot discipline: watch the poll's transitions (arm-sanitize,
  nonce handoff) rather than trusting any single read.

## THE ARTIFACTS / LEDGERS

- **kernel #162 pack = 5,223,425 B sha256 47c175cb (W-101; CURRENT —
  kexec/kernel/zImage; backups in ~/agent-runs/w101-artifacts/, incl. the
  pre-w101 pack f69f4539).** UTS banner "#161 Mon Oct 5 17:12:07 -03 2026"
  (= the in-tree counter; the project ledger = #162 — the split holds).
- Kernel tree = the #162 build state; kernel-patches/ = current snapshot
  (full-regen format; apply-checked; timestamp-strip in the recipe).
- Run records: ~/agent-runs/w101-run1-* and w101-run2-* (records, led
  files, raw batteries, session logs), live-20261005-17*.log.
- TASK-005/006 = report + brief + root review, in ~/agent-runs/.
- The r1/r2 LED delta set: 68×8 (the bootloader-phase constant).

## THE DECISION QUEUE FOR SESSION 18 (ranked)

1. **W-101 draws r3+ (the class-vs-dice question).** N=2 (r1 early-CR-death,
   r2 bucket-flip-CR-death) — the spectrum will say whether the early
   classes are dice or a new modal. Watch EVERY ring tail for the CR→LF
   signature. No artifact change = pure draws (one variable = none).
2. **The bucket-flip decision:** (a) payload guard (forbid placements
   ≥0xa8000000 — today's buf_placement_bad reserves only [0xa0000000,
   0xa1000000)); (b) accept + document (draws as-is); (c) the W-32c
   question (should it force the BUILD constant, or the RUNTIME delta it
   could read from bc[6]? — design note only). If (a): a payload build +
   TASK-style audit cycle.
3. **The CR→LF signature audit (offline):** what runs in the window right
   after a console write completes (early_write → pb_flush_rings →
   printk plumbing) that could die there — candidate: a subagent source
   audit in the style of TASK-005.
4. **TASK-005 follow-ups:** the early_write breadcrumb instrument
   (designed, not queued); the ring3 arm-sanitize patch; the F1
   sub-mechanism (console-lock candidate).
5. **The strategic fronts (unchanged):** the 126-family wedge proper (the
   modal death — r2 died at its position); the CPU1 release; the 185-path
   (needs a survivable draw).

## THE OPEN TECHNICAL THREADS (detail)

- **The split-brain on flipped draws:** stubs patched with the TRUE delta
  (bc[6]=e8000000) vs the W-32c-forced variable (e0000000) — the PB-CMA
  line shows both (va=d6800000 stub-math; pv_off=e0000000 variable). Any
  code reading the VARIABLE on a flipped draw gets 128 MB-off math. The
  FDT trims the bank (a0000000-a8000000 ignored) — the kernel copes at the
  memblock level; the split shows up downstream.
- **The W-94 snapshot ran live (r2):** cnt=1, m0=a8000000+0x17e00000
  (post-shave), sentinels intact, cnt2=1 ⇒ memblock healthy through
  map_lowmem even on the flipped draw.
- **ring3.count** accumulates (23,832 → 24,272); do not use as per-run.
- **The decode-offset fix:** verify any ring decode's FIRST chars against
  a known line (the banner) before quoting the tail.
- **The 1160-printless era** (TASK-005 F1): on deep draws the console is
  silent past PB-CMA while execution continues — mechanism inferred
  (console routing/lock), breadcrumb instrument designed.
- The bc[24..31] cluster = mixed-writer (payload sweep / head-common
  markers / W-94 snapshot); r2's finals decoded (the snapshot values).

## THE STANDING RULES (the short list — full: docs/README.md)

- ASK THE USER before EVERY device run; hands-off; video + LED timings;
  the user pauses between runs to write timings down — ASK on their cue.
- python for hex (rule 13) — including timeline notes; READ the log
  (rule 14); never pattern-write a poll line.
- fresh-slot discipline: CHANGED values (or the poll's watched
  transitions + bc[15]/nonce); ring decode = count-many chars only, and
  mind the raw+0x78 offset; watch the tail for the CR→LF signature.
- The kernel make = a clean shell; rule-16 the SHIPPED artifact (disasm +
  sizes + hashes); back up the old pack before mkkernel; snapshot regen
  via COMMANDS.md. head.o = armv6k (no movw/movt there!).
- NVRAM and RPMB untouchable. Nothing QNX-side after the GICD-off.
- git commit + push every state change; the bootstrap = the wrap product;
  wrap at 40-60% context.
- The poller = the run wrapper; the door keeper BEFORE anything
  device-touching; the user's LED timings = the run's data — ASK.
