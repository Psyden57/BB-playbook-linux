> **STATUS: HISTORICAL snapshot (session 6). The live overview lives in newdocs/PROJECT_OVERVIEW.md; the live state in newdocs/PROJECT_STATE.md.**
> This file is kept for its rationale/narrative value; do not update it going forward.

# BlackBerry PlayBook Mainline Linux Boot Project

## Goal
Boot mainline Linux on the BlackBerry PlayBook tablet (TI OMAP4430, ARMv7-A, codename "winchester") via a kexec-style jump from the running QNX 6.6 userspace.

## Hardware
- **SoC**: TI OMAP4430 (dual-core Cortex-A9, ARMv7-A)
- **RAM**: 1 GB DDR2/DDR3 (varies by unit; two 64GB eMMC units available: Samsung MCGAFA and SanDisk SEM32G)
- **Boot ROM**: QNX 6.6 (BlackBerry Tablet OS 2.1.0.x)
- **Peripherals**: PL310 L2 cache controller, GICv1, UART3 (debug), I2C3/4, WDT2, GPIO, DISPC, SGX GPU

## Current Approach
**QNX-side payload** (`qnx2linux.c`) runs as root on CPU0, builds a flat L1 page table in IRAM, flushes caches, holds CPU1 in reset, disables GICD, switches TTBR0, disables MMU, then branches to:
- **T2**: Test blob (hello.bin) in IRAM — breadcrumbs + UART loop → WDT2 reboot
- **T3**: Real Linux zImage with appended DTB — kernel runs for real; WDT2 provides ~15s watchdog window; breadcrumbs + DEBUG_LL ring survive reboot

## Key Memory Map
| Region | Physical | Purpose |
|--------|----------|---------|
| DRAM base | 0x80000000 | Main RAM (1GB = 0x80000000–0xBFFFFFFF) |
| IRAM | 0x40300000 | On-chip RAM (flat table @ 0x40304000, stub @ 0x40308000) |
| PL310 L2 | 0x48242000 | Cache controller |
| WDT2 | 0x4A314000 | Watchdog timer |
| GICD | 0x48241000 | Interrupt controller |
| UART3 | 0x48020000 | QNX console; DEAD post-idle (L4PER auto-idle) — the kernel never writes it (see 03) |
| BC primary | 0x90000000 | Breadcrumb page + ring2 (4KB) |
| BC mirrors | 0x94000000, 0x88000000, 0x9FE00000 | Survive WDT2 reboot |
| DEBUG_LL rings | 0x88000080, 0x90000080, 0x94000080 | Kernel console capture (CACHEABLE ring sections, SMC-flushed) |

## Build Environment
- **Host**: Linux x86_64
- **Cross-toolchain**: QNX SDP 6.6 (`arm-unknown-nto-qnx6.6.0eabi-*`)
- **Env setup**: `source ../qnx-env.sh` (sets up compiler paths)
- **Build**: `./build.sh` in `kexec/`

## Deployment
- **SSH**: RNDIS over USB, device at 169.254.0.1, root login with key `../rsa`
- **Deploy script**: `./jump.sh zImage` (the kernel zImage + the appended DTB)
- **Payload mode**: `PAYLOAD_MODE=--l2on ./jump.sh zImage` keeps the L2 enabled
  (the current mode); the default `--probe` disables it (the retired mode)
- **Run pinned to CPU0**: `on -C 0 /tmp/qnx2linux ...`
- **Timeout**: 120s (was 30s — SIGHUP was killing payload mid-setup)

## Critical Ops Rules
1. **Always sync after dd/cp** — QNX drops uncommitted cache on kernel death
2. **Never plain-mmap device/unmapped memory** — use `mmap_device_memory` with `PROT_NOCACHE | MAP_PHYS`; speculative reads to unmapped bus cause external aborts → WDT2 reset (~10s)
3. **Never `dd if=/dev/mem` with skip** — skip doesn't guarantee seek; use compiled tool with `mmap_device_memory` after addresses proven live via `pidin mem`
4. **QNX userland gaps**: no `od`, no `head`, `dd` numeric-only args, `pidin` arg format
5. **Boot timing**: 2–3 minutes to runlevel 2/SSH; wait ~3 min before assuming hang
6. **Payload SIGSEGV ≠ reboot** (2026-09-02, run 31) — a user-mode abort in the payload kills the process, QNX survives, SSH stays up. Only jump-context deaths reboot. BUT: a jump-context payload death (post-GICD-off) still takes procnto down → WDT2 → reboot
7. **PRCM (CM1/CM2) registers are secure-filtered** — a CLKSTCTRL write from NS = SIGSEGV (run 31). The monitor's PPA services are the only path to clock-domain control.

