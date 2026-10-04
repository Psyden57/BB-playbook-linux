# BOOTSTRAP SESSION 15 (written 2026-10-04, session 14's wrap-up product)

## THE ONE-PARAGRAPH STATE

Session 14 = the workstation migration (WSL2 → CachyOS + qemu Debian 13
VM driven by the Hermes agent) + the device link rebuilt + the W-96
determinism series and NOTHING else changed on the device (kernel #159
artifact byte-unchanged, sha256 42bf3025… verified pre-run; the payload
rebuilt from unchanged sources). The results: **run 1 on the COLD
machine (3 weeks unpowered; DRAM decayed to AA-rot; NO valid L2
fossils) = the W-84 triad EXACTLY (142 + 0x3E7 + ring 0); run 2 (warm)
= 126 with THE FULL RING ALIVE (1165 chars: banner → PB-ADJ/MEM/RES →
CMA 16MiB@0xbe800000 → PB-CMA correct → END — the log ends exactly
where marker 126 claims); run 3 (warm) = 156 = the W-93 front
reproduced (bc[2]/mirror = 127|1|2 because 156's writer is
pair-less — rule 17 archaeology now recorded).** The sweep machinery =
a consistent FLOOR-lifter on warm machines (every warm run ≥126) but
the depth still draws 126..156 — the lottery, tamed not cured. The
promoted lead = the PB-RES r[0] pointer: dtb_phys + 0x40 with the
CORRECT totalsize (0x15519 HEX = 87,321 — read the W-93 erratum before
interpreting any "15,519") = the decompressor's ATAG-compat path
(head-S ~384-460) — UNREAD, still the first task.

## THE MANDATORY READ ORDER (before any work)

1. `newdocs/HANDOFF.md`, `DEVELOPMENT.md`, `SETUP.md` (the standing rules)
2. `newdocs/PROJECT_STATE.md` (rewritten for session 14)
3. `newdocs/session-notes/session-14.md` (the migration + BerryShell-V4
   + the W-96 series = THE session-14 knowledge base)
4. `docs/03_DEBUGGING_SESSIONS.md` (the W-96 record, append-only)
5. `docs/README.md` (rules 1-17) + `newdocs/KNOWN_ISSUES.md`
6. `PLAYBOOK-REFERENCE.md` §5/§8/§10 (the safety model)

## THE NEW WORKSTATION FACTS (session 14)

- The workspace = the qemu Debian 13 VM (this machine); the kernel tree
  at /home/psyden/kernel/linux (NOT Modified-tree git — the repo's
  kernel state = kernel-patches/); the QNX SDP at ~/qnx660-master; the
  payload toolchain needs `libc6-i386` + `zlib1g:i386` (installed).
- **The device link**: USB RNDIS passed through to the VM (device =
  169.254.0.1, host = 169.254.0.2). SSH (port 22) is CLOSED until the
  qconn door (4455) authenticates → run `python3 ~/BerryShell-V4.py
  hold` FIRST (the door keeper; it rides reboots on its own — proven
  through three WDT2 cycles this session). **The door = single-session:
  while hold runs, use plain ssh/scp only** (auth/exec stall).
- **The UTS-stamp riddle (don't re-derive it)**: the ring banner reads
  "#157 … Sep 13" — that is the in-tree UTS build counter of the
  Sep-12-packed artifact (sha256-verified = the W-95-shipped bytes).
  The PROJECT's "kernel #159" ledger counts PB-marker changes. Two
  different counters — never reconcile, never treat the banner as a
  staleness signal. The artifact-truth check = sha256 against
  ~/.hermes/cache/scratch/w95-artifacts/ (recreate if pruned:
  pack = kexec/kernel/zImage + the DTB, rule-16 style).
- Decode tooling: `~/agent-runs/decode_readback.py` (bc + ring) —
  rule 13 compliant. The raw run logs + ring dumps = ~/agent-runs/.

## THE FIRST TASK (W-97 prep): THE ATAG-COMPAT READ (no device needed)

The ring's PB-RES r[0] = a34ee9a8 while dtb_phys = a34ee968 (+0x40),
the value = the CORRECT totalsize. The bootstrap-14 step 4 = STILL
UNREAD: head-S ~line 384-460 — the decompressor's ATAG-compat path
(r8 = the stub's TTBR0 = nonzero = "an ATAG list around"). Read +
explain the +0x40 delta BEFORE the next instrumented run. NOTE: with
r[0] sitting at dtb_phys+0x40, r[0] READS the header word at +0x40 —
the totalsize the kernel prints comes from there (a header-shaped
 Interpretation, not a copy). Verify against
arch/arm/kernel/setup.c's PB-RES print (grep it — rule 14).

## THE ORDER AFTER (if the +0x40 falls)

1. The ring-flush cadence past the CMA (the batch-64 flush = too coarse
   in the 126/156 region; the slab-era prints = lost). An observability
   fix, payload+kernel side, ONE variable.
2. The determinism repeat: 3× more W-96-class runs (the front should
   sit in the 126..156 band on warm machines; a 185-class draw = the
   W-94 warmth question reopened).
3. The sweep A/Bs (the 0x770-vs-0x7F0 two-op scheme, the whole-DRAM
   early sweep) — ONE VARIABLE at a time.
4. The SMP bring-up (the CPU1 release, the SAR neutralization) = the
   real TLB-op cure. THE PARK IS STILL FORBIDDEN without the SAR
   neutralization.
5. The rootfs era, then the display (the DISPC RE), then the RE queue.

## THE STANDING RULES (the short list — full: docs/README.md)

- ASK THE USER before every device run; hands-off; unfiltered jump.sh
  output; record video; the LED timings on request (the user keeps
  them on video — ASK when wanted).
- python for hex (rule 13); grep, don't recall (rule 14); the native
  edit tools (rule 15); verify the shipped binary (rule 16); the
  mirror/slot discrimination (rule 17 — and the NEW pair-less-marker
  clause: bc[2]/mirror can stand from an earlier sibling when the
  latest writer is pair-less — check the writer shape before reading
  the pair, mmu.c's list is the truth).
- NVRAM and RPMB untouchable. The device clock = don't trust.
- Nothing QNX-side after the GICD-off; the kernel make = a clean shell;
  re-scp memdump3 after any reboot; /tmp = wiped on reboot.
- git commit every state change; the bootstrap = the wrap-up product;
  wrap ONLY at 40-60% context (keep working until then). PUSH note:
  the repo push needs a one-time PAT on this VM (~/.git-credentials
  absent; helper=store is set) — commits pile up locally until then.
- jump.sh may outlive its 300 s wrapper on slow runs — the readback =
  manual if the wrapper dies (memdump3 pages: bc 90000000/40, ext
  90000040/30, ring 88000080/4e0 — the manual set, exact commands in
  newdocs/COMMANDS.md).
