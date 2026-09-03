# Kernel Debugging Guide

## 2026-09-02 NIGHT STATUS UPDATE (session 6 — SUPERSEDES the section below)
**The console WORKS** (cacheable DRAM rings + batched SMC 0x101 flushes +
PB-PANIC) and the kernel boots cacheable to **bc=127 (paging_init:
map_kernel done)**, dying at dma_contiguous_remap via a corrupted
dma_mmu_remap[0].base. **The 171 wall is SOLVED/RETIRED** (the L2-off bypass
cliff + the head.S C/B/S strip; the L2-off mode itself is retired — --l2on
is the mode). The current kernel also has:
- The ring sections (0xC80/0xD00/0xD40) CACHEABLE (r7); the peripheral
  sections (0xFEB/0xFEC/0xFED) + vectors DEVICE (r6)
- **UART3 writes REMOVED from omap4bc.S** (senduart/LSR drain) — UART3 dies
  post-idle; a posted store to the dead THR stalls the store buffer. The
  monitor's UART3 capture is INACTIVE; ring1 = the primary readback
- **All flushes = pb_smc_flush (monitor SMC 0x101, C-flow, synchronous)** —
  zero unbounded polls boot-wide; the flush-free-marker claim is superseded
  (flush-free stores strand dirty in L2 when the L2 is on)
- **ACTLR.SMP left at 1** (clearing it wedged boots); the TTB-flags strip
  REMOVED (kernel RAM = WBWA — the UNCACHED kernel was the 120/171 walls)
- CONFIG_CMDLINE_FORCE=y (the cmdline was placement-flaky); PB-ADJ/PB-MEM
  prints in adjust_lowmem_bounds/arm_memblock_init; the memblock limit
  forced to end_of_DRAM (adjust computed 0); the omap4 dram barrier
  DISABLED (its stack struct was corrupted; the WFI erratum workaround is
  off for bring-up)
- CONFIG_CACHE_L2X0=y + the pl310 DTS node @ 0x48242000 (latencies <3 3 3>).
  UPDATE 2026-09-03: the latency-SIGBUS guard is a NO-OP (writes route via
  omap4_l2c310_write_sec = WARN+skip); the real l2x0_of_init hazards are the
  by-way 0x7FC write (deadlock op) + l2c_wait_mask poll — see 07/03.
- **The corruption root** = the secure-domain L2 config (PL310 data latency
  0x111 live, NS write = SIGBUS; L3/EMIF auto-idle secure-only) — the fix =
  RE the trustzone-omap4 monitor (see 05_NEXT_STEPS.md)
The sections below are kept for reference; the "M=1 Hang" analysis is
historical (its root cause was NOT any of the listed theories — it was the
ring-map shift bug).

## 2026-09-02 STATUS UPDATE (late — HISTORICAL, superseded above)

## Current Kernel State
- **Source**: Mainline Linux (version per build)
- **Config**: `omap4-winchester` defconfig + DEBUG_LL + EARLY_PRINTK
- **Format**: zImage with appended DTB (`omap4-winchester.dtb`)
- **Load**: Payload places at buffer+0x8000 (1MB aligned + 0x8000)
- **Entry**: zImage base (word 0 = `b start`), r2 = DTB phys
- **Decompressor**: Relocates to zreladdr 0x80008000, PHYS_OFFSET=0x80000000

## Boot Flow (Kernel Side)

```
zImage entry (compressed/head.S)
    ├─ Decompressor: relocates kernel to 0x80008000
    ├─ TLBIALL ×6 (clears all TLB entries — QNX/flat-table TLB gone)
    ├─ Setup early page tables (identity for decompressor)
    ├─ Jump to decompressed kernel @ 0x80008000
    │
    └─ Kernel stext (kernel/head.S)
        ├─ __create_page_tables
        │   ├─ bc=114 (ring-block)
        │   ├─ bc=115 (cpt done)
        │   └─ bc=119 (cpt-returned)
        │
        ├─ __enable_mmu
        │   └─ bc=121
        │
        ├─ __turn_mmu_on  ← **HANGS HERE (bc=122)**
        │   ├─ SCTLR M=1, C=1, I=1
        │   ├─ ISB
        │   └─ Never returns
        │
        ├─ (If it continued:)
        ├─ bc=124 (first post-M store VA 0xD0000004)
        ├─ bc=123 (post-M flushed)
        ├─ mmap_switched (bc=120)
        ├─ start_kernel (bc=118)
        ├─ paging_init (bc=110)
        └─ setup_arch (bc=111)
```

