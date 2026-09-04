> **STATUS (2026-09-04):** the LIVE command reference is newdocs/COMMANDS.md — this file remains the deep file-inventory/memory-map tables.

# Key Files Reference

## kexec/ (Userspace Payload & Tools)

### Core Payload
| File | Purpose |
|------|---------|
| `qnx2linux.c` | Main payload: `--hello`, `--t2`, `--t3`, `--probe`, `--smcprobe`, plus session-6 modes: `--l2on` (skip the L2 disable — the current jump mode; bc[10] = PL310 CTRL readback), `--l2lat` (PL310 latency probe: reads 0x108/0x10C, attempts the NS write = SIGBUS proof), `--l2test` (the L2-on/off A/B corruption test — **phase C wedges the box on purpose**). All logic in do_hello/do_t2/do_t3/do_smcprobe/do_l2lat/do_l2test. |
| `qnx2linux` | Built binary (ELF ARM). Run: `on -C 0 /tmp/qnx2linux --t3 ...` |
| `build.sh` | Build script — sources qnx-env.sh, assembles stubs, compiles C. **Run after `source ../qnx-env.sh`** |
| `qnx-env.sh` | Sets up QNX SDP cross-toolchain paths (CC, AS, LD, OC). NOTE: it lives in `playbook-dev/` (source with `. ../qnx-env.sh` from kexec/) |

### Assembly Stubs (Built by build.sh)
| Source | Binary | Load Address | Entry | Purpose |
|--------|--------|--------------|-------|---------|
| `hello.S` | `hello.bin` | 0x40305000 | hello_start | T2/hello blob: breadcrumbs + UART loop |
| `stub.S` | `stub.bin` | 0x40304000 | _start | Legacy T2 stub |
| `stub2.S` | `stub2.bin` | 0x40308000 | _start | T2 trampoline (TTBR0 switch → cont) |
| `stub3.S` | `stub3.o` | (linked into qnx2linux) | tramp_pos_start | T3 trampoline (buf+0x800, pre-switch) |
| `probe.S` | `probe.bin` | 0x40309000 | probe_start | Optional IRAM probe for T3 |
| `cacheops.S` | (linked) | — | clean_inval_l1_all | L1 clean+invalidate by set/way |

### Support Tools
| Binary | Source | Purpose |
|--------|--------|---------|
| `memtest` | `memtest.c` | **NEW** — standalone DRAM integrity test (24MB + canary pages) |
| `memdump3` | `memdump3.c` | Read physical memory (bc pages, rings) post-reboot |
| `memw32` | `memw32.c` | Write 32-bit to physical address |
| `scatter` | `scatter.c` | Scatter-test: which pages survive jump+WDT2+reboot |
| `secure-probe` | `secure-probe.c` | Secure monitor probe |
| `i2c-hammer` | `i2c-hammer.c` | I2C stress test |
| `sevtap` | `sevtap.c` | SEV event tap |
| `smctest` | `smctest.c` | SMC call test |
| `xntest`, `xntest2` | `xntest.c`, `xntest2.c` | Misc tests |

### Deployment
| File | Purpose |
|------|---------|
| `jump.sh` | Deploy + detached payload launch + live bc polling + reboot wait + full readback (bc + mirrors + ring1). Usage: `./jump.sh zImage`; `PAYLOAD_MODE=--dmaquiet ./jump.sh zImage` is the session-9 run default (modes: --l2on/--dmaquiet/--t3/--probe/--ppa/--l2lat; default `--probe`) |
| `retry-jump.sh` | Retry harness — **STALE: it loops until mirror0 = 0x47, but the 0x47 (post-SMC) writer was removed; mirror0 = 0x46 (70) is now the normal final value.** Do not use it as-is |
| `rsa`, `rsa.pub` | SSH key for root@169.254.0.1 |

### Kernel Artifacts
| File | Purpose |
|------|---------|
| `kernel/Image` | Uncompressed kernel (architecturally broken for arbitrary load) |
| `kernel/zImage` | Compressed kernel — **correct path** (zImage + the appended DTB, packed by mkkernel.sh) |
| `kernel/omap4-winchester.dtb` | Device tree for PlayBook |

---

