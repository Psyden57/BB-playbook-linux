# Architecture

## The transition, end to end

```
 QNX 6.6 (Tablet OS 2.0) userland                 bare metal
┌──────────────────────────────────────┐          ┌────────────────────────────┐
│ qnx2linux (root, System mode)        │          │ IRAM stub (0x40304000)     │
│ 1. allocate 24MB contiguous phys     │          │  flat page table (1MB secs)│
│ 2. copy kernel+DTB into it           │          │  continuation code         │
│ 3. build flat table (tt[] in IRAM)   │  blx     │  probe.bin (optional)      │
│ 4. copy trampoline (position-indep)  │ ───────► │                            │
│ 5. arm WDT2, hold CPU1 in warm reset │          │ trampoline (runs from      │
│ 6. clean+inval L1, GICD CTLR = 0     │          │  payload text VA)          │
│ 7. enter_stub: TTBR0=flat, switch    │          │   └─► continuation (IRAM,  │
│ 8. optional monitor SMC (L2 on/off)  │          │       VA==PA): MMU off,    │
│ 9. last cached act: nothing          │          │       r0=0 r1=~0 r2=DTB    │
└──────────────────────────────────────┘          │           └─► stext        │
        WDT2 (~59s) fires ─► warm reset ─► QNX    └────────────────────────────┘
        breadcrumbs + console ring survive                 Linux 6.15.11
        (read back over SSH after reboot)
```

## Components

### kexec/qnx2linux.c — the payload (QNX userland, ~1450 lines)

The main program. Modes (PAYLOAD_MODE env or argv[1], selected in jump.sh):

| Mode | Purpose |
|------|---------|
| `--probe` (default) | T2-era test: jump to probe.bin in IRAM |
| `--t3` | jump directly into the kernel (no probe) |
| `--l2on` | keep the PL310 L2 enabled through the jump (current default for boots) |
| `--l2lat` | PL310 latency probe (proved NS latency writes SIGBUS) |
| `--l2test` | retired: L2-off A/B cliff test (phase C wedges on purpose) |
| `--ppa` | secure PPA service probe (0x25/0x26/0x27/0x23 with PL310 readbacks) |