## DEBUG_LL / Earlyprintk Capture

### The console chain (2026-09-02 reality)
```
printk → earlycon0 (early_printk.c console) → early_write → printascii
      → debug.S → addruart_current → omap4bc.S addruart
      → UART3 (PA 0x48020000; VA 0xFED20000 — section 0xFED, mapped in head.S
        since run 32 — BEFORE that it was unmapped and the L3 swallowed every
        MMU-on write silently!)
      + THREE DRAM ring mirrors via busyuart's ring_put:
        ring3 = 0xD4000080/0xD4000100 (PA 0x94000080/0x94000100) ← THE MONITOR
                captures UART3 here too (persistent, cumulative count)
        ring2 = 0xD0000080/0xD0000100 (PA 0x90000080/0x90000100)
        ring1 = 0xCC000080/0xCC000100 (WRONG — PA 0x8C000000 unmapped; the
                correct alias would be 0xC8000080 → PA 0x88000080; the ring1
                writes are silently dropped by the L3 — known omap4bc.S bug)
      + a PL310 CIPA+sync per touched line (guarded: skipped when CTRL=0)
```

### Ring Layout (omap4bc.S)
Each ring page (0x88000000, 0x90000000, 0x94000000):
```
+0x00–0x7F:  Reserved / headers
+0x80:       count (u32) — sanitized to 0 at bc init
+0x84:       index (u32) — sanitized to 0 at bc init
+0x100–0x4FF: chars (1024-byte circular window)
```
CAVEAT: the monitor's UART3 capture (ring3) maintains its OWN cumulative
count — the payload's header zeroing does not stick. Use the content + the
bc[15] nonce to judge freshness.

### Read Post-Reboot
```bash
# Ring3 = THE primary (the monitor's UART3 capture — full boot log tail)
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 94000080 0x8"
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 94000100 0x400"
# Ring1/2 (usually stale — the console writes there are partially broken)
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 88000080 0x20"
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 90000080 0x20"
```

### Expected Output If Working
```
count=XX index=YY
chars: "Uncompressing Linux... done, booting the kernel.\n"
       "[    0.000000] Booting Linux on physical CPU 0x0\n"
       "[    0.000000] Linux version ...\n"
       ...
```

### If Silent at M=1
Rings will show decompressor messages but **no kernel messages past `__turn_mmu_on`**.

## Kernel Config Essentials

### Required
```kconfig
CONFIG_ARM=y
CONFIG_ARCH_OMAP2PLUS=y
CONFIG_MACH_OMAP4_WINCHESTER=y
CONFIG_DEBUG_LL=y
CONFIG_EARLY_PRINTK=y
CONFIG_ZBOOT_ROM_TEXT=0x80008000
CONFIG_ZBOOT_ROM_BSS=0x80000000
CONFIG_CMDLINE="earlyprintk console=ttyO2,115200"
```

### For Minimal Debug Build (If M=1 Persists)
```kconfig
# Disable everything possible
CONFIG_SMP=n
CONFIG_MODULES=n
CONFIG_BLK_DEV_INITRD=n
# Only essential drivers:
CONFIG_SERIAL_8250=y
CONFIG_SERIAL_8250_CONSOLE=y
CONFIG_SERIAL_OF_PLATFORM=y
# No USB, no MMC, no GPU, no WiFi, no Bluetooth
```

## Adding Kernel Breadcrumbs (If Needed)

### Patch `kernel/head.S` (or `arch/arm/kernel/head.S`)
```asm
/* In __turn_mmu_on, after enabling MMU: */
#ifdef CONFIG_DEBUG_LL
    ldr r0, =0x90000004    @ bc[1]
    mov r1, #0xC0          @ custom step: post-M
    str r1, [r0]
    /* PL310 flush if needed */
#endif
```

