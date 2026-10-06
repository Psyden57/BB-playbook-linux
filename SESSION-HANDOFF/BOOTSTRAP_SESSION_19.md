# BOOTSTRAP SESSION 19 (written 2026-10-05, session 18's wrap-up product)

## ★ NOTE FROM THE USER — THE PREVIOUS SESSIONS' AGENTS ARE REACHABLE

**You can ask the previous sessions' agents (16/17/18) any questions you
may have.** If anything in this bootstrap, the repo, or the device state
is missing context, unclear, or you cannot figure something out — write the
question down; the user will relay it to the session-18 agent (still alive)
and bring the answer back. Use this early rather than guessing (it has
worked repeatedly: the W-101 REPLACE directive and the W-102 scoping both
came back through the relay).

## THE ONE-PARAGRAPH STATE

Session 18 = W-101 r3 + r4, THE DECODE CORRECTION, and W-102 (built + r1).
HEADLINES: (1) **THE CR→LF MICRO-SIGNATURE = DISSOLVED** — a ring-decoder
OFF-BY-ONE: ring_put pre-increments the idx, so the ring text starts at
+0x101 (not +0x100); the session-17 decoder's raw[0x78:] ate the first char
and DROPPED THE TRUE LAST — every "death in the CR→LF gap" was the missed
char. All records end \r\n-complete (r1/r2/r3 + W-99 r1). Decoder fixed
(raw[0x79:]) in ~/agent-runs/w101-artifacts/decode_readbacks_generic.py +
the skill's scripts/decode_readback.py. (2) RESTATED DEATH WINDOWS (source
order): W-101 r2 = the svm-store class INSIDE dma_contiguous_remap's
iotable_init call (after 167, at p[0]=0; bc[13]=svm_pa=0xbfbfffd4); W-101
r1 = after the COMPLETE PB-ADJ#1 record; W-101 r3 = the [126→125] window =
early_fixmap_shutdown (bc[1]=126 + bc[2]=0x17e + mirror=0x27e; bc[13]=
0xdfbf7000) = the 126-family wedge PROPER. (3) W-101 r4 = THE TRIAD
(142+0x3E7+ring0; bc[16]=0). (4) **W-102 = THE FIXUP-REGION FINE MARKERS**
(ledger #163; banner #162; pack 5,223,233 sha ee303813): bc[17] = 1
(walk-1 done) / 2 (W-101 inv-loop entry). Designed → TASK-007 preflight
audit (GO) → build → rule-16 (disasm+pool+census+DTB verified) → flown.
**W-102 r1 = THE 142-WINDOW SPLIT: bc[17]=0 ⇒ THE FIXUP-REGION DEATHS
(W-101 r4, W-102 r1) ARE WALK-1 DEATHS — the W-101 second walk is NOT
implicated (r4 resolves by equivalence).** The triad struck 2 consecutive
draws; 0x3E7 = 4th lifetime instance (writer STILL unidentified; the
"999 = 1000−1" countdown lead logged in the r1 record, unverified).
(5) The bucket-flip decision = ACCEPT + DOCUMENT (DECISIONS D13 — no
guard). (6) ring3.count = 0x5A01 + this-run chars (5 datapoints; the base
exists pre-console; its writer unpinned). (7) off→red series: 68×8, 67, 68,
69 (11 instances — the band holds). (8) The WRAPPER-LATENCY measurement
(user-flagged): a run = device reboot ~3.5 min + battery readback ~1.5 min;
no wrapper stall; an optional slim (the 0xF80 ring read → the manual
battery) = an open user decision.

## THE MANDATORY READ ORDER (before any work)

1. `newdocs/HANDOFF.md`, `DEVELOPMENT.md`, `SETUP.md` (the standing rules)
2. `newdocs/PROJECT_STATE.md` (the session-18 block = the current state)
3. `newdocs/session-notes/session-18.md` (the decode correction, r3/r4/r1,
   W-102, the wrap)
4. `docs/03_DEBUGGING_SESSIONS.md` (the session-17 note + W-101 r1-r4 +
   W-102 r1 + the SESSION-18 DECODE CORRECTION)
5. `docs/README.md` (rules 1-17) + `newdocs/KNOWN_ISSUES.md` (the
   session-18 header) + `newdocs/DECISIONS.md` (D13)
6. `PLAYBOOK-REFERENCE.md` §5/§8/§10 (the safety model)
7. ~/agent-runs/: `w101-run{3,4}-record.md`, `w102-run1-record.md`,
   `W-102-design.md`, `TASK-007-w102-preflight.md` (+report) + the raw
   batteries (local-only).

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
- **The post-jump window (measured):** after the jump the VM sees the link
  drop; then a ~4-5 MINUTE window where the interface + route exist but ARP
  gets no answer; then "Connection refused" for seconds while the keeper
  re-handshakes, then SSH. jump.sh's reboot-wait loop (80 × ~9 s, probes
  every ~3-9 s once reachable) rides ALL of it — do NOT panic, do NOT touch
  the device, do NOT restart the wrapper. **A run's total wall-clock =
  the device's reboot cycle (~3.5 min) + the battery readback (~1.5 min,
  dominated by the 0xF80 ring transfer) ≈ 5-6 min — this is NORMAL; the
  probes catch SSH within seconds of it opening** (session-18 measured; do
  not chase a phantom stall).
