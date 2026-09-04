# Hardware Reference — BlackBerry PlayBook (Winchester)

## 2026-09-05 UPDATES (session 9 — supersede the details below where they conflict)
- **MMC2/eMMC MMCHS registers (0x480B4000+) are NOT NS-accessible**: the
  first SYSCONFIG read from the payload = SIGBUS fltno=5 (external abort,
  W-28) — and that abort class then FROZE the box completely (SSH dead,
  display dead; power-hold required). Treat device-register external
  aborts as device-wedging, not cheap userland crashes. DISPC registers
  ARE NS-accessible (kill + readback verified, bc[16]/bc[17] = 0/0).
- **The DMA-master quiesce state (run with --dmaquiet)**: eMMC via `slay
  devb-mmcsd-winchester` (QNX survives — the qnx6 write-back cache
  absorbs writes; only execs needing a devb READ fail, e.g. "cat: cannot
  execute" — NOT a fault); DISPC killed at bc 34 + register-verified;
  WiFi SDIO never brought up (DTS: mmc1/3/4/5 disabled). The kernel's
  per-run deaths SURVIVED this quiesce → the source is the stale-view
  class (newdocs/contradictions), not a rogue DMA master.
- **The zreladdr inflation region** [0xa0008000, ~0xa0f80000): the payload
  placement sweep now REJECTS any 24MB window intersecting
  [0xa0000000, 0xa1000000) — the decompressor relocates itself + its
  malloc pool into the window when they overlap, and W-35 (the only
  overlapping placement ever) died in head.S's tail.
- Extended bc slots (0x90000040+, read via `memdump3 90000040 0x30`):
  bc[16]/bc[17] = DISPC kill readbacks (0/0), bc[18] = the payload's
  placement, bc[19] = pmd/readback, bc[20]-bc[25] = the pmd pattern
  (VAs 0xdfc/0xdfe/0xdf8/0xdf4/0xdf0/0xdfa).

## 2026-09-02 NIGHT UPDATES (session 6 — supersede the details below where they conflict)
- **PL310 data latency = 0x111 (1/1/1 cycles), set by QNX, live** (read
  0x48242000+0x10C; tag latency @0x108 reads 0). The NS write to either =
  SIGBUS (external abort, secure-filtered) — proven by the payload's --l2lat.
  No monitor service for the latencies exists in the RE'd API. This config is
  the prime suspect for the machine's silent DRAM corruption.
- **UART3 is DEAD post-idle** (L4PER auto-idle): a posted store to the dead
  THR stalls the CPU's store buffer, and a read from it stalls forever. The
  kernel's omap4bc.S no longer writes UART3 at all (senduart/LSR drain
  removed). **The console = the DRAM rings** (ring1 = 0x88000080 count /
  +0x100 chars, 3840-char window, CACHEABLE sections flushed in batches via
  monitor SMC 0x101). The monitor's UART3 capture (0x94000100) is INACTIVE.
- **The L2-off mode is retired** (the bypass path wedges under sustained
  traffic — the --l2test A/B proof). The mode = --l2on (the L2 stays enabled).
- **DISPC scanout continues after "display quiesced"** — the payload's
  DISPC-kill does not blank the screen; the framebuffer fetch = DRAM
  contention through the whole boot. Note the screen state on every run.
- GPIO1 (0x4A310000): 0x194 = SETDATAOUT, 0x190 = CLEARDATAOUT (bit 13 =
  the FAN5702 indicator-LED enable — NOT the backlight).

## SoC: TI OMAP4430
- **CPU**: 2× Cortex-A9 @ 1.0 GHz, ARMv7-A, VFPv3, NEON
- **L1**: 32 KB I + 32 KB D per core, 4-way, VIPT
- **L2**: PL310, 1 MB, 16-way, PIPT
- **Interrupts**: GICv1 (GICD @ 0x48241000, GICC per-CPU)
- **Timers**: Local timer per CPU, global timer, watchdogs (WDT2 @ 0x4A314000)

## Memory Map (Physical)
```
0x00000000–0x3FFFFFFF  Peripherals / Boot ROM / Internal
0x40000000–0x4FFFFFFF  Peripherals (GPIO, I2C, UART, PRCM, PL310, GIC, WDT...)
0x40300000–0x4030FFFF  IRAM (64 KB on-chip)
0x48000000–0x48FFFFFF  L3/L4 Interconnect (PL310 @ 0x48242000)
0x4A000000–0x4AFFFFFF  More peripherals (WDT2 @ 0x4A314000)
0x80000000–0xBFFFFFFF  DRAM (1 GB = 256 MB × 4 banks?)
0xC0000000–0xFFFFFFFF  High peripherals / Alias
```

