You are continuing a QNX→Linux kexec project on a BlackBerry PlayBook (TI
OMAP4430 "winchester", HS device, QNX 6.6, root via SSH root@169.254.0.1,
key at playbook-dev/rsa). This is session 8.

The project is now a **Git repo** (private: github.com/Psyden57/BB-playbook-linux,
HTTPS + cached PAT). The repo is the source of truth — read IN THIS ORDER:

1. `newdocs/HANDOFF.md` (the read-order protocol itself)
2. `newdocs/PROJECT_STATE.md` (current technical state — wins over older docs)
3. `newdocs/ARCHITECTURE.md`, `newdocs/COMMANDS.md` (the complete command
   reference — device SSH options, debug loop, memdump3 decode, capstone/
   objdump RE, patch-snapshot recipe, git flow)
4. `newdocs/session-notes/session-07.md` (latest session notes; sessions 1-6
   are backfilled there too)
5. `docs/03_DEBUGGING_SESSIONS.md` — sections "Run W-1" through "Run W-8"
   (the current run map)
6. `docs/README.md` — the **canonical Critical Rules list (12 numbered
   rules)** + the key-address cheatsheet + the post-run decision tree
7. `newdocs/contradictions/` (5 files — preserved unresolved disputes)
8. On NVRAM/RPMB/brick hazards and device lessons: `PLAYBOOK-REFERENCE.md`
   §5/§8/§10 — SAFETY-CRITICAL

STATE SUMMARY (end of session 7):

- **The repo**: sources (`kexec/`), docs layers, `kernel-patches/` (the full
  mainline-6.15.11 diff + config — REGENERATE after kernel changes, recipe
  in newdocs/COMMANDS.md), git identity set, HTTPS push working.
- **The 127 wall is SOLVED** (placement/DTB-bank mismatch, session 7). W-4
  reached bc=171 on the uncompressed-Image path.
- **Current front: the zImage path dies inside `__fixup_pv_table`.**
  W-6/W-7/W-8 (all zImage, --l2on): bc=107→130 post-fixup_smp, never 131;
  ring = smoke test only; **bc[2]=0x3E7 in all three zImage runs** (unknown
  writer, zImage-correlated). W-7 post-mortem: fixup code bytes byte-intact
  in the decompressed image (PA = VA−0x20000000), the bl intact,
  `__pv_offset`/`__pv_phys_pfn_offset` read back 0. W-8 (CIPA-flushed
  markers — the eviction-luck variable eliminated): 130 landed, 131 absent
  → the death inside the fixup is cleanly confirmed.
- **Major observability finding (session 7)**: SO bc stores only *dirty* the
  PL310; dirty lines reach DRAM by eviction and are DISCARDED by QNX's L2
  re-init after the warm reset. pbmark/pbmark3 now CIPA-flush both bc lines
  by PA (0x768/0x730, NS-safe). The phys2virt instrumentation stores
  (bc[6]=r8, markers 140/141) still lack the flush — inconclusive by design.
- **The Image path is parked** (its DTB loss mystery: "Neither atags nor dtb
  found" ×2 with a verified-correct params[1] — probe loop exonerated). The
  zImage path was the pivot; it now has its own wall.
- The device clock was NTP-synced once over WiFi (2026-09-04) — don't rely
  on it; the nonce design (time ^ staging phys) stays.
- ARM objdumps EXIST: `~/toolchains/armv7-eabihf/bin/arm-linux-objdump`
  (kernel, full symbols) and `~/qnx660-master/host/linux/x86/usr/bin/
  arm-unknown-nto-qnx6.6.0eabi-objdump` (QNX ELFs). Capstone for raw
  memdump3 dumps.
- Session-7 lesson enforced: **verify claims against the codebase — grep,
  don't recall.** Two sessions nearly chased phantom bugs on recalled
  pseudo-source.

RULES (safety-critical subset — the FULL canonical list is docs/README.md
"Critical Rules (Never Forget)", 12 numbered rules, plus the run rules in
newdocs/DEVELOPMENT.md and the session protocol in newdocs/HANDOFF.md):

- Ask the user before every device run. The user can hard-reboot, charges
  the device, and video-records runs — ask for LED timings every time.
- Verify the bc[15] nonce on every readback. Distrust bc[6]+ unless this
  run's code demonstrably wrote it. memdump3 words are big-endian — reverse
  each 4-byte group when decoding.
- Never: NS PRCM writes, NS PL310 CTRL/AUX/latency writes, by-way ops
  (0x7FC), unbounded polls, UART3 writes from the kernel, monitor 0x105,
  `dd if=/dev/mem`, plain-mmap of device memory, **NVRAM or RPMB access
  (brick hazards — PLAYBOOK-REFERENCE.md §5/§8)**.
- Do not re-test dead ends (the L2-off mode, the UART pad hunt, the monitor
  latency-service hunt — all closed; the full lists are in docs/03 +
  newdocs/KNOWN_ISSUES).
- File edits: the Edit tool, never python heredocs/sed (a past session lost
  code to one; verified data-loss incidents exist).
- Kernel changes → regenerate the `kernel-patches/` snapshot (recipe in
  newdocs/COMMANDS.md) and commit.
- Update docs/03 (append-only) + newdocs/PROJECT_STATE.md after every run;
  git commit + push.

FIRST TASK (from docs/03, run W-8): **diagnose the `__fixup_pv_table` death
on the zImage path.** The facts: fixup_smp completes (130, flushed), fixup_pv's
bl and bytes are intact, its .data stores never landed, its own
instrumentation stores are unflushed (inconclusive), and bc[2]=0x3E7 recurs
on every zImage run. Candidate experiments (docs/03, W-8 section):
(1) CIPA-flush the phys2virt instrumentation stores, (2) order-swap
fixup_pv/fixup_smp, (3) inline-test the fixup's first stores in head.S,
(4) re-derive bc[4]/dtb_phys exactly with the objdump — the params block
itself may be implicated. Pick up from there; a successful fixup_pv run
then delivers the DTB+full-memory boot the 171-era matrix wants.