- Observation runs = `kexec/poll-jump.sh` (never bare jump.sh):
  `PAYLOAD_MODE=--l2on ./poll-jump.sh zImage`. The live log =
  ~/agent-runs/live-TIMESTAMP.log. (poll-jump.sh's expected deployed-size
  string is updated to the W-102 pack.)
- **Readback battery (the standard):** after every run — `memdump3
  90000000 0x40` + `90000040 0x40` + `88000080 0xf80` + `90000080 0x8`
  (ring2 count) + `94000080 0x8` (ring3 count) + `94000000 0x8` (mirror0).
  Decode with `~/agent-runs/w101-artifacts/decode_readbacks_generic.py
  <raw> <prefix>` — **the text offset is raw[0x79:] NOW** (the 8-byte slip
  class is long fixed; the 1-byte slip fixed in session 18 — verify the
  first fresh char = "[" and the last = "\n" against the count). Capture
  with a plain redirect; NEVER `| tee file | head` (SIGPIPE truncation).

## THE ARTIFACTS / LEDGERS

- **W-102 pack = CURRENT: 5,223,233 B sha256 ee303813 (ledger #163; UTS
  banner "#162"; kexec/kernel/zImage).** Backups: ~/agent-runs/
  w102-artifacts/zImage.pre-w102-packed = the #162 pack (47c175cb) + the
  pre-build Image/zImage snapshots + the fixup-region disasm.
- Kernel tree = the #163 build state; kernel-patches/ = current (full-regen
  format; git apply --check clean); poll-jump.sh's expected sizes updated.
- Records: ~/agent-runs/w101-run{1..4}-* and w102-run1-* (records, led
  files, raw batteries, session logs); TASK-005/006/007; W-102-design.md;
  the generic decoder (fixed).
- Session-18 commits: 06018a9 (r3 + decode correction), 778aa8c (r4 + D13),
  3694e6c (W-102 build), ae3b8d5 (W-102 r1) + the wrap commit.

## THE W-102 MARKER SEMANTICS (bc[17] = 0x90000044) — the readback key

| bc[17] | bc[16] | meaning |
|--------|--------|---------|
| 0      | 0      | died in WALK-1 (the patch walk / its setup) — W-102 r1's result |
| 1      | 0      | died entering the W-101 block (the CTRL-check gap) |
| 1      | 0xFFFFFFFF | the L2-off skip path (block entered, skipped) |
| 2      | 0      | died inside the inv loop / its syncs |
| 2      | 272    | the pass COMPLETED (healthy) |

