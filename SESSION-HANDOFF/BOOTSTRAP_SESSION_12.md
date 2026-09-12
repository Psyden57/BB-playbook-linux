# BOOTSTRAP SESSION 12 (written 2026-09-11/12, session 11's wrap-up product)

## THE ONE-PARAGRAPH STATE

Session 11 ran W-69..W-83 (15 runs, 15 builds) and rewrote the wedge
model. The console is ALIVE (the earlycon ring — restored by the L2-ON
run; alive originally since the session-4 era). The CMA block passes
(the TLBIALL skipped). The kernel = 6.15.11 with **CONFIG_SMP=n**, run
with **--l2on**. THE UNIFIED WEDGE FAMILY: the machine wedges on
SCU-routed global ops with CPU1 held — the TLB maintenance ops
(deterministic with the L2 off, flaky with the L2 on) and the
ldrex/strex exclusives (the spinlocks — deterministic with the L2 on +
SMP=y, harmless with !SMP). The current front = **the first TLB op after
the CMA (clear_fixmap in early_fixmap_shutdown), death bc[1]=126→125**.
The session-10 "dsb nosh" chain NEVER existed on the hardware (the rule-16
audit: GNU as rejects the name; GCC's IAS silently emitted the same
full-system mcr; the f57ff062 literal = an invalid ISB-class encoding).
The session-10 "stale pgd pair" = an attribute-bit misread (the entries
are valid section descs in every shape). The ACTLR, the cache state, the
pgd content, and the CP13 writes = all exonerated as the TLB-op wedge's
cause. THE CPU1 PARK IS FORBIDDEN (it broke the post-WDT2-reset recovery:
the released CPU1 resurrected QNX via the SAR path = the 11-min dark
device). The kernel to run first = **the current kexec/kernel/zImage
(5,221,585 B packed, the SMP=n + the TLBIALL-skipped + the sweeps-via-SMC
build)**.

## THE MANDATORY READ ORDER (before any work)

1. `newdocs/HANDOFF.md`, `DEVELOPMENT.md`, `SETUP.md` (the standing rules)
2. `newdocs/PROJECT_STATE.md` (the front — rewritten for session 11)
3. `newdocs/session-notes/session-11.md` (the full run map + the ruled-out
   list + the toolchain facts — THE session-11 knowledge base)
4. `docs/03_DEBUGGING_SESSIONS.md` (the W-69..W-83 records, append-only)
5. `docs/README.md` (rules 1-17) + `newdocs/KNOWN_ISSUES.md`
6. `newdocs/audit-approach-2026-09-11.md` + `bootdumps-2026-09-11/`
   (the session-10 audit + the bootrom RE — still the RE reference)
7. `PLAYBOOK-REFERENCE.md` §5/§8/§10 (the safety model)

## DEVICE FACTS (verified this session)

- QNX 6.6 at 169.254.0.1, the SSH options in COMMANDS.md (hmac-sha1
  dropped). /tmp wiped per reboot (jump.sh redeploys; re-scp memdump3
  after any non-jump recovery).
- **The LED protocol is ambiguous in the kernel-running state**: the blue
  = the payload/kernel start, off = "the kernel running" OR dead. THE
  DARK-NO-LED DEVICE = check the SSH FIRST (the user verified SSH was
  down during the 11-min dark episode — the W-73/W-77 breaker). The
  power-button hard reset = preserves the DRAM-ish (the readback = lower
  confidence, partially trampled); the battery pull = the DRAM wiped.
- The readbacks: `memdump3 90000000 0x40` (bc[0..15]) + `memdump3
  90000040 0x40` (bc[16..31] — the extended forensics incl. the pgd-dump
  slots). The ring: 88000080/0x4e0 + the python word-reverse decode.
- The bc[15] nonce mismatch between the jump.sh t=5s dump and the
  post-reset readback = NORMAL (the t=5s dump = pre-nonce-write; the
  post = this run's nonce).

## THE FIRST TASK (W-84): the payload archaeology bisect

The TLB-op wedge = the machine's response to the SCU-routed global ops
with CPU1 held. The ONLY era where TLB ops ever completed = the runs
23-32 (2026-09-02) — the OLD payload (--t3, the minimal flow). Every
payload since = the redesigned flow (the DISPC kill, the devb slay, the
CIPA sweeps, the memtest, the placement ladder). THE BISECT:
1. `git log kexec/qnx2linux.c` — find the 2026-09-02-era commit (the
   runs 23-32 era, the pre-W-39 shape).
2. Check out that qnx2linux.c, build the era's payload, pair it with the
   CURRENT kernel (the SMP=n + the TLBIALL-skipped + the L2-ON — note the
   era's --t3 = the L2-off via 0x102; adapt: the era's payload may need
   the mon_call(0x102) kept, i.e. the era's exact flow first).
3. Run. The TLB ops fine = THE PAYLOAD is the poison → bisect the payload
   deltas (the DISPC kill? the devb slay? the CIPA sweeps? the memtest?).
   The TLB ops wedged = the payload exonerated → the real cure = the CPU1
   release done right (the SAR neutralization 0x4A326B00 + the
   kernel-side re-hold in head.S + the park blob) or the SMP bring-up.
4. ONE VARIABLE AT A TIME; the bc[27] phases are already in place.

## THE BEQUEST ORDER (after the TLB-op front falls)

1. The LED-progress encoding (the user's idea, cheap): the kernel already
   drives GPIO1_13 (the SETDATAOUT at marker 111, main.c) — add the
   per-phase colors/blips + the per-initcall heartbeat so a video
   reconstructs the death point for the unrecoverable runs.
2. The 171 wall (the taskstats/kmem_cache era) — may return; the era
   matrix says it appeared in BOTH the cached and the uncached eras.
3. The SMP bring-up (the bequest): the CPU1 release done RIGHT (the SAR
   neutralization + the re-hold + the AUX_CORE_BOOT flow) = the real TLB
   cure AND the second core.
4. The rootfs era: the eMMC (the QNX-partitioned disk), the initramfs,
   the console = a real driver (the earlycon → ttyO2).
5. The display (the DISPC RE) — the user's RE targets.
6. The RE queue: the bootrom's CH* parsers, the ip=0xF0 service, the
   registry blob (id 0x18), the hidden 4 KB dispatch region.

## THE STANDING RULES (the short list — full: docs/README.md)

- ASK THE USER before every device run; hands-off; unfiltered jump.sh
  output (never `| tail`); record video; the LED timings on request.
- python for hex decode; grep, don't recall; the native edit tools.
- Shipped-binary verification (rule 16) before every run — **this session
  proved it twice**: the "nosh" chain and the f57ff062 literal would have
  poisoned weeks of work.
- NVRAM and RPMB (hd6) untouchable. The device clock = don't trust.
- git commit + push every state change; the bootstrap = the wrap-up
  product.
