# BOOTSTRAP SESSION 16 (written 2026-10-05, session 14's wrap-up product)

## THE ONE-PARAGRAPH STATE

Session 14 = the workstation migration + the device link rebuilt + NINE
runs (W-96 ×3, W-97, W-98 — the live-poller era). **THE HEADLINE: the
boot REACHED THE INITCALL ERA — ~10 walls past the CMA in one boot
(bc[1]=185 = the W-94 record front, reproduced) with the initcall
breadcrumbs DECODED: bc[6]=2 = postcore level, bc[7]=0xc0e0719c =
`atomic_pool_init` (System.map) = the fn RUNNING at death, bc[2]=0xBEEF
= the per-initcall WDT2 kick nonce (main.c:1629-1642 — the ladder
mechanism documented).** The fronts from a COLD machine (3 weeks
unpowered): 142-triad → then warm: 126/153/156/185 drawn (7 warm runs,
kernel #159-class bytes unchanged throughout — the sweep machinery =
the floor-lifter, proven ALL 8192 chunks in ≤7 s per flight ~5-6× the
W-91 measure). The +0x40 pointer riddle = DISSOLVED (a cross-kernel
comparison artifact; every historical r[0] = kern + APP(own-pack)
EXACTLY); W-96 run 2's +0x300 = the sole outlier, parked one-run-deep.
The ATAG-compat lead (bootstrap-14's step-4) = CLOSED (never fires).
The WDT2 down-counter = UNREADABLE pre-arm (the channel reads WLDR
= the LOAD register until the payload's own kick); the off→red 68 s
question = parked (bootrom-phase). **W-99 = designed, not built: the
ring batch-64 flush = too coarse in the initcall era (the death-era
prints sat unflushed) — tighten the cadence (one kernel-side variable),
rule-16, poller run. The witness: atomic_pool_init's death analysis
(CMA/gen_pool vs the TLB-op class) waits on what the console says.**

## THE MANDATORY READ ORDER (before any work)

1. `newdocs/HANDOFF.md`, `DEVELOPMENT.md`, `SETUP.md` (the standing rules)
2. `newdocs/PROJECT_STATE.md` (rewritten for session 14's finale)
3. `newdocs/session-notes/session-14.md` (the migration + BerryShell-V4
   + the poller + W-96/97/98 = THE session-14 knowledge base)
4. `docs/03_DEBUGGING_SESSIONS.md` (the W-96/97/98 records — note W-98's
   record sits BEFORE W-97's: the append-order slip, fix noted)
5. `docs/README.md` (rules 1-17) + `newdocs/KNOWN_ISSUES.md`
6. `PLAYBOOK-REFERENCE.md` §5/§8/§10 (the safety model)

## THE NEW WORKSTATION FACTS (session 14, extended)

- The workspace = the qemu Debian 13 VM; the device link = USB RNDIS
  passthrough + `~/BerryShell-V4.py hold` (the door keeper; rides
  reboots unattended — proven through NINE WDT2 cycles this session).
  The door = single-session (hold blocks auth/exec).
- **The instrument = `kexec/poll-jump.sh`** (session-14 built, commit
  05d5d8f): jump.sh wrapped with a 3 s host-side poller (full bc page
  0x80, the WDT2 counter 4a31402c, the ring window 0x140, deployed
  sizes at pass 1). The live log = ~/agent-runs/w98-live-*.log class.
  Run it EXACTLY like jump.sh: `PAYLOAD_MODE=--l2on ./poll-jump.sh zImage`.
- The git push = autonomous (the PAT seeded ~/.git-credentials).
- The skills = playbook-dev-orchestration + blackberry-playbook-dev-access
  (Hermes-side; updated with the session's lessons).

## THE LADDER MAP (the initcall-era crumbs = NEW decode table)

W-98-era readback shape (the boot past the CMA → the slab):
- bc[1]=185 = the last PB_MMU_BC marker before the initcall loop
- bc[2]=0xBEEF = the per-initcall kick nonce (main.c:1637)
- bc[6]=level, bc[7]=the running initcall fn (resolve via System.map —
  the death hero = `atomic_pool_init` at postcore, level 2)
- bc[13]=0xbfbfff50 (the slab-era pte pointer, deterministic), 
- bc[14]=0xC0DE0030 (the second-parse done-latch — W-94's marker)
- The W-95-era slots (bc[16..31]) = sweep heartbeats + tallies.

## THE FIRST TASK (W-99): THE FLUSH CADENCE

The ring = dead-quiet in the initcall era (1165 chars = the same count
as every 126/153/156-class run — the log carried nothing new PAST
PB-CMA) because omap4bc.S's batch flush = every-64-chars: the death-era
prints sat in the unflushed tail. ONE kernel-side variable: tighten
the batch (16) OR add a per-initcall flush; rebuild (the tree, then
mkkernel.sh — NOTE the packed artifact then changes: rule-16 re-verify,
the W-95 backup stays for the W-97-era artifact identity), then poller
run. EXPECT: the console speaking through the death = the richest
evidence class (atomic_pool_init's own prints — the CMA/gen_pool path,
the allocation's GFP_ATOMIC, the pool gen creation — vs the TLB-op
class = the interpretive fork the console text resolves).

## THE ORDER AFTER (if the console speaks)

1. atomic_pool_init's death analysis from the new evidence (the
   allocation path vs the TLB-op class at the slab depth).
2. The determinism repeat at the new front (is 185-repeatable or a
   luck-draw like 126 vs 156?).
3. The CPU1 release (the bequest) = STILL the strategic cure front.
4. rootfs, display, RE queue = the post-boot era.

## THE STANDING RULES (the short list — full: docs/README.md)

- ASK THE USER before every device run; hands-off; unfiltered output;
  video (the user's recordings = the session's data — ask; missed runs
  get no LED record, note it and move on).
- python for hex (rule 13); grep, don't recall (rule 14); the native
  edit tools (rule 15); verify the shipped binary (rule 16); the
  mirror/slot discrimination (rule 17) + the pair-less-marker clause
  + the initcall-crumbs decode table above.
- NVRAM and RPMB untouchable. Nothing QNX-side after the GICD-off.
- The kernel make = a clean shell; re-scp memdump3 after any reboot.
- git commit + push every state change (the push = autonomous now);
  the bootstrap = the wrap-up product; wrap ONLY at 40-60% context.
- The poller = the run wrapper now (never bare jump.sh for observation
  runs); the door keeper BEFORE anything device-touching.