## SOLVED 2026-09-02 (night): the "171 wall" and the corruption — root cause = secure-domain L2 config
**The L2-off mode is DEAD.** The payload `--l2test` A/B proved it: device stores
with the L2 ON = clean; the identical stores with the L2 OFF (the 0x102 disable)
= machine wedge. The old "171 wall" (taskstats/kmem_cache) was the bypass traffic
cliff plus the head.S C/B/S strip that left the whole kernel UNCACHED — both fixed
this session. The mystery bc[2]=0x3E7 (999) = QNX-boot leftovers in the bc page,
not a kernel wild write.

**The current model (all --l2on jumps)**: the kernel boots cacheable, console via
the three DRAM rings (UART3 is dead post-idle and never written by the kernel),
markers flushed via monitor SMC 0x101. It reaches bc=127 (paging_init:
map_kernel done) and dies deterministically at dma_contiguous_remap — a corrupted
`dma_mmu_remap[0].base` (0xbe800000 → 0xa1000000). The corruption is the
session-long "silent corruption" class, and `--l2lat` pinned the prime suspect:
**PL310 data latency = 0x111 (1/1/1 cycles), set by QNX, secure-only** — the NS
write = SIGBUS, and the RE'd monitor API has no latency service. L3/EMIF
auto-idle (PRCM) is also secure-only. **The fix = inside the secure monitor**
(RE trustzone-omap4: a latency service and/or the PPA clock-domain service).

**Watchdog truth (2026-09-02)**: WDT2 registers = TGR 0x30 (trigger: ANY write
reloads CRR from LDR), SPR 0x48 (start/stop service), CRR 0x28, LDR 0x2C
(0xFFE2B400 = **58.6 s** @ 32.768 kHz, PTV=0), CLR 0x24 (prescaler). The "60 s"
was approximate; the payload's old `~WTGR` complement-write works (any write
triggers). QNX's `wdtkick -t 15000` daemon (15 s period) is the fallback kicker —
after the jump nobody kicks but the kernel.

**Console truth (2026-09-02 night)**: the kernel's console = earlycon0
(earlyprintk.c) → printascii → **omap4bc.S** → CACHEABLE DRAM rings only —
**UART3 writes are REMOVED from omap4bc.S** (UART3 dies post-idle: a posted
store to the dead THR stalls the store buffer; a dead-target read stalls
forever). The monitor's UART3 capture (ring3, 0x94000100) is therefore
INACTIVE — the DRAM rings (ring1 = 0x88000080 count / +0x100 chars, 3840-char
window) are the readback. memdump3 prints words BIG-ENDIAN-formatted: reverse
each 4-byte group when decoding. NOTE: the old "0xFED was never mapped"
diagnosis was WRONG — the pristine DEBUG_LL block in head.S always mapped it
via addruart; the real console killers were the flush machinery and the UART3
store. PB-PANIC notifier prints the panic text into the rings. UART pad hunt
is still DEAD.

## Earlier milestone: the M=1 wall (bc=122 ceiling) — SOLVED 2026-09-01
Root cause: the PlayBook ring-map block in `arch/arm/kernel/head.S`
(`__create_page_tables`) shifted PA>>12-style values with `lsl #SECTION_SHIFT` (=20,
non-LPAE) instead of `lsl #12` — the shift truncated every descriptor to zero, mapping
the DEBUG_LL ring VAs (0xC80/0xD00/0xD40), the PL310 section (0xFEB) AND the
abort-trap vectors (0x000/0xFFF) to PA 0x00000000 (boot ROM) / PA 0x20000000.
Fixed; verified in the rebuilt vmlinux disassembly.