### Patch `__create_page_tables`
```asm
/* After page table creation: */
#ifdef CONFIG_DEBUG_LL
    ldr r0, =0x90000004
    mov r1, #115
    str r1, [r0]
#endif
```

### Rebuild Kernel
```bash
# In kernel source tree:
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- omap4_winchester_defconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) zImage
# Append DTB:
cat arch/arm/boot/dts/omap4-winchester.dtb >> arch/arm/boot/zImage
# Deploy:
scp -i ../rsa arch/arm/boot/zImage root@169.254.0.1:/tmp/
```

## OMAP4-Specific Errata Workarounds

### Check Kernel's `omap4_errata.c`
```c
/* Common OMAP4 errata handled by kernel: */
- ERRATA_I688: CPU may hang if MMU enabled with specific cache state
- ERRATA_I753: L2 cache maintenance requires specific sequence
- ERRATA_I759: Data cache clean may not complete
```
Verify kernel has `CONFIG_OMAP4_ERRATA=y` and errata init runs early.

### PL310 Errata (Hardware)
- PL310 r3p2 has known errata: 753970, 764419, etc.
- Kernel's `pl310.c` applies workarounds in `pl310_init()`
- But **our flat table bypasses kernel's PL310 init** — kernel re-inits PL310 after MMU on

## The M=1 Hang: Detailed Analysis

### What Happens at `__turn_mmu_on`
```c
/* arch/arm/kernel/head.S */
__turn_mmu_on:
    /* r0 = SCTLR value with M=1, C=1, I=1 */
    mcr p15, 0, r0, c1, c0, 0    @ write SCTLR
    isb                          @ instruction sync barrier
    /* Next instruction fetched with MMU ON */
    /* If page tables wrong → prefetch abort */
    /* If translation fault → data abort on first access */
```

### Why No Abort Fires
- Our abort trap at VA 0x0/0xFFFF0000 → PA 0x9FE00000 catches **prefetch/data aborts**
- bc[1] ≠ 0xAB post-reboot → **no abort occurred**
- Possibilities:
  1. **Infinite loop in `__turn_mmu_on`** (unlikely — it's 2 instructions)
  2. **Jump to wrong address** after ISB (PC corruption?)
  3. **Cache coherency** — new page tables not visible to I-fetch?
  4. **SCTLR bits** — something beyond M/C/I causes hang (alignment, branch pred, etc.)
  5. **L2 cache state** — PL310 not flushed properly for new tables?

### Probe's M=1 Survives — Key Difference
| Aspect | Probe (step 53) | Kernel |
|--------|-----------------|--------|
| Runs from | IRAM (0x40308000) | DRAM (0x80008000) |
| Page tables | Flat table (uncached desc) | Kernel's own tables |
| Descriptors | Uncached (C=B=0) | Cached? (kernel default) |
| L2 state | Flushed by payload | Kernel re-inits PL310 |
| ACTLR | QNX state (0x41) | Kernel ORs onto it |

### Test: Force Uncached Descriptors in Kernel
Patch kernel's early page table creation to use `0xC02` (AP=11, C=0, B=0) instead of default `0x40E` (or whatever). If survives → descriptor cacheability was the issue (but we tested this in payload and it didn't help... wait, we tested C/I=0 post-M in payload and it still hung. But kernel's descriptors are different.)

Actually the session record says: "Cacheable vs uncached early descriptors: both hang — linefill-snoop via descriptor attrs innocent". This was tested by modifying payload's flat table descriptors. But kernel builds its own tables.

### Test: Clear ACTLR Before M=1
In kernel's `__enable_mmu`:
```asm
mrc p15, 0, r0, c1, c0, 1   @ read ACTLR
bic r0, r0, #0x3            @ clear SMP + broadcast
mcr p15, 0, r0, c1, c0, 1   @ write ACTLR
```

### Test: Explicit L2 Clean+Inv Before M=1
In kernel's `__enable_mmu` (if PL310 accessible at VA):
```asm
/* PL310 clean+inv by way */
ldr r0, =0xFEB42000          @ PL310 VA (rebased)
mov r1, #0xFFFF
str r1, [r0, #0x7FC]
1: ldr r1, [r0, #0x730]
tst r1, #1
bne 1b
```

## Minimal Kernel Build for Isolation

### Strategy
Build kernel with **only**:
- Earlyprintk/DEBUG_LL
- Serial console (UART3)
- No SMP, no modules, no initrd, no filesystems, no drivers
- Static `init` that just prints and spins

### Config Fragment (`minimal.config`)
```kconfig
CONFIG_ARM=y
CONFIG_ARCH_OMAP2PLUS=y
CONFIG_MACH_OMAP4_WINCHESTER=y
CONFIG_DEBUG_LL=y
CONFIG_EARLY_PRINTK=y
CONFIG_CMDLINE="earlyprintk console=ttyO2,115200"
CONFIG_SMP=n
CONFIG_MODULES=n
CONFIG_BLK_DEV_INITRD=n
CONFIG_SERIAL_8250=y
CONFIG_SERIAL_8250_CONSOLE=y
CONFIG_SERIAL_OF_PLATFORM=y
CONFIG_INITRAMFS_SOURCE=""
# Disable all filesystems, block, net, usb, gpu, etc.
```

### Build
```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- minimal.config
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) zImage
```

### Deploy & Test
```bash
cat arch/arm/boot/dts/omap4-winchester.dtb >> arch/arm/boot/zImage
scp -i ../rsa arch/arm/boot/zImage root@169.254.0.1:/tmp/
./jump.sh zImage
```

If minimal kernel also hangs at M=1 → issue is in core MMU enable path, not drivers.

## Comparing T2 vs T3

| Aspect | T2 (hello.bin) | T3 (zImage) |
|--------|----------------|-------------|
| Target | MMU-off blob | Linux kernel (MMU-on) |
| Entry | buf+0x8000 (identity) | zImage base (decompressor) |
| Page tables | Flat (identity) | Kernel's own |
| MMU | Disabled in cont | Enabled in `__turn_mmu_on` |
| Result | ✅ Survives | ❌ Hangs at M=1 |
| BC reached | 20→21→23→3→4 | 100→...→122→silence |

**Key insight**: The trampoline/continuation (MMU-off) works perfectly. The failure is **specifically in the kernel's MMU enable sequence**.

## Next Kernel Debug Steps (Priority Order)

1. **Read DEBUG_LL rings** post-reboot — confirm kernel produces zero output past decompressor
2. **Build minimal kernel** (no drivers, no SMP) — test if M=1 still hangs
3. **Add bc marker in kernel's `__turn_mmu_on`** — confirm it's reached, then dies
4. **Try uncached descriptors in kernel** — patch early page table creation
5. **Clear ACTLR in kernel** — before M=1
6. **Explicit PL310 flush in kernel** — before M=1
7. **Compare with working ARMv7 board** — same kernel config on known-good hardware?

## Kernel Build Environment (Reference)
- **Cross-compiler**: `arm-linux-gnueabihf-` (different from QNX toolchain!)
- **Defconfig**: `omap4_winchester_defconfig` (or `omap2plus_defconfig`)
- **DTB**: `omap4-winchester.dts` → `omap4-winchester.dtb`
- **Append DTB**: `cat zImage omap4-winchester.dtb > zImage-dtb`
- **Payload expects**: zImage with appended DTB at `/tmp/zImage`

## Useful Kernel Source Locations
```
arch/arm/kernel/head.S          # __turn_mmu_on, __create_page_tables
arch/arm/kernel/head-common.S   # Common head code
arch/arm/mm/mmu.c               # Page table helpers
arch/arm/mm/init.c              # paging_init, map_lowmem
arch/arm/mach-omap2/omap4_errata.c  # OMAP4 errata
drivers/memory/pl310.c          # PL310 driver
include/asm/debug-ll.S          # DEBUG_LL macros
kernel/printk/printk.c          # earlyprintk
```