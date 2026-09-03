# Next Steps & Continuation Guide

## UPDATE 2026-09-03 (session 7) — the "root cause" section below is UNDER REVISION
The monitor RE closed with NO latency/PPA L2 service anywhere (run PPA-1:
0x25/0x23 rejected 0xFF02; 0x26/0x27 accepted, no PL310 change), so the
"fix = inside the secure monitor" plan below is DEAD. The wall now reads as
a **head.S pv-table fixup bug** (the 0x1f7f0000 VA = a broken pv delta,
matching the session-6 "broken pv PHYS_OFFSET 0x41810000" observation — not
random machine corruption). See docs/03 session 2026-09-03 and the task list
below.

## UPDATE 2026-09-02 (night, session 6) — READ THIS FIRST

**The console WORKS.** The boot log lands in the CACHEABLE DRAM rings
(ring1 = 0x88000080 count / +0x100 chars, 3840-char window) and survives the
WDT2 reset via batched SMC 0x101 flushes. PB-PANIC prints panic text into the
rings. Decode memdump3 output by reversing each 4-byte group (big-endian word
format).

**The L2-off mode is DEAD** — proven by the payload's `--l2test` A/B: device
stores with the L2 ON = clean; the identical stores with the L2 OFF = machine
wedge. The old "171 wall" (taskstats/kmem_cache) = the bypass traffic cliff
plus the head.S C/B/S strip that left the kernel UNCACHED. Both fixed. The
bc[2]=0x3E7 (999) mystery = QNX-boot leftovers in the bc page, not a kernel
wild write. The old "0xFED was never mapped" diagnosis was also WRONG (the
pristine DEBUG_LL block always mapped it).

**The current wall**: --l2on boots reach bc=127 (paging_init: map_kernel done)
with 1100-1200 chars of clean console, then die deterministically at
dma_contiguous_remap — `dma_mmu_remap[0].base` corrupted (0xbe800000 →
0xa1000000) → `BUG: not creating mapping for 0xa1000000 at 0x1f7f0000 in user
region` → the CMA area left unmapped → a later abort. Reproducible in BOTH
L2 modes.

**The root cause (pinned by --l2lat)**: PL310 data latency = **0x111 (1/1/1
cycles)** live, set by QNX — the NS write = SIGBUS (secure-filtered) and the
RE'd monitor API has no latency service. L3/EMIF auto-idle (PRCM) is also
secure-only. The machine corrupts/loses DRAM transactions under this config —
every wild write, |0x80 ring byte, corrupted pv-patch site and wedged loop
this session = it, placement-dependent.

**The fix = inside the secure monitor.** RE `trustzone-omap4` (the QNX-PACKED
module — already dumped: device-binaries/trustzone-omap4 + trustzone-omap4.dis (12.5k lines)):
1. A L2X0 tag/data-latency service (the old "0x104 = auxcoreboot" mapping is
   SUSPECT — verify against the SMC dispatch table)
2. The PPA clock-domain service (disable L3/EMIF auto-idle)
If a latency service exists: payload sets 3-cycle latencies pre-jump
(C-flow mon_call) and the --l2on mode should boot clean past paging_init.

## Task List (in order)

### 1. RE the trustzone-omap4 monitor module — CLOSED (2026-09-03)
The monitor is NOT any QNX binary: trustzone-omap4 = the /dev/trustzone
crypto resmgr (0 SMCs), libsecure_dispatcher = crypto/KDS (0 SMCs), procnto
= 0 SMCs / 0 PL310 access. The SMC services live in the TI ROM monitor and
the 0x100-0x113 table (docs/03) is COMPLETE: **no tag/data-latency service
exists**. The one new discovery: devpm-omap4 calls PPA services 0x26/0x27
(devpm shape: r0=idx r1=0 r2=4 r3=pargs r6=0xFF r12=0) — not in mainline's
PPA list (0x21/0x23/0x25). Full details: docs/03 session 2026-09-03.

### 2. Run the payload --ppa probe — DONE (run PPA-1, 2026-09-03)
Outcome: 0x25/0x23 rejected (0xFF02 — not in the BlackBerry monitor),
0x26/0x27 accepted (0x0) with NO PL310 readback change (ctrl=1, aux=1e070000,
tag=0, data=0x111, prefetch=0). **The secure-side fix path is EXHAUSTED** —
no latency SMC, no PPA L2 service, nothing in the monitor touches the PL310
config. Full record: docs/03 session 2026-09-03, run PPA-1.

### 3. The bc=127 wall — ROOT-CAUSED (2026-09-03): load placement vs DT bank mismatch
NOT machine corruption, NOT the pv fixup itself (phys2virt.S is byte-identical
to mainline). The kernel loads at 0xa4080000 (PHYS_OFFSET_eff 0xa4000000)
while the DTS memory bank = 0x80000000+1GB — 576 MB of memblock sits BELOW
PHYS_OFFSET and can never be linear-mapped; bottom-up allocs (dram_sync
steal, CMA) land there (0xa1000000) and dma_contiguous_remap's
__phys_to_virt(0xa1000000) = 0xBD000000 < TASK_SIZE (0xBF000000) → "in user
region" BUG, deterministic. Bonus finding: the observed VA 0x1f7f0000 =
0xa1000000 MINUS 0x81810000 = the unpatched pv-stub placeholder
(__PV_BITS_31_24+__PV_BITS_23_16) — an unpatched pv site exists (defect 2,
mechanism open; the session-6 "barrier struct corruption" 0x1f7f0000 = the
same signature). Full proof chain: docs/03 session 2026-09-03.