Key subsystems inside:
- **Placement search** (2026-09-03 v4): 129-slot 2 MB-aligned sweep of the
  upper DRAM bank with per-reason failure counters, then a generic loop;
  kernel placed at `kern_off` (first 2 MB-aligned offset inside the buffer,
  because QNX's free pool has no aligned 24 MB run).
- **fdt_patch_memory()**: minimal FDT walker that rewrites the /memory
  node's `reg` to match the actual placement (bank = [kern_phys, 0xc0000000)).
- **params block** (IRAM 0x40309800): `[0]` kernel entry, `[1]` DTB phys
  (read by both the probe and the stub continuation).
- **jump buffer verification**: memcpy + memcmp through a NOCACHE mapping
  before the jump; REVERIFY pass right before entering the no-I/O zone.
- **WDT2 lifecycle**: proper enable (SPR 0xBBBB/0x4444) + kick (TGR
  complement) before the jump; disable (SPR 0xAAAA/0x5555) on abort paths.

### kexec/stub3.S — trampoline + continuation

- `tramp_pos_start` (position-independent, runs from the payload's text):
  TTBCR=0, DACR=all, TTBR0=flat table, breadcrumbs 63/64, `bx` to the
  continuation in IRAM.
- `cont_start` (copied to IRAM 0x40308040, VA==PA): breadcrumb 21, records
  the DTB chain value to 0x90000030, SVC mode + scratch stack (0x89000000),
  MMU off, `r0=0, r1=~0, r2=params[1]`, `bx r9` → target.
- NOTE: no TLBIALL here (wedged with CPU1 in reset; redundant).

### kexec/probe.S — bare-metal diagnostic (optional)

Runs at IRAM 0x40309000 before chaining into the kernel: validates the DTB
(magic + totalsize), toggles the FAN5702 LED (GPIO1_13), checks PL310
accessibility, records readbacks into the bc page (bc[4]=r2, bc[6]=DTB
magic, bc[8]=PL310 CTRL, bc[9]=data latency), runs a 1 GB store loop (L2
sanity), then chains into the kernel with the ARM boot convention.

### IRAM layout (0x40304000+)

| Address | Contents |
|---------|----------|
| 0x40304000 | flat page table (`tt[]`, 16 KB) |
| 0x40308040 | continuation (from stub3.S) |
| 0x40309000 | probe.bin (optional) |
| 0x40309800 | params block (kernel entry, DTB phys, kern_phys, DTB size) |

### The debug/observability channels

1. **Breadcrumb ladder** (bc page, DRAM 0x90000000): word 1 = current step;
   strongly-ordered stores (MMU-off PA writes reach DRAM in both L2 states);
   survives the WDT2 warm reset; read back with `memdump3` over SSH.
   Mirrors at 0x88000000/0x94000000/0x9FE00000. bc[15] = per-run nonce.
2. **Console ring** (ring1, DRAM 0x88000100, count at 0x88000080): the
   kernel's DEBUG_LL (`omap4bc.S`) mirrors every earlycon character into
   the ring AND to UART3; flushed via monitor SMC 0x101 batches; survives
   the reset. Decode `memdump3` output word-reversed (it prints big-endian).
3. **PB-PANIC notifier**: kernel panics print into the rings.
4. **LED** (FAN5702, GPIO1_13 EN): blue during payload/kernel start, off
   during kernel run, red at reboot — the user video-records runs for
   timing analysis.
5. **ring3** (0x94000100): the secure monitor's own UART3 capture —
   INACTIVE while the kernel dies post-idle (a posted store to the dead THR
   stalls the store buffer). Historical.

### The kernel side

- Tree: `/home/psyden/kernel/linux` (mainline 6.15.11, non-LPAE omap2plus).
- Patches: `arch/arm/kernel/head.S` (PlayBook prelude: breadcrumb ladder,
  DEBUG_LL smoke test, r1/r2 save/restore, ring-map fixes incl. the M=1
  fix), `arch/arm/include/debug/omap4bc.S` (DEBUG_LL → UART3 + DRAM rings),
  `arch/arm/mach-omap2/omap4-common.c` (L2 write_sec routing), DTS
  `omap4-winchester.dts` (memory bank, pl310 node, reserved-memory nodes),
  `arch/arm/mm/dma-mapping.c` (PB-CMA print).
- Build: `mkkernel.sh` packs Image/zImage + DTB into `kexec/kernel/`.
- Config: session-6 config; recoverable from any built Image via
  `scripts/extract-ikconfig` (CONFIG_IKCONFIG=y). Forced cmdline:
  `console=ttyO2,115200n8 earlyprintk keep_bootcon ignore_loglevel maxcpus=1 root=/dev/mmcblk0p2 rootwait`.

### The secure monitor (TrustZone) surface

SMC #0 (or #1 — the immediate is ignored) with the service in r12; C-flow
`mon_call()` shape works, inline-in-stub SMCs hang. Verified services:

| Service | Function |
|---------|----------|
| 0x100 | L2X0 DBG_CTRL write |
| 0x101 | L2 clean+invalidate by PA (the payload's flush mechanism) |
| 0x102 | L2X0 CTRL write (0/1) |
| 0x108 | SCU_PWR / omap4 suspend (devpm's entry) |
| 0x109 | L2X0 AUXCTRL write |
| 0x113 | L2X0 PREFETCH write |
| 0x100-0x113 | table complete — **no tag/data-latency service exists** |
| PPA 0x26/0x27 | devpm suspend pair (accepted, semantics unknown) |
| PPA 0x25/0x23 | rejected on this monitor (0xFF02) |

NS access rules (hard-won): PL310 CTRL/AUX/latency writes = SIGBUS/abort;
by-way ops (0x7FC) = deadlock; by-PA (0x768) + sync (0x730) = safe; PRCM
writes = secure-filtered (silent SIGSEGV, no reboot).

### Tools (kexec/)

`memdump3` (device-side RAM dump, prints words big-endian — reverse each
4-byte group), `memtest`, `scatter` (RAM survival map across the reset),
`smctest` (interactive SMC caller), `secure-probe`, `sevtap`, `xntest`,
`i2c-hammer`, `uart-hunt`, `waitdev`, `memw32`, `retry-jump.sh`.

## Interaction with the rest of the machine

- **CPU1**: held in warm reset via `RSTCTRL_CPU1` (PRCM 0x4824380C/0x4824340C)
  before the jump; the kernel brings it up itself (SMP).
- **WDT2** (0x4A314000): armed by the payload so a hung kernel = warm reset
  = back to QNX = readbacks over SSH. The 58.6 s window is the debugging
  cadence.
- **eMMC / WiFi / display**: untouched; QNX's drivers are simply abandoned
  mid-flight. The kernel needs its own drivers (mainline has them all for
  OMAP4).
