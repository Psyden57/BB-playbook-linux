# BOOTSTRAP SESSION 17 (written 2026-10-05, session 16's wrap-up product)

## ★ NOTE FROM THE USER — THE SESSION-16 AGENT IS STILL AVAILABLE

**Remember: you can ask the session-16 agent any questions you may have.**
If anything in this bootstrap, the repo, or the device state is missing
context, unclear, or you cannot figure something out — write the question
down; the user will relay it to the session-16 agent (still alive) and
bring the answer back. Use this early rather than guessing.

## THE ONE-PARAGRAPH STATE

Session 16 = W-99 (the flush-cadence build, kernel #160) + **the LIVE
UNPATCHED-PV CATCH** + the TASK-004 audit + W-100 (the inv-only pv-site
pass, kernel #161) × 4 runs. THE HEADLINES: (1) the PB-CMA print's
designed discriminator FIRED LIVE: va = base − 0x81810000 = 0x3cff0000
while __pv_offset read correct → the BUG line → death MID-PRINTK — the
#4-family class observed directly for the first time; (2) TASK-004
(audited, root-reviewed) = mechanism (a): **a stale (L2) line serves the
C-world instruction fetch of a .text pv stub** — the fixup's patch loop
has ZERO cache maintenance, and the W-32c block covers only the two pv
VARIABLES (cross-draw proof: W-96 r2 va=0xde800000 vs W-99 va=0x3cff0000
on the same function); the cure must be INV-ONLY (0x770) — a CLEAN would
poison DRAM (W-90a lesson); (3) W-100's inv-only pass RUNS (272/272,
bc[16]=0x110, 4/4 runs) and is benign — but the stale-site draw has NOT
recurred (0 hits in 4), so the discriminator is still open; (4) THE
STRUCTURE LEARNED (same #161 artifact, 4 draws): the 126-remap-window
death = per-draw dice (r1/r4 died there; **r3 PASSED it and reached the
INITCALL ERA** — bc[2]=0xBEEF, bc[6]=1 core, bc[7]=`rcu_init_tasks_generic`
= the NEW death hero (core_initcall, kernel/rcu/tasks.h:2278), bc[1]=184);
the CMA-reserve-FAILED draw (r2) sails to post-setup_arch (bc[1]=111,
main.c:1261) — the 126-wall IS the CMA-remap's own TLB-op family; (5)
off→red = 68 s ×6 runs (the bootloader-phase delta — user observed the
red IN THE BOOTLOADER; iron-clad).

## THE MANDATORY READ ORDER (before any work)

1. `newdocs/HANDOFF.md`, `DEVELOPMENT.md`, `SETUP.md` (the standing rules)
2. `newdocs/PROJECT_STATE.md` (the session-16 block = the current state)
3. `newdocs/session-notes/session-16.md` (the W-99/W-100 knowledge base)
4. `docs/03_DEBUGGING_SESSIONS.md` (W-99 r1, W-100 r1..r4 — the run records)
5. `docs/README.md` (rules 1-17) + `newdocs/KNOWN_ISSUES.md` (the
   session-16 headers + the W-100 candidate notes)
6. `PLAYBOOK-REFERENCE.md` §5/§8/§10 (the safety model)

## THE DEVICE LINK (unique to this VM — not in the repo docs)

- The PlayBook = USB RNDIS passthrough at 169.254.0.1. SSH is CLOSED
  until the qconn door authenticates. First: test
  `ssh -i ~/playbook-dev/rsa root@169.254.0.1 "echo up"` with the option
  set from newdocs/COMMANDS.md — if it works, the BerryShell-V4 hold
  daemon is alive (check `pgrep -af BerryShell-V4` and
  ~/agent-runs/berryshell-door.log; do NOT start a second one — the door
  is single-session). If refused: `python3 ~/BerryShell-V4.py hold
  --log ~/agent-runs/berryshell-door.log` (background,
  persist_on_release=true) and wait for "Authenticated" in the log.
- The door keeper was RESTARTED this session after a host PC restart —
  the drill works (interface up via the hotplug rule, hold re-auth'd in
  seconds).
- Observation runs = `kexec/poll-jump.sh` (never bare jump.sh):
  `PAYLOAD_MODE=--l2on ./poll-jump.sh zImage`. The live log =
  ~/agent-runs/live-TIMESTAMP.log.
- **Readback discipline (session-16 lessons): re-dump FRESH after every
  run** (`memdump3 90000000 0x40` + `88000080 0xf80` + `90000040 0x40` +
  `94000000 0x8`); decode with decode_readback.py; **ring text = decode
  count-many chars ONLY** (beyond count = prior-run residue); use the
  CHANGED-VALUE test for slot freshness (a slot whose value differs from
  the previous readback = fresh).

## THE ARTIFACTS / LEDGERS

- kernel #160 pack = 5,224,865 B sha256 b77d1197 (W-99; ~/agent-runs/
  w99-artifacts + the #159 pack in w95-artifacts).
- **kernel #161 pack = 5,227,633 B sha256 f69f4539 (W-100; CURRENT —
  kexec/kernel/zImage; backups in ~/agent-runs/w100-artifacts).**
- Run records: ~/agent-runs/w99-run1-*, w100-run{1..4}-* (bc/ring/
  slots/mirrors/led files + session logs), live-2026*.log.
- TASK-003 (W-99 preflight), TASK-004 (pv-site audit) — report + brief
  with root review, in ~/agent-runs/.
- The kernel tree = the #161 build; kernel-patches/ = current snapshot
  (spliced format, apply-checked; the regen recipe in COMMANDS.md now
  strips timestamps).
- The UTS counters: the banner "#160" = the in-tree counter; the project
  ledger "#161" — different counters (unchanged rule).

## THE DECISION QUEUE FOR SESSION 17 (ranked)

1. **W-101 — the fixup-side per-site inv (THE ROBUST CURE) vs more W-100
   repeats.** The W-100 pass runs + is benign; the stale-site draw hasn't
   recurred (0/4). Options: (a) build W-101 = patch+inv per table entry
   INSIDE the head.S inline fixup (MMU-off, PL310 at PA 0x48242000,
   0x770 inv-only — the omap4bc-proven pattern at that exact stage; then
   NO C-world fetch can ever see a pre-patch line; ONE kernel-side
   variable; rule-16 + snapshot as usual) and fly it; (b) more W-100
   repeats until a stale-site hit (the class seems rarer than W-99
   suggested); (c) accept W-100 as insurance and park; (d) both (a)+(b).
   NOTE: the pv classes are ORTHOGONAL to the region-staleness classes
   (r2's map_lowmem garbage WARN fired with a CLEAN pv — see below).
2. **The 185-path (initcall-era draws).** Two heroes now: atomic_pool_init
   (postcore/2, W-98) + rcu_init_tasks_generic (core/1, W-100 r3). More
   initcall-era draws = the hero distribution + the era's evidence. The
   console stays PRINTLESS past PB-CMA there (count 1160 every time) —
   the printk-source audit of the early initcalls = the next
   observability lead if the era recurs.
3. **The lottery map (record it as it grows):** 126-remap-death (modal),
   wedge-pass → initcall era (r3), CMA-reserve-fail → deep-111 (r2),
   stale-pv overlay (W-99), map_lowmem garbage-region WARN (W-90a/92/97/
   W-100 r2 — 4 datapoints; survivable; the class is orthogonal to pv).
   New flavors this session: cma_area_count garbage; the read-era 43 s
   stall (cold-cache reads, r3).
4. **The CPU1 release = still the strategic cure front** (unchanged).
5. The atomic_pool_init analysis = its framework is in TASK-003 §3 +
   the report; waits for a 185 draw's evidence (the ring is printless —
   the bc-side or the initcall printk audit = the levers).

## THE OPEN TECHNICAL THREADS (detail)

- **The pv-site mechanism (a) status:** HIGH/MEDIUM; 136 stub sites /
  272 instructions enumerated (all in __pv_table; S1 = dma-mapping.c:287
  = the va= field site; S2/S3 = :311; I1 = mmu.c:1053). The fixup's
  patch loop (head.S:373-393) has zero cache ops; the W-32c block
  (main.c) covers only 0xc0f09540-9f (the two variables). W-100's pass =
  main.c start_kernel, after the W-32c loop: walks the table, NS 0x770
  per entry, syncs every 64 + final, L2-guarded, bc[16] = count.
- **The persistence model (rings):** pb_flush_rings (SMC 0x101,
  [0x80,0x580)) runs per COMPLETED console write (early_write →
  pb_flush_rings, early_printk.c:70) — the primary path; the omap4bc
  batch flush (16-char cadence, [0x80,0x5C0)) = the never-completed-
  printk insurance; empirically the DRAM text survives BEYOND both
  windows (+31 chars in W-99) — mechanism unresolved (eviction/QNX-side
  writeback suspected). The console is NOT the bottleneck; the boot dies
  before printing more.
- **jump.sh's ring readback = 0xf80 (full page)** — the W-99 instrument
  fix; it captured the complete tail for the first time.
- **The wedge family: PROBABILISTIC** (r3 passed it; r1/r4 did not) —
  any future "wall" claims must quote N draws.
- **The r2 CMA-fail branch:** cma_area_count read full (mm/cma.c:227/
  :618) → no CMA → no remap → the boot sailed to 111. If it recurs, the
  counter's staleness = the next micro-thread.
- **The r3 LED anomaly:** reads stalled ~43 s (cold cache?); payload
  phase = 86 s vs the usual 30-45 s. Recorded.

## THE STANDING RULES (the short list — full: docs/README.md)

- ASK THE USER before EVERY device run; hands-off; video + LED timings;
  the user pauses between runs to write timings down — ASK on their cue.
- python for hex (rule 13) — INCLUDING timeline notes: NEVER pattern-write
  a poll line from a previous run's shape; READ the log (the r3 lesson).
- fresh-slot discipline: a readback slot counts as this-run's only if its
  value CHANGED since the last readback (or correlate via bc[16]/nonce).
- Ring decode = count-many chars only; the rest is residue.
- The kernel make = a clean shell; rule-16 the SHIPPED artifact every
  time (disasm + sizes + hashes); back up the old pack before mkkernel;
  snapshot regen via COMMANDS.md (timestamp-strip included).
- NVRAM and RPMB untouchable. Nothing QNX-side after the GICD-off.
- git commit + push every state change (autonomous); the bootstrap = the
  wrap product; wrap at 40-60% context.
- The poller = the run wrapper; the door keeper BEFORE anything
  device-touching; the user's LED timings = the run's data — ASK.