(bc[17] pre-marker = payload DISPC readback 0 + arm-era 0. Payload writes 0;
only the W-102 markers write 1/2.)

## THE DECISION QUEUE FOR SESSION 19 (ranked)

1. **W-102 draws r2+** — pure draws on #163. Watch EVERY readback's
   bc[17]/bc[16] combo (table above). Questions: does the walk-1 class
   persist (2 consecutive: r4 → r1)? Do OTHER classes finally show 1/2?
   Does the triad recur (3 consecutive)?
2. **The writer audits (offline, build-free):** 0x3E7 (4 instances; now
   consecutive with fixup deaths; the "999 = 1000−1" countdown lead);
   bc[27] (0x29 r1 / 0x31 r4); bc[19]=0x84 (all fixup-era readbacks);
   bc[10]/bc[12]=1. A TASK-005-style subagent source audit is the natural
   vehicle.
3. **The wrapper-slim decision (user):** move the 0xF80 ring read from
   jump.sh/the wrapper to the manual battery (~30-60 s faster wrapper exit;
   the manual battery captures the same ring).
4. **TASK-005 follow-ups unchanged:** the early_write breadcrumb instrument
   (designed, not queued); the F1 sub-mechanism (console-lock candidate —
   the printless era is real and unaffected by the decode fix).
5. **The strategic fronts (unchanged):** the 126-family wedge proper (the
   modal deep class — now with clean visibility); the CPU1 release (the
   bequest); the 185-path.

## THE OPEN TECHNICAL THREADS (detail)

- **The walk-1 deaths (r4, r1):** bc[1]=142 + bc[2]=0x3E7 + ring1/2=0 +
  mirror0=0x46 + 143/144 present + bc[16]=0 + bc[17]=0. Died inside the
  272-site patch walk or its setup. No finer split without new markers (an
  option: a mid-walk marker — weigh vs the region's sensitivity).
- **0x3E7:** 4 instances (W-35, W-84, W-101 r4, W-102 r1); zImage-
  correlated per KNOWN_ISSUES; writer STILL unidentified.
- **The fixup-region pool layout** (for future disasm): the marker/const
  pools sit at ~c00082ec-830c behind the `b __enable_mmu` terminator;
  c0008300 = 0x90000044 (W-102), c0008304 = 0x48242000 (PL310), c0008308 =
  0x90000040 (bc[16]). Section symbols move per build — re-resolve.
- **ring3:** page is dual-writer (busyuart + the C mirror loop); count =
  0x5A01 + chars; text = \n-only stream + deep-run residue.
- **The decode discipline:** source-order death-window derivation +
  both-ends verification (skill references/readback-analysis.md §4/§5/§7).
- **The W-101/W-102 build flow precedent** (reuse for the next kernel
  change): design note → TASK preflight audit (subagent; includes the
  standalone armv6k encode-probe) → clean-shell build → rule-16 (disasm +
  pool + byte-census + DTB identity) → mkkernel.sh → poll-jump expected-size
  update → snapshot regen → commit.

## THE STANDING RULES (the short list — full: docs/README.md)

- ASK THE USER before EVERY device run; hands-off; video + LED timings;
  the user pauses between runs to write timings down — ASK on their cue.
- python for hex (rule 13) — including timeline notes; READ the log
  (rule 14); never pattern-write a poll line.
- fresh-slot discipline: CHANGED values (or the poll's watched transitions +
  bc[15]/nonce); ring decode = count-many chars only, text at raw[0x79:];
  the ring tail does not localize deaths — the ladder + source order does.
- The kernel make = a clean shell; rule-16 the SHIPPED artifact (disasm +
  sizes + hashes); back up the old pack before mkkernel; head.o = armv6k
  (no movw/movt there!).
- NVRAM and RPMB untouchable. Nothing QNX-side after the GICD-off.
- git commit + push every state change; the bootstrap = the wrap product;
  wrap at 40-60% context.
- The poller = the run wrapper; the door keeper BEFORE anything
  device-touching; the user's LED timings = the run's data — ASK.