### 3. The bc=127 wall — SOLVED (W-4); the DTB delivery on the Image path is the open issue
The placement/DTB-bank fix WORKED: W-4 booted past 127 to bc=171 (the old
session-6 wall). But both W-4 (probe) and W-5 (--t3) lost the DTB en route
("Neither atags nor dtb found" ×2, 16MB fallback) — the uncompressed-Image
r2 chain is broken somewhere between the (verified-correct) params[1] and
the kernel's vet. PARKED: session 6's proven path = the ZIMAGE, whose
decompressor natively delivers the appended DTB (r2) and auto-derives
zreladdr (CONFIG_ARM_APPENDED_DTB + CONFIG_AUTO_ZRELADDR=y). Full analysis:
docs/03 runs W-4/W-5 + the W-series conclusion.

### 4. Next run (first thing next session)
`PAYLOAD_MODE=--l2on ./jump.sh zImage` — expect "Machine model: BlackBerry
PlayBook", full memory, cma reserved; then the 171 wall gets retested with
a real DTB. LED: no probe LED dance on --t3; with --l2on+probe the usual
blue→off→red sequence. Record LED timings from the video.

### 5. The l2x0_of_init hazards (was "guard cache-l2x0.c") — amended
The NS-latency-SIGBUS guard is a NO-OP: the DT latency writes already route
through omap4_l2c310_write_sec (default = WARN+skip). The REAL hazards when
the boot first reaches l2x0_of_init (init_IRQ): l2c_enable's by-way write to
L2X0_INV_WAY (0x7FC = the on-device deadlock op) and the l2c_wait_mask poll.
Patch l2c_enable (skip by-way on winchester) or run CONFIG_CACHE_L2X0=n and
keep QNX's L2 setup. Moot until the wall is broken.

### 5. Then the boot should sail
With the latencies fixed (or auto-idle off): the --l2on boot continues past
paging_init into initcalls with a working console and the PB-MEM/PB-ADJ
diagnostics. Expect walls at the eMMC/ADMA, the WiFi SDIO, and the
PRCM-dependent drivers — each with a readable panic.

### 4. No-rootfs panic = SUCCESS for the kexec mechanism
The DTB has no root filesystem — the boot ends in "No working init found" or
an initramfs-less panic. The panic text lands in the rings. Then the rootfs
(initramfs with a busybox shell) is the next milestone.

## Historical (completed phases)
- M=1 wall — SOLVED (head.S ring-map shift bug, 2026-09-01)
- DRAM memtest — CLEAN (2026-09-01)
- Monitor service RE — FIRST PASS (2026-09-02; the table in 03; latency/PPA
  services still missing — the RE continues)
- L2 disable via SMC 0x102 — WORKING but the L2-off mode is RETIRED (the
  bypass wedges under sustained traffic — session 6)
- The 171 wall — SOLVED/RETIRED (the bypass cliff + the C/B/S strip —
  session 6)
- The console — WORKING (cacheable rings + SMC 0x101 flushes — session 6)
- The memblock limit-0 panic — FIXED (forced limit + PB-MEM prints)

## Documentation Maintenance

| File | When to Update |
|------|----------------|
| `00_PROJECT_OVERVIEW.md` | Major milestone changes |
| `01_KEXEC_ARCHITECTURE.md` | Payload logic changes |
| `03_DEBUGGING_SESSIONS.md` | After each debugging session |
| `04_KEY_FILES_AND_COMMANDS.md` | New tools, changed commands |
| `05_NEXT_STEPS.md` (this file) | After each decision point |
| `SESSION-HANDOFF/` | After each session (the record + the handoff) |

## Critical Rules (current)
- **Payload SIGSEGV ≠ reboot** — user-mode aborts are cheap (QNX survives);
  only jump-context deaths reboot.
- **PRCM (CM1/CM2) writes from NS = SIGSEGV** — secure-filtered; the monitor's
  PPA services are the only path.
- **NS writes to the PL310 latency regs (0x108 tag / 0x10C data) = SIGBUS** —
  proven by --l2lat (0x104 = AUX CTRL, not a latency reg). CTRL/AUX/PREFETCH
  go through the SMC services only; latency regs have NO service at all.
- **Check the bc[15] nonce on every readback** — stale content is the #1
  misread risk (the rings/bc persist across reboots).
- **The ring1 log = the primary evidence** — decode memdump3 output by
  reversing each 4-byte group. The monitor's ring3 UART capture is INACTIVE
  (the kernel never writes UART3 — it dies post-idle and a posted store to
  the dead THR stalls the store buffer).
- **Sustained DEVICE (SO) stores wedge the machine with the L2 on** — the
  ring/bc sections are CACHEABLE; data is flushed in batches (SMC 0x101).
  Never reintroduce per-char device-store consoles.
- **Zero unbounded polls boot-wide** (audited) — new code uses pb_smc_flush.
- **The UART pad hunt is DEAD** — don't re-attempt.
- jump.sh launches the payload detached; a hung payload ≠ SIGHUP death.
- Sync after every dd/cp; never plain-mmap device memory.
- Ask the user for LED timings on every run (they video-record the runs).

## Contact / Context
Solo low-level ARM boot project. OpenViking memory has the full session
records (`viking://resources/playbook-dev/...`) and the project state summary
(`viking://user/default/memories/entities/project/playbook_linux_port.md`).