## The Pristine Kernel & How to Diff

The kernel tree is a modified vanilla **6.15.11**. The unmodified reference
tree is kept extracted alongside it — use it to answer "what did WE change?"
before blaming upstream code:

| Path | What |
|------|------|
| `/home/psyden/kernel/linux/` | The working tree (built; ~2.5 GB with artifacts; **not** a git repo) |
| `/home/psyden/kernel/pristine/` | Vanilla linux-6.15.11, extracted, unbuilt (~1.7 GB) |
| `/home/psyden/kernel/linux-6.15.11.tar.xz` | The original tarball — re-extract to refresh the pristine tree |
| `/home/psyden/kernel/build.log` | An old kernel build log |

**Rebuild the pristine tree** (e.g. after a kernel version bump):
```bash
rm -rf /home/psyden/kernel/pristine
mkdir -p /home/psyden/kernel/pristine
tar xf /home/psyden/kernel/linux-6.15.11.tar.xz -C /home/psyden/kernel/pristine --strip-components=1
```

**Diff the hand-written changes** (what our tree adds/changes vs upstream).
PlayBook-modified files (the canonical list = the regenerate recipe in
newdocs/COMMANDS.md): `arch/arm/kernel/{head.S,
head-common.S,phys2virt.S,setup.c,early_printk.c}`, `arch/arm/mm/{mmu.c,
init.c,dma-mapping.c}`, `arch/arm/mach-omap2/{omap4-common.c,io.c}`,
`init/main.c`, `kernel/{cgroup/cgroup.c,taskstats.c}`, `mm/slab_common.c`,
`arch/arm/include/debug/omap4bc.S` (new), `arch/arm/boot/dts/ti/omap/
omap4-winchester.dts` (new), `arch/arm/Kconfig.debug`. Recipe:
```bash
P=/home/psyden/kernel/pristine; L=/home/psyden/kernel/linux
# Modified files only (fast):
diff -rq $P/arch/arm $L/arch/arm 2>/dev/null | grep differ
# Full patch of one file:
diff -u $P/arch/arm/kernel/head.S $L/arch/arm/kernel/head.S
# Everything (excludes build artifacts — slow):
diff -rq $P $L --exclude='*.o' --exclude='*.o.cmd' --exclude='*.a' \
  --exclude='*.cmd' --exclude='*.d' --exclude='*.bin' --exclude='*.dtb' \
  --exclude='vmlinux' --exclude='System.map' --exclude='.config' \
  --exclude='include/generated' --exclude='include/config' \
  --exclude='arch/*/include/generated'
```
The kernel build command (from `/home/psyden/kernel/linux`):
```bash
make -j12 ARCH=arm CROSS_COMPILE=/home/psyden/toolchains/armv7-eabihf/bin/arm-linux- zImage
/home/psyden/playbook-dev/kexec/mkkernel.sh zImage   # packs zImage + DTB -> kexec/kernel/zImage
```

### CONFIG recovery + discipline (2026-09-03)
- **CONFIG_IKCONFIG=y is ON** — the full `.config` is baked into every built
  kernel. If `.config` is ever lost/corrupted (a broken `make` syncconfig can
  rewrite it), recover with:
  `scripts/extract-ikconfig arch/arm/boot/Image > .config` (use the newest
  built Image/zImage). This was used to restore the session-6 config after a
  mangled syncconfig dropped DEBUG_LL/DEBUG_PLAYBOOK_BC/ARCH_OMAP2PLUS.
- The forced cmdline (CONFIG_CMDLINE, 102 chars) =
  `console=ttyO2,115200n8 earlyprintk keep_bootcon ignore_loglevel maxcpus=1 root=/dev/mmcblk0p2 rootwait`
- **Never run a bare `make` on this tree without the full CROSS_COMPILE
  prefix** (the toolchain = `/home/psyden/toolchains/armv7-eabihf/bin/arm-linux-`);
  an unprefixed make syncconfigs against the host and can mangle .config.

## Workspace Layout (everything outside docs/)

