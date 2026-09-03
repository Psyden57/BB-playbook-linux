# PlayBook kexec-style Linux boot — Design Document
Status: DRAFT v0.1 (2026-08-29). Companion to `KEXEC-PROMPT.md` (mission), `PLAYBOOK-REFERENCE.md` (device ground truth), `SESSION-HANDOFF.md` (session state). Addresses deliverables 1, 2 and 4 of the prompt; code comes after this doc is reviewed.

---

## 1. Prior-art research summary (deliverable 1)

### 1.1 What exists and what doesn't
- **No PlayBook Linux kernel trees exist** (XDA-era or otherwise — confirmed by search; also no `blackberry-playbook` device in pmaports `device/testing` or `device/community`). The "community kernels" mentioned in KEXEC-PROMPT.md never existed. Everything device-specific must be built from scratch; everything SoC-specific is already upstream.
- **Mainline OMAP4430 support is in excellent shape.** pmOS ships `linux-postmarketos-omap` = vanilla kernel.org 6.15 + ~11 mostly-DTS patches (N900, Galaxy Espresso, Nook Encore). Droid 4 (`omap4-droid4-xt894.dtb`), Blaze/Xoom, PandaBoard, and Galaxy Espresso (OMAP4430 tablets!) are in-tree. The Espresso boards are the closest analogues: OMAP4430 + TWL6030 + eMMC + MIPI panel tablets.
- **droid4-kexecboot (tmlind) is the mechanics model** (same SoC): stock bootloader → tiny buildroot kernel+initramfs on a spare eMMC partition → kexecboot UI → `kexec` into the real kernel. Note its stage-1 kernel is booted *by the stock bootloader*; ours must be reached *from inside QNX* instead — that first jump is the part nobody has done on this device.
- **PlayBook hardware BOM** (iFixit/TechInsights/Chipworks teardowns, cross-checked with the IFS driver list): OMAP4430, 1GB Elpida DRAM (PoP), SanDisk/Samsung eMMC, **TWL6030 PMIC** (IFS: `pmic-winchester-twl6030`), **TI WL1283 WiLink 7** (mainline `wl128x` driver), Wolfson WM8994E codec (mainline), Cypress CY8CTMA3 touch (SPI bus per `.script`), MPU-3050 gyro + BMA150 accel (both mainline), Intersil ISL9519 charger, ST camera ISP, 1024×600 MIPI DSI panel (driver unknown — no candidate; display is a later phase).

### 1.2 UART — solved, not a problem
`dumped4869ifs/proc/boot/.script` line 3:
`devc-seromap -e -F -b115200 -c48000000/16 0x48020000` → **OMAP4 UART3, PA 0x48020000, 115200 8N1, 48MHz UART clock.** Same port the bootrom uses ("Starting RIM bootrom with UART_DEBUG enabled"). Consequences:
- The QNX→Linux stub needs no driver to emit boot diagnostics: poll-write LSR/THR at 0x48020000. Linux: `console=ttyO2,115200n8 earlyprintk` (and `earlycon` if we add `stdout-path` in the DTS).
- Externally reachable pads still unknown, but TX is now identifiable: `echo x > /dev/ser1` from the QNX shell while scoping probe pins = a driven 115200 signal. The bootrom's own output guarantees the line exists on the PCB somewhere.
- **Blind-debug fallback** (no UART probe found): the stub/kernel can encode progress in LED/backlight blinks (TWL6030 LED regs via I2C1) — cruder but workable. Keep as plan B.

### 1.3 ATags vs DTB decision: **DTB**
Mainline omap4 is DT-only; `DT_MACHINE_START` sets `.nr = ~0`, so **r1 = 0xffffffff + r2 = DTB physical address** selects DT boot (standard multiplatform convention). Belt-and-braces: build the target kernel with `CONFIG_ARM_APPENDED_DTB` (+ optionally `CONFIG_ARM_ATAG_DTB_COMPAT`), so a partially-buggy r2 still boots from the appended blob. No ATags path will be implemented.

### 1.4 SoC facts this design relies on (verify-on-device where marked)
| Item | Value | Source/confidence |
|---|---|---|
| RAM | 0x80000000–0xBFFFFFFF, 2×512MB, Elpida | cfp info (§1.6 ref doc) — solid |
| IRAM (OCMC) usable | 0x40304000 (cfp "IRAM Base"), OMAP4430 OCMC = 56KB @ 0x40300000 | cfp — solid; on-device size check TODO |
| GIC distributor | 0x48241000 | omap4.dtsi — solid |
| GIC CPU interface | 0x48240100 | omap4.dtsi — solid |
| L2C-310 (1MB) | 0x48242000 [verify] | omap4.dtsi — check with safe memdump |
| WDT used by QNX | timer/WDT base from `omap4430-wdtkick` binary [TODO: strings/RE] | wdtkick kicks every **15s** (`-t 15000`) |
| Secondary-core boot regs | CONTROL_AUX_CORE_BOOT0/1 = 0x4A002E08/0x4A002E0C [verify] | OMAP4430 TRM — check |
| CPU1 reset bit | PRCM RM_CPU1_CPU1_RST [address TODO from TRM] | TRM — must locate before relying on it |
| QNX OS image in DRAM | 0x800FF800–0x803BF60F | cfp "OS Address" — solid |

Known hazard (PLAYBOOK-REFERENCE §10.2): speculative reads of unmapped/clock-gated bus = async external abort + watchdog reset. Every register probe in the plan below uses `memdump.c` v2 semantics (`mmap_device_memory`, PROT_NOCACHE) and only addresses already proven by the IFS drivers or DTS.

---

## 2. Architecture decision (Option A vs B)

**Recommendation: Option A first, Option B as a later evolution — same payload, two payloads supported.**

Reasoning:
- The droid4 motivation for two stages (boot menu, kernel selection, no reflashing) is largely satisfied **without** a second Linux: our injection point is a full QNX userland (`env.sh`). A tiny menu in the payload (default: do nothing → normal QNX boot; opt-in via a flag file or timeout) gives boot selection at zero extra cost. Each `.signed` UFS repack already persists any payload change.
- Every additional jump (QNX→kexecboot, kexecboot→target) repeats the risky QNX→ARM-state handoff. Option A does the risky handoff exactly once per boot.
- Once any Linux boots, kexec *inside* Linux is stock tooling — Option B then costs one extra small kernel+initramfs and becomes trivially addable (the payload already knows how to load "some kernel + initramfs"; kexecboot is just one of the loadable targets).
- Phase 1 (this design) therefore implements the QNX→Linux jump directly. The memory layout is identical for B's kexecboot target later.

---

## 3. Memory map of the jump

```
0x80000000 +0          ┬ 1GB DDR (2×512MB banks)
0x800FF800             │ ← QNX OS image region (cfp OS Address, ends 0x803BF60F);
                       │   above it: procnto, drivers, splash, our payload process...
0x84000000 (TBD, see 3.1) ┬ JUMP REGION (allocated by payload, MAP_PHYS, pinned):
                          │   target kernel zImage  (~8–16MB)
                          │   target DTB            (≤256KB)
                          │   (later: initramfs)    (~10MB)
                       │
0x40304000  IRAM       ┬ STUB REGION (clobberable — boot-time remnants only):
0x40304000 …           │   stub code (PIC asm, <1KB)
0x4030C000             │   16KB flat 1st-level page table (identity map, see 5.2)
                       ┘
```

### 3.1 Choosing the jump-region base
Hard requirements:
- Above everything QNX has allocated **at the time of the jump** (measured, not guessed: `showmem`/`pidin` on the 2.1 unit post-boot; on the 2.0 unit read procnto's free-list bounds via /dev/mem only if needed — the MAP_PHYS allocator (below) already guarantees non-overlap, the base only matters for the DTB `memory` node and for staying inside bank 0 until the DTB claims all 1GB).
- Below 0xA0000000 (bank 0) initially; zImage self-decompresses and relocates, so exact address is not load-bearing, but keep everything in one bank for the first phases.
- Working value: **0x84000000** (64MB into DRAM) — comfortably above the whole OS image region + typical QNX usage; adjust after on-device `showmem` evidence.

### 3.2 How the payload pins the jump region
`mmap(0, size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PHYS, ...)` — QNX's documented physically-contiguous allocation; procnto cannot hand those pages to anyone else, which satisfies the "QNX won't touch it between load and jump" requirement without early-boot tricks. The kernel image is read from a file on the UFS (SCP'd over USB while QNX runs, or baked into the `.signed` UFS) into this buffer.
- IRAM pages are taken via `mmap_device_memory` on 0x40300000–0x4030FFFF (read first: verify only boot remnants, no live code — devc/procnto don't use OCMC in the IFS driver list).

---

## 4. The QNX-side payload (`qnx2linux`, C, root, /dev/mem-based)

Runs as `ksh /base/scripts/startup.sh` → sources our `/radio/scripts/env.sh` (UFS, 2.0.0.4869). Context: root userland; devb, i2c×3, TWL6030, spi3, UART, splash, backlight, rpmb, nvram all already running (ref doc §1.7). Sequence:

1. **Gate**: if flag file absent → exit (normal QNX boot). Optional 5s menu if present.
2. **Load target**: read zImage + DTB from UFS into the MAP_PHYS buffer at the chosen physical addresses; verify simple header magic (`0x016f2818` for a raw Image, zImage has its own head) and DTB magic `0xD00DFEED`. `sync` discipline per ref §10.1.
3. **Quiesce** (§6): kill splash + backlight, `slay devb-mmcsd-winchester` (after closing our file handles!), optionally kill wdtkick later in the plan (§7).
4. **Prepare CPU1**: park/stop the secondary core (§5.3).
5. **Build stub** in IRAM: flat page table + asm blob, parameterized with (kernel entry PA, DTB PA, UART still @0x48020000).
6. **Last kicks**: clean/invalidate all caches from C (`msr`/CP15 + PL310 if present), disable GIC distributor+interface, then enter the stub via the QNX-side mapping of IRAM.
7. Never returns. On failure → watchdog/power-cycle → normal QNX boot (nothing persisted: the only flash writes ever are the UFS repack, already a proven-safe path).

Process-context caveat: between steps the QNX scheduler may run other threads on CPU0 — harmless (we only reserve memory and stop services). The critical non-preemptible window begins inside the stub (first instruction `cpsid if`) and ends at the jump; see 5.3.

---

## 5. The jump stub (modeled on ARM `machine_kexec` + `relocate_kernel.S`, 3.x/6.x)

### 5.1 What ARM Linux requires at entry (mmu-off CPU0)
- r0 = 0; r1 = 0xffffffff (DT_MACHINE selector); r2 = DTB physical address (8-byte aligned).
- IRQs/FIQs/aborts masked; MMU, D-cache, I-cache off; SCTLR baseline.
- L1 + L2 cleaned+invalidated to point-of-coalescence so the loaded image and DTB are visible with caches off.
- GIC quiet (no in-flight IRQs).

### 5.2 The identity-mapping problem, and its solution
The classic kexec path runs the MMU-off switch from an *identity-mapped page* Linux prepared at boot. QNX gives us no such mapping, and executing through QNX's `/dev/mem` mapping breaks the moment the MMU turns off (PC is then a raw virtual address that maps to something arbitrary on the physical bus). Solution: **build our own tiny 1st-level page table in IRAM** (16KB, section entries only, at 0x4030C000):

- 0x40300000–0x4030FFFF → identity (stub code region)
- 0x80000000–0xBFFFFFFF → identity (so the final jump target and any data are flat)
- the QNX virtual page(s) through which we *entered* the stub → 0x40304000 (so we survive the TTBR0 switch before the branch to the identity-mapped region)

Execution sequence (all in the IRAM blob, position-independent):
```
enter (via QNX mapping of IRAM, MMU on, QNX TTBR0):
  cpsid if                      ; from here nothing may preempt us
  ldr ttbr0 = 0x4030C000 | attrs ; switch to our flat table
  isb; dsb
  ldr pc, =0x40304000+<part2>   ; branch into identity-mapped IRAM
part2 (PC == PA, our table):
  clean+invalidate L1 (all sets/ways, CP15 c7)
  clean+invalidate L2 PL310 (if present @0x48242000 [verify])
  tlbiall; isb
  sctlr: M=0, C=0, I=0, (Z=0), isb
  mov r0,#0; ldr r1,=0xffffffff; ldr r2,=<DTB_PA>
  ldr pc,=<KERNEL_PA>           ; zImage base (word 0 is `b start`; see 8.2 note)
```
Everything above is a well-trodden shape — it is `relocate_kernel.S` with QNX's absence of an identity map compensated by the IRAM page table. Estimated size < 100 instructions; IRAM budget is not a concern (16KB table + ~1KB code vs 56KB total, and only boot remnants otherwise).

### 5.3 Secondary core (CPU1) — SOLVED (2026-08-30, TRM swpu231ap + live-verified)
All registers are non-secure memory-mapped; no monitor services needed. (Earlier
PRCM_MPU/AUXCOREBOOT firewalls and monitor-service failures were wrong-address
artifacts: the real AUX_CORE_BOOT regs live in WUGEN, and the CPU reset regs in the
MPU local PRCM at 0x48243xxx — inside the region powerman already maps.)

| Register | Address | Purpose |
|---|---|---|
| WKG_CONTROL_0 / _1 | 0x48281000 / 0x48281400 | per-CPU live status (STANDBYWFI bit 8, reset-cause bits). Read-only. |
| AUX_CORE_BOOT_0 | 0x48281800 | ROM pen status ("boot me" flag for CPU1) |
| AUX_CORE_BOOT_1 | 0x48281804 | ROM pen jump address for CPU1 |
| RM_PDA_CPU0/1_RSTCTRL | 0x4824340C / 0x4824380C | bit0 RST: per-CPU software warm reset; **held until cleared by the other CPU** (TRM Table 4-30: "one CPU can set this bit to reset the other CPU") |
| RM_PDA_CPUi_CONTEXT | 0x48243408/0x48243808 | context-lost status (W1toClr) |
| CM_PDA_CPUi_CLKCTRL | 0x48243414/0x48243814 | CPU clock control |

Live-verified state: boot0=0x2, boot1=0x8010e0f4 (QNX's own secondary startup — QNX
boots CPU1 through the ROM pen), RSTCTRL=0 (running), WKG_CONTROL_1=0x700 (WFI bits).

**Jump-time procedure (payload step 4):**
1. Optionally wait for WKG_CONTROL_1 STANDBYWFI=1 (CPU1 idle, caches clean-ish).
2. Write bit0=1 to RM_PDA_CPU1_RSTCTRL (0x4824380C) → CPU1 held in warm reset.
3. Continue with quiesce + jump. Linux boots `maxcpus=1`; CPU1 stays held = fully safe.

**SMP re-enable later (phase T6), two options — we own the kernel either way:**
- (a) Kernel patch in `omap4_boot_secondary()`: write AUX_CORE_BOOT_1 = secondary
  startup, set boot0 status, clear RSTCTRL bit0 → CPU1 → ROM pen → Linux. (~15 lines.)
- (b) Before the QNX→Linux jump: point AUX_CORE_BOOT_1 at a tiny WFE pen in IRAM that
  polls boot1 for a changed address, release CPU1 into the pen, then jump. Stock-ish
  Linux (memory-mapped auxcoreboot path, as mainline uses on non-secure-accessible
  setups) then just writes boot1 + SEV. No RSTCTRL handling in Linux at all.
- Explicit non-goal: leaving CPU1 running QNX while CPU0 jumps (see 5.3 of the
  earlier revision — it must be dead, penned, or reset-held first).

Note: after any CPU1 warm reset, CPU1 re-enters the ROM pen and will branch to
whatever AUX_CORE_BOOT_1 currently holds (initially QNX's stale 0x8010e0f4) — always
set boot1 BEFORE releasing the reset.

### 5.4 Payload-side pre-jump cache/GIC work
Done in C before entering the stub (steps 6 of §4): `__clear_cache` equivalents via CP15 on all L1, PL310 sync/clean/inv via its registers, then `GICD_CTLR=0`, `GICC_CTLR=0`. (The stub repeats L1/L2 maintenance defensively.)

---

## 6. Device quiesce plan (work item 4)

| Device | Running state at injection | Action | Why / risk |
|---|---|---|---|
| devb-mmcsd-winchester | mounted, 10MB cache, SDMA channel 0 | close our fds, then `slay devb-mmcsd-winchester` | Only DMA engine in the quiesce list that touches arbitrary RAM — must be dead before jump. Order matters: load kernel first, then kill. qnx6 cache loss is irrelevant (we're leaving anyway). |
| splash / boot animation (`sh /proc/boot/splash_script`) + `backlight_win` | display pipeline active | kill processes; optionally blank DISPC via /dev/mem | DISPC's own framebuffer reads are benign (bounded by its own FB), but killing frees RAM and stops surprises. Full DSS driver in Linux is a later phase regardless. |
| TWL6030 PMIC + regulators | configured | leave as-is | Linux twl6030 driver re-inits; powering rails down mid-jump would be worse. |
| i2c×3, spi3 | idle (touch probes periodically) | leave | No DMA into our buffers; Linux re-inits. |
| trustzone-omap4, rpmb-omap4 | idle | leave (do **not** touch /dev/trustzone, /dev/rpmb — hard safety rule) | |
| UART3 (devc-ser1) | console | leave; stub owns the port after jump | Same PA/baud on both sides = seamless handover. |
| GIC | active | disable (5.4) | No IRQ may land during MMU-off window. |
| WDT | 15s armed | keep armed in early phases (§7) | Safety net. |

## 7. Watchdog policy
- **While the jump is unreliable (phases T1–T3): keep it armed.** The stub path from `cpsid if` to kernel entry is microseconds; the 15s budget covers loading + quiesce with wdtkick still alive. A hang anywhere = reset = normal QNX boot. This is the primary safety net and matches the user's "opt-in, nothing persisted" rule.
- **Once jumps are boring**: have the payload `slay omap4430-wdtkick` (or stop kicking) as the final step before the jump so early-kernel hangs don't mask kernel output; manual power-cycle is the recovery. Do this only after T3 succeeds reliably.

## 7b. AS-BUILT implementation notes (2026-08-30, T2 PASS — supersedes parts of §5)

The proven implementation (kexec/qnx2linux.c + stub3.S) differs from the first
draft in ways that matter:

1. **The jump core runs on CPU0, not CPU1.** The original plan entered the stub
   via CPU1's ROM pen; that path is closed (CPU1 warm reset = SAR RAM context
   restore, monitor-verified). Instead the payload thread enters **System mode**
   via `ThreadCtl(_NTO_TCTL_IO_PRIV, 0)` — QNX 6.6's documented privileged-user
   mode — making all CP15 and SMC operations legal from "userland". The flat
   page table trick from §5.2 is unchanged, but the trampoline now lives in the
   executable jump buffer (PROT_EXEC on MAP_ANON|MAP_PHYS — QNX honors it; data
   pages are XN-enforced), and the continuation is written through the NOCACHE
   view so its bytes are in DDR (post-MMU-off fetches must not hit stale L2).
2. **Trampoline detail (stub3.S):** TTBCR=0 (all VAs → TTBR0 — QNX uses TTBR1
   for high VAs; without this the identity walk uses kernel tables), DACR
   all-manager, TTBR0=flat table, TLBIMVA(continuation), bx to the continuation
   at its PHYSICAL address (DRAM sections are identity-mapped; avoids any
   vaddr/phys offset assumptions). Continuation: TLBIALL, SCTLR M/C/I off,
   breadcrumbs, bx r9 to the target.
3. **Breadcrumbs** at 0x9FE00000 (mirror) survive WDT2 warm resets; the primary
   0x9F000000 is churned by the reboot. Written with strongly-ordered (NOCACHE)
   accesses; read back over SSH after reboot. The blob force-holds both cores at
   the end so the WDT2 warm reset fires deterministically (~15 s) — avoiding the
   TWL6030 PMIC watchdog's 127 s power-off, which destroys RAM state.
4. **Ordering rules (see kexec/README.md):** no I/O after the CPU1 hold or GICD
   off; all kernel calls (mmap_device_memory) before IRQs-off; byte pointers for
   raw copies; `on -C 0` pins the payload to CPU0.
5. WDT2 kick = complement WTGR (+0x30); force-reset = hold both CPUs and let it
   time out. TWL6030 PMIC watchdog (i2c0, slave 0x48, reg 0x2C, 127 s) must be
   serviced by the kernel after the jump.

## 8. Target kernel plan (work item 1)

### 8.1 Build
- Vanilla mainline (start at 6.15.x to match pmOS's proven `linux-postmarketos-omap` config baseline; that APKBUILD/config is a gift — reuse its `config-postmarketos-omap.armv7` as the base defconfig).
- `omap2plus` multiplatform, `CONFIG_ARM_APPENDED_DTB=y`, `CONFIG_ARM_ATAG_DTB_COMPAT=y` (harmless), `CONFIG_DEBUG_UNCOMPRESS`/LL_DEBUG on UART3 (OMAP low-level debug port 3) for pre-decompression output.
- Cross toolchain: arm-linux-gnueabihf gcc or clang/lld (pmOS uses LLVM=1) — needs setup on the Linux PC (§9).

### 8.2 Boot entry note
zImage word 0 is `b start`, so branching to the load base works; U-Boot convention enters at base+0x24 (post-header). The stub will target base+0 initially and the T3 experiment validates it (both offsets differ by a branch instruction only).

### 8.3 `omap4-winchester.dts` — phased bring-up
Include `omap443x.dtsi`; phase the nodes:
- **P1 (boot to UART)**: cpu/memory (1GB @0x80000000)/L2/GIC/OMAP4 core defaults, UART3 `&uart3` console, `chosen { stdout-path }`. TWL6030 on I2C1 (bus @0x48072000 per `.script`).
- **P2 (rootfs)**: MMC controller for the eMMC (identify instance + pinctrl from `devb-mmcsd-winchester` RE / TRM; likely MMC1 or MMC2 — TODO), twl6030 regulators (VAUX/VMMC rails per `pmic_twl6030_cfg`/`pmic-winchester-twl6030` RE).
- **P3**: WL1283 (wl12xx platform-data-style node + caldata — needs the MAC/nvs data; the QNX wifi driver may reveal the nvram blob), WM8994 audio, touch (CY8CTMA3 on spi3), sensors on I2C, buttons.
- **P4 (display)**: DSS + the 1024×600 DSI panel — panel identification is an open problem (no known driver); block until P3 works. simplefb continuity from QNX's splash framebuffer is a possible early stopgap (match QNX's FB scanout config into a `simple-framebuffer` node) — evaluate cheaply before writing a panel driver.

Cmdline P1/P2: `console=ttyO2,115200n8 earlyprintk maxcpus=1 ignore_loglevel` (+ `root=` from P2 on).

## 9. Tooling prerequisites (host)
- Cross toolchain: `arm-linux-gnueabihf-` gcc or LLVM/clang+lld; `mkimage` not needed (no uImage), `dtc` needed.
- QNX side: the 6.5/BB NDK already builds `qnx2linux` (plain C, libc only).
- Payload delivery: file on UFS via SSH/SCP (QNX booted) or baked into `.signed` UFS (bb10mt) — both established.

## 10. Test plan (deliverable 4; watchdog = safety net, §7)

| Phase | Unit | Test | Success signal | Failure mode → recovery |
|---|---|---|---|---|
| T0 | 2.1 | `showmem`/`pidin` inventory; memdump v2 reads of GIC/L2C/AUXCOREBOOT addrs | values consistent with TRM/DTS | — (read-only) |
| T1 | 2.0 | env.sh payload runs: blink LED / write UART3 via `/dev/ser1`; scoping pass for TX pad | observable output | none — normal QNX continues |
| T2 | 2.0 | full stub chain, target = **stub loop writing "KEXEC-HELLO" on UART3 forever** | UART text (or LED pattern if blind); watchdog resets after 15s → QNX boots | hang → WDT reset → normal boot |
| T3 | 2.0 | jump to minimal mainline zImage+appended DTB, `CONFIG_DEBUG_UNCOMPRESS` on UART3, `maxcpus=1` | decompressor banner on UART | hang → WDT reset; iterate on entry/state bugs |
| T4 | 2.0 | full kernel to initramfs shell (static busybox, UART console) | shell prompt on UART | as above |
| T5 | 2.0 | eMMC rootfs (P2 DTS: MMC + TWL6030 regulators) | userspace boots from /dev/mmcblk… | as above; fs errors → normal boot unaffected (QNX partitions untouched) |
| T6 | 2.0 | CPU1 bring-up patch, SMP on | 2 CPUs in /proc/cpuinfo | UP fallback via cmdline |
| T7 | 2.0 | kexecboot stage (Option B) if wanted: small kernel+initramfs as the T4 target, then kexec to T5 target | menu + second jump inside Linux | as above |

Safety envelope throughout: no boot-chain writes, no RPMB, no nuke opcodes, flash writes only to UFS via the established bb10mt path; every crash = power-cycle = normal QNX.

## 11. Open questions / TODO
1. `setup-core-inactive` RE — may hand us CPU1 parking for free (5.3.2).
2. PRCM RM_CPU1_CPU1_RST + AUX_CORE_BOOT addresses — TRM lookup + safe on-device verification.
3. PL310 presence/address verify (T0).
4. WDT register base from `omap4430-wdtkick` (for the later disable path).
5. eMMC instance (MMC1 vs MMC2) + pinctrl — from `devb-mmcsd-winchester` RE and/or QNX startup hwi tables.
6. UART3 external pad hunt (T1 scoping) — nice-to-have, no longer blocking.
7. Jump-region base confirmation via `showmem` (3.1).
8. QNX MAP_PHYS allocation granularity/alignment (confirm 2MB+ contiguity is practical; fall back to several MAP_PHYS blocks + memcpy into place if needed).