## Key Peripheral Addresses
| Peripheral | Base | Notes |
|------------|------|-------|
| PL310 L2 Cache | 0x48242000 | by-PA (0x768) + sync (0x730) NS-safe; by-way (0x7FC) = DEADLOCK; config regs secure-filtered |
| GIC Distributor | 0x48241000 | CTLR @ +0x0 |
| CPU1 Reset Ctrl | 0x4824380C | Write 1 = hold in warm reset |
| CPU0 Reset Ctrl | 0x4824340C | |
| Aux Boot | 0x48281800 | CPU1 entry vector |
| WDT2 | 0x4A314000 | See the register map below (TGR@0x30 = reload trigger) |
| UART3 (Debug) | 0x48020000 | console=ttyO2, 115200 8N1, 48 MHz clock; **the monitor captures its TX into ring3** |
| I2C3 (LED) | 0x48070000 | FAN5702 @ 0x36 |
| I2C4 | 0x48350000 | |
| GPIO1 | 0x4A310000 | |
| PRCM CM1 | 0x4A004000 | MPU inst @ +0x300 (CLKSTCTRL @ 0x4A004300) — **secure-filtered** |
| PRCM CM2 | 0x4A008000 | CORE inst @ +0x700 (L3_1/L3_2/SDMA/MEMIF/D2D/L4CFG/L3INSTR CLKSTCTRLs @ 0x4A008700–0x4A008E00), L3INIT @ +0x1300, L4PER @ +0x1400, ALWAYS_ON @ +0x600 — **secure-filtered (write = SIGSEGV)** |
| DISPC | 0x48050000 | Display controller |

## WDT2 Register Map (2026-09-02, from drivers/watchdog/omap_wdt.h + live reads)
| Offset | Register | Notes |
|--------|----------|-------|
| 0x24 | WCLR | prescaler: (1<<5)\|(PTV<<2); PTV=0 on this unit |
| 0x28 | WCRR | current counter — down-counting from 0xFFFFFFFF |
| 0x2C | WLDR | load value — **0xFFE2B400 = 58.6 s** @ 32.768 kHz, PTV=0 |
| 0x30 | WTGR | TRIGGER — **ANY write reloads CRR from WLDR** (value ignored) |
| 0x48 | WSPR | start/stop service: 0xBBBB then 0x4444 = start; 0xAAAA then 0x5555 = stop |
**Kick = any write to 0x4A314030.** The payload's `~WTGR` complement-write is a
valid trigger. WSPR is at 0x48 (NOT 0x30 — older notes conflated them).

## IRAM Layout (Used by Payload)
```
0x40304000–0x40307FFF  Flat L1 page table (16 KB = 4096 entries × 4 bytes)
0x40308000–0x40308FFF  Continuation (identity-mapped, entered at phys)
0x40309000–0x403097FF  Probe (optional, IRAM probe)
0x40309800–0x40309807  Probe params: [kernel_entry_phys, dtb_phys]
0x40305000             hello.bin load (T2/hello mode)
```

## Breadcrumb / Ring Pages (Survivor Band)
Scatter test: pages at 16 MB stride in 0x85–0xA1 survive full jump+WDT2+QNX-reboot.
| Page | Purpose |
|------|---------|
| 0x88000000 | BC mirror + DEBUG_LL ring1 (chars @ +0x100, count @ +0x80) |
| 0x90000000 | BC primary (magic, step, count, nonce, cpu0_alive, magic2) |
| 0x94000000 | BC mirror + DEBUG_LL ring2 |
| 0x9FE00000 | BC mirror (4th) + abort trap vectors + DEBUG_LL ring3 |

**Ring header**: `count` @ +0x80, `index` @ +0x84 — sanitized to 0 at bc init.

**Abort trap** (cpt maps VA 0x0 + 0xFFFF0000 → PA 0x9FE00000):
- Prefetch abort vector @ 0x0C → branches to handler @ 0x40
- Data abort vector @ 0x10 → same handler
- Handler: writes 0xAB to bc[1] (VA 0xD00000A0), PL310 CIPA flush, spins

## eMMC Variants (Two Units)
| Unit | eMMC | Capacity | Notes |
|------|------|----------|-------|
| Primary (current) | Samsung MCGAFA | 64 GB | **Suspect DRAM degradation** |
| Secondary | SanDisk SEM32G | 32 GB | Untested for kernel runs |

**Important**: DRAM is separate from eMMC. The "unit" refers to the whole tablet. Both have 1 GB RAM but may have different DRAM chips/boards.

## Boot Chain
1. **Boot ROM** (internal) → loads QNX IFS from eMMC
2. **QNX IPL/Startup** → initializes DRAM, peripherals, starts procnto
3. **QNX Userspace** (runlevel 2) → SSH/RNDIS up (~2-3 min)
4. **Our Payload** (qnx2linux) → kexec jump to Linux
5. **Linux zImage** → decompresses to 0xa0008000 *(CORRECTION
   2026-09-04, session 8: was "0x80008000" — AUTO_ZRELADDR's 128 MB
   bucket for all our buffer placements; PHYS_OFFSET = 0xa0000000, the
   DTS bank must match — docs/03 run W-21)*, enables MMU, starts kernel
6. **WDT2** (if not kicked) → resets board **60 s** after the last kick (user-timed
   exactly, 2026-09-01; the "15 s" in older docs = QNX's wdtkick PERIOD, not the window)
7. **QNX Reboot** → breadcrumbs + rings survive for forensics

## QNX 6.6 Specifics
- **ThreadCtl(_NTO_TCTL_IO_PRIV)** → System mode (required for CP15, SMC)
- **mmap_device_memory** with `PROT_NOCACHE | MAP_PHYS` → device/physical mapping
- **mem_offset64** → get physical address from virtual
- **SMC #0** → TI HAL monitor calls (service 0x101 = L2 range flush)
- **No `od`, `head`** — use `memdump3` or custom tools
- **`dd` numeric args only** — no `skip=` with non-numeric
- **`pidin mem`** → list physical memory regions (verify address live before probing)

## ACTLR / Diagnostic Register (QNX State)
- **ACTLR** (c1, c0, 1): QNX reads 0x41 = SMP (bit 0) | Broadcast (bit 1) — benign
- **Diagnostic** (c15, c0, 1): QNX reads 0x810 = errata bits — benign
- Kernel ORs its ACTLR bits onto this (proc-v7 preserves unknown bits)

## WDT2 Behavior
- QNX's `wdtkick` daemon runs every 15s (`omap4430-wdtkick -t 15000`), writes
  the TGR trigger (0x4A314030) → CRR reloads from WLDR → the 58.6 s window restarts
- If not kicked: board warm-resets — **the timeout window is 58.6 s**
  (WLDR=0xFFE2B400 @ 32.768 kHz, PTV=0; verified from the live registers
  2026-09-02). The "15 s" figure = the wdtkick PERIOD; the "60 s" figure was
  the pre-measurement approximation.
- Our payload: early kick (bc-armed) + late bare-register kick (pre-enter_stub)
- The KERNEL also kicks per marker/per initcall (WDT kicks in the marker
  macros, 2026-09-02) — with the kicks working, the device stays alive as
  long as the boot progresses
- **Without kicks: death wherever the kernel happens to be when the 58.6 s
  expires — the boot is deterministic, so the death point is too (observed
  in the L2-off era; the mode is retired — see the NIGHT UPDATES above)**

## UART3 / The Monitor's Capture Ring (2026-09-02)
- UART3 = 0x48020000 (console=ttyO2); **no external pads accessible** (a
  2-hour pad hunt found nothing — do not re-attempt)
- **The secure monitor taps UART3's TX and appends every byte to a capture
  buffer at 0x94000100** (count @0x94000080, index @0x94000084, cumulative,
  persistent across WDT2 resets — the bookkeeping lives in the secure world)
- The capture CANNOT be cleared from NS (zeroing the headers gets re-written
  by the monitor on the next capture)
- The kernel's console (earlycon0 → printascii → omap4bc.S) writes UART3 →
  **the monitor captures it → the full boot log is readable post-reboot** —
  the primary post-mortem evidence channel
- Run 23's kernel's 23 KB boot log was recovered this way
  (SESSION-HANDOFF/ring3-recovered-log-2026-09-02.txt)
- The capture buffer's char window is 1 KB (0x94000100–0x940004FF, wrapping);
  the region beyond holds an old 0x55-filled buffer (the splash-era fill)

## PL310 State as Left by QNX (verified 2026-09-01 via NS reads)
- cache-id (0x000) = 0x410000C4 → L310 r3p2
- control (0x100) = 0x1 (enabled); aux (0x104) = 0x1E070000
- tag latency (0x108) = 0x00000000; **data latency (0x10C) = 0x00000111 (1-cycle!)**
- prefetch ctrl (0xF60) = 0x5; power ctrl (0xF80) = 0x0
- **NS write access is filtered**: control write = bus hang; data-latency write =
  synchronous SIGBUS (fltno=5); by-way (0x7FC) background clean+inv = deadlock
  (erratum 727915 class). Only by-PA (0x768) + sync (0x730) maintenance are safe
  from NS.
- **Monitor service table (RE'd 2026-09-02, mainline omap-secure.h + on-device)**:
  SMC #0, r12=service: 0x100 DBG_CTRL, **0x101 clean+inv by PA (verified)**,
  **0x102 CTRL write (disable = r0=0; WORKS from the payload's C-flow mon_call,
  4/4)**, 0x103/4/5 auxcoreboot, 0x108 SCU_PWR, 0x109 AUXCTRL, 0x113 PREFETCH.
  No tag/data-latency service exists. See docs/03 for the flakiness matrix.

## Display Stack (Must Quiesce Before Jump)
- `splash` — boot animation, writes framebuffer
- `backlight_win` — backlight control
- `screen` — window compositor, GPU active
- **Kill all three**: `slay splash backlight_win screen`
- DISPC keeps scanout-reading last frame — the payload now ALSO blanks it
  pre-jump (DISPC_CONTROL 0x48050440, GFX 0x480504A0, VID1 0x480504C0,
  VID2 0x48050500 = 0) to remove EMIF contention (2026-09-01)
- Everything restarts on WDT2 reboot

## Known Working / Verified
| Component | Status |
|-----------|--------|
| T2 kexec (hello.bin) | ✅ Works — breadcrumbs 20/21/23 + blob step 3 + count>0 |
| PL310 L2 flush via SMC 0x101 | ✅ Verified (smcprobe) |
| IRAM flat table + trampoline | ✅ Works |
| CPU1 hold in reset | ✅ Safe |
| GICD off | ✅ Required |
| WDT2 kick strategy | ✅ Works (TGR writes; the section-PA bug fixed run 30) |
| zImage decompressor | ✅ Works |
| **L2 disable via SMC 0x102** | ✅ **Works (C-flow mon_call, 4/4) — the L2-off kernel boots deep** |
| Memtest payload | ✅ Executed — DRAM clean |

## Known Broken / Open (2026-09-02 night)
| Component | Status |
|-----------|--------|
| **Secure-domain L2 config** | ❌ PL310 data latency = 0x111 (1/1/1) live, NS write = SIGBUS; no monitor latency service — the corruption root; fix = RE trustzone-omap4 |
| **dma_contiguous_remap wall** | ❌ Corrupted dma_mmu_remap[0].base at bc=127 — a symptom of the above (see 05_NEXT_STEPS) |
| L3/EMIF auto-idle | ⚠️ Suspected silent-corruption contributor; NS writes = SIGSEGV; needs the monitor's PPA services |
| L2X0 driver latency write | ⚠️ CONFIG_CACHE_L2X0=y enabled; the driver's NS latency write = SIGBUS — guard cache-l2x0.c |
| ~~The 171 wall~~ | ✅ SOLVED session 6 (the L2-off bypass cliff + the uncached kernel; the L2-off mode retired) |
| ~~Console output~~ | ✅ WORKING (cacheable rings + SMC 0x101 flushes; UART3 never written by the kernel) |
| Uncompressed Image boot | ❌ Architecturally broken (r2 loss parked — DECISIONS D6) |
| UART pads | ❌ None accessible (2-hour hunt) — replaced by the DRAM console rings (ring1; see the night update above) |
| L2X0 by-way op (l2c_enable) | ⚠️ The REAL driver hazard when the boot reaches init_IRQ: `L2X0_INV_WAY` (0x7FC) = the NS deadlock op (KNOWN_ISSUES #3) |

## Debug UART
- **Physical**: UART3 @ 0x48020000 (48 MHz clock; console=ttyO2; the QNX IFS
  runs `devc-seromap -e -F -b115200 -c48000000/16 0x48020000`)
- **QNX**: `/dev/ser3`-style (devc-seromap)
- **Linux earlyprintk**: VA 0xFED20000 (section 0xFED — mapped in head.S since
  run 32; before that the MMU-on console writes vanished into the L3)
- **Pads**: NONE accessible (2-hour hunt, 2026-09-02) — **the monitor's ring3
  capture is the only console readback**
- **Payload UART loop** (hello.bin): polls UART3 FR/BUSY, prints breadcrumbs

## I2C3 LED (FAN5702)
- Slave: 0x36 (7-bit)
- Register 0x10 (GENERAL), bit 3 = BLUE
- Devctl: `0x80100505` (SEND), 18-byte buffer
- Used by payload for visual status (blue = armed)