| Path | What |
|------|------|
| `playbook-dev/kexec/` | **The payload & tools** (the table above) — the daily-work directory |
| `playbook-dev/docs/` | This documentation set |
| `playbook-dev/SESSION-HANDOFF/` | Session records + handoffs; the LATEST bootstrap = `BOOTSTRAP_SESSION_10.md` (the session-10 start prompt) + `ring3-recovered-log-2026-09-02.txt` |
| `playbook-dev/device-binaries/` | **QNX OS binaries dumped from the device, many pre-disassembled** — including `trustzone-omap4` + `trustzone-omap4.dis` (**the secure monitor — ALREADY dumped and disassembled; session 7's RE target**), `procnto.dis`, `libsecure_dispatcher-omap4.so.1` + `.dis`, `devb-mmcsd-winchester` (+`.dis`), `devpm-omap4.so` (+`.dis`), `omap4430-wdtkick` (+`.dis`), `led-fan5702.so`, `winch_lcdctl` (+`.dis`), `setup-core-inactive` (+`.dis`), `splash_script`, `pidin-in.txt` |
| `playbook-dev/dumped4869ifs/` | The dumped QNX 6.6 IFS (boot filesystem) tree: `etc/`, `proc/boot/`, `root/` — the OS's own files/configs |
| `playbook-dev/optimized-docs/` | PlayBook NDK app-dev docs (QNX Neutrino kernel info included) — mostly irrelevant for the port, but the Neutrino sections document the kernel we are evicting |
| `playbook-dev/KEXEC-DESIGN.md`, `KEXEC-PROMPT.md` | Early design docs (historical) |
| `playbook-dev/LED-RE.md`, `PLAYBOOK-REFERENCE.md`, `README.md` | LED/driver RE notes, a device reference, the repo README |
| `playbook-dev/qnx-disasm.sh`, `startup.sh` | The disassembler helper (for the device-binaries dumps), shell startup |
| `playbook-dev/rsa`, `rsa.pub` | SSH keys |
| `playbook-dev/pmaports-main.zip` | postmarketOS omap4 pmaports reference (kernel config hints) |
| `playbook-dev/swpu231ap.pdf` (+`.txt`), `Texas Instruments OMAP 4 ...Wiki.htm.txt` | The OMAP4430 TRM (SWPU231AP) and the pmOS wiki page — the authoritative hardware references |
| `playbook-dev/rootufstest/` | Rootfs/UST test scratch |
| `/home/psyden/toolchains/armv7-eabihf/` | The **Linux** cross-toolchain (kernel builds; `arm-linux-` prefix; the binaries are `arm-buildroot-linux-gnueabihf-*` with `arm-linux-` symlinks) |
| `/home/psyden/qnx660-master/` | The QNX SDP 6.6 install (target headers used by the payload builds) |

---

## Build Commands
```bash
cd /home/psyden/playbook-dev/kexec
source ../qnx-env.sh
./build.sh
# Verifies: hello.bin, stub.bin, qnx2linux, memtest, etc.
```

---

## Deployment & Run Commands

### Standard Kernel Jump (zImage + probe)
```bash
cd /home/psyden/playbook-dev/kexec
./jump.sh zImage
```
Deploys: qnx2linux, probe.bin, memdump3, zImage, omap4-winchester.dtb → /tmp/
Quiesces display stack (slay splash/backlight_win/screen)
Runs: `on -C 0 /tmp/qnx2linux --probe /tmp/zImage /tmp/omap4-winchester.dtb /tmp/probe.bin`
Waits for reboot, reads breadcrumbs (0x90000000) and ring1 (0x88000080)

### Manual Deploy + Run
```bash
# Deploy
scp -i ../rsa qnx2linux probe.bin memdump3 kernel/zImage kernel/omap4-winchester.dtb root@169.254.0.1:/tmp/

# Quiesce display
ssh -i ../rsa root@169.254.0.1 "slay splash 2>/dev/null; slay backlight_win 2>/dev/null; slay screen 2>/dev/null; sleep 1"

# Run (pinned to CPU0)
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/qnx2linux --probe /tmp/zImage /tmp/omap4-winchester.dtb /tmp/probe.bin"

# Wait for reboot (~3 min), then read breadcrumbs
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 90000000 0x24"
```

### Memtest Run (NEW)
```bash
# Deploy
scp -i ../rsa memtest root@169.254.0.1:/tmp/

# Run (pinned to CPU0, 2 rounds default)
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memtest 2"

# Or custom rounds
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memtest 10"
```
**No reboot expected**. If crashes → procnto dies → WDT2 reboot (bc survives).

### Post-Reboot Forensics (2026-09-02 night update)
```bash
# Breadcrumb page — FULL 0x40 dump (bc[15] nonce @+0x3C = fresh-run check!)
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 90000000 0x40"

# Ring1 = THE KERNEL CONSOLE LOG (the primary console evidence!)
# count/index @0x88000080/84, chars @0x88000080+0x80+idx (3840-char window)
# Dump the whole log: memdump3 88000080 <count+0x80> — then reverse each
# 4-byte group (memdump3 prints words BIG-ENDIAN-formatted)
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 88000080 0x540"

# Ring3 = the monitor's UART3 capture — INACTIVE now (the kernel never
# writes UART3; see 01/03). Ring2 (0x90000080) = the same log, second copy.
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 94000080 0x8"

# Mirror0 = the enter_stub "jump started" marker (0x46=70; NO 71 writer —
# 70-without-71 is NORMAL, do not retry on it)
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 94000000 0x8"

# WDT2 live state (CRR @0x28 = remaining, LDR @0x2C = window, TGR @0x30)
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 4a314028 0x8"

# Other mirrors
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 88000080 0x20"
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 9fe00000 0x24"
```

### Readback Interpretation (2026-09-02 night)
| Field | Meaning |
|-------|---------|
| bc[0]/bc[5] | magic / magic2 (0x4C424B43 / 0x4C424B44) |
| bc[1] | last flushed marker (historical: 127 = the session-7 wall; 133 = console registered; 132 = the old console-write wedge; 41 = jump-chain; 0xAB = abort). **Session-9 ladder: see docs/README's decision tree (the marker map + the setup.c collision rule)** |
| bc[2] | v\|0x100 (the pb_bc pair — should match bc[1]); a junk value here = QNX-boot leftover, NOT a kernel wild write |
| bc[3] | 0x41 = the kernel's ACTLR dump (start_kernel ran); otherwise the payload staging phys |
| bc[6] | 0x2102 = --l2on kept the L2 on (may be overwritten by the stub's register echo) / initcall level |
| bc[7] | initcall fn pointer — resolve via System.map (STALE probe cache-id = 0x410000C4 if never reached) |
| bc[10] | payload: PL310 CTRL readback (1 = L2 ON in --l2on mode); kernel: 0xC0DE0001/2 = the console registered/writing |
| bc[11] | 0xC0DE0010 = parse_early_param entered; 0xC0DE0030 = the done-latch was already closed (would be a bug) |
| bc[12] | 0xC0DE0020 = parse_early_options returned (the console handler completed) |
| bc[13] | cmdline length (102 = the forced CONFIG_CMDLINE) |
| bc[14] | 0xC0DE0030 = the latch-closed probe fired |
| bc[15] | run nonce = time(NULL)^phys — MUST differ between runs |
| mirror0 +4 | 0x46 (70) = jump started (the last payload write — normal) |

---

## SSH Connection Details
- **Host**: 169.254.0.1 (RNDIS over USB)
- **User**: root
- **Key**: `../rsa` (relative to kexec/)
- **Options**: `-o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o MACs=+hmac-sha1 -o ConnectTimeout=8`

---

## Memory Map Constants (from qnx2linux.c)

```c
#define BC_ADDR        0x90000000ULL   // Primary breadcrumbs
#define BC_MAGIC       0x4c424b43u     // 'CBKL'
#define BC_MAGIC2      0x4c424b44u     // 'CBKM'
#define IRAM_TABLE     0x40304000ULL   // 16KB flat L1 table
#define IRAM_STUB      0x40308000ULL   // Continuation (identity)
#define IRAM_MAXCODE   0x1000ULL
#define RSTCTRL_CPU0   0x4824340cULL
#define RSTCTRL_CPU1   0x4824380cULL
#define AUX_BOOT       0x48281800ULL
#define GICD_CTLR      0x48241000ULL
#define QNX_STARTUP1   0x8010e0f4u

// Breadcrumb mirrors (survivor band)
#define BC_MIRRORS     { 0x94000000, 0x88000000, 0x9FE00000 }

// WDT2
#define WDT2_BASE      0x4A314000ULL
#define WTGR_OFFSET    0x30

// T3
#define T3_BUF_SIZE    0x1800000u      // 24MB
#define ZIMG_PAD_MAX   0x1000u
```

---

## Debugging Commands (On Device)

### Check ACTLR / Diagnostic Register
```bash
# Requires System mode (ThreadCtl IO_PRIV)
# In qnx2linux --smcprobe or custom tool:
mrc p15, 0, r0, c1, c0, 1   @ ACTLR
mrc p15, 0, r1, c15, c0, 1  @ Diagnostic
```

### PL310 Registers
```
0x48242000  Cache ID
0x482427FC  Clean + Invalidate by Way
0x48242730  Cache Sync
0x48242768  Clean + Invalidate by PA
```

### GICD
```
0x48241000  CTLR (write 0 to disable)
```

### Reset Control
```
0x4824340C  CPU0
0x4824380C  CPU1 (write 1 = hold in warm reset)
```

### WDT2 (2026-09-02 register truth — from drivers/watchdog/omap_wdt.h)
```
0x4A314024  WCLR (prescaler)
0x4A314028  WCRR (current counter, down-counting)
0x4A31402C  WLDR (load — 0xFFE2B400 = 58.6 s @ 32.768 kHz, PTV=0)
0x4A314030  WTGR (TRIGGER — ANY write reloads CRR from WLDR; value ignored)
0x4A314048  WSPR (start/stop service: 0xBBBB then 0x4444 = start)
```
**Kick = any write to 0x4A314030.** WSPR is NOT at 0x30.

### UART3 (Debug Console — 2026-09-02 correction)
```
0x48020000  Base (console=ttyO2, 115200 8N1, 48 MHz — per the IFS devc-seromap
            line and omap4bc.S; the old 0x4806A000 was wrong)
```
**UART3 = DEAD post-idle (L4PER auto-idle)** — the kernel must never write it
(a posted store to the dead THR stalls the store buffer; a dead-target read
stalls forever). senduart/LSR-drain are REMOVED from omap4bc.S. The console =
the DRAM rings (ring1 = primary). The monitor's UART3 capture (ring3) is
therefore INACTIVE. The pad hunt is DEAD (2 h, nothing accessible).

### PRCM (CM1/CM2 — SECURE-FILTERED from NS!)
```
0x4A004000  CM1 (MPU inst +0x300 → MPU CLKSTCTRL @ 0x4A004300)
0x4A008000  CM2 (CORE inst +0x700: L3_1 @ 0x4A008700, L3_2 @ 0x4A008800,
            SDMA @ 0x4A008A00, MEMIF/EMIF @ 0x4A008B00, D2D @ 0x4A008C00,
            L4CFG @ 0x4A008D00, L3INSTR @ 0x4A008E00; L3INIT inst +0x1300,
            L4PER inst +0x1400, ALWAYS_ON inst +0x600)
```
CLKSTCTRL CLKTRCTRL bits[1:0]: 0=HW_AUTO, 1=SW_SLEEP, 2=SW_WKUP. **Writes
from NS = SIGSEGV (run 31)** — the monitor's PPA clock-domain service is the
only path.

---

## Common Issues & Fixes

| Symptom | Cause | Fix |
|---------|-------|-----|
| "random death points" (bc 102/107/115/119) | WDT2 timeout during setup | Early + late WDT2 kicks |
| Silent payload death mid-setup | jump.sh 30s SSH timeout → SIGHUP | Timeout 120s |
| Earlyprintk abort before paging_init | UART/PL310 VAs unmapped | Rebase debug VAs to section-compatible |
| bc readback garbage | Missing DSB before PL310 CIPA | DSB in all pbmark macros |
| DTB magic corruption (0xedfe0dd0 → 0xadfe0dd0) | DRAM bit flip (bit30) | Memtest → blacklist or swap unit |
| Payload crash → device reboot | procnto dies → WDT2 | Expected; bc pages survive |
| Buffer placement fails repeatedly | Allocator returns same bad block | Leak rejected buffers (don't munmap) |