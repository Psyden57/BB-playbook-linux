# BlackBerry PlayBook — Research Reference
Consolidated knowledge base. Everything below is derived from hands-on device access, binary analysis (Ghidra/capstone), and cross-referencing tool source (bb10mt, deckard-adjacent python). Items marked **[!]** are inferred but unverified. Last updated: Aug 28, 2026 (post device-testing round: CREAD reinterpreted §4.6, write-path signature condition §4.7, cfp ground truth §1.3-1.4; current focus = custom OS on a working unit — see SESSION-HANDOFF.md "CURRENT FOCUS").

---

## 1. The device

| | |
|---|---|
| Product | BlackBerry PlayBook (RIM "Winchester" board family; "win2"/winchester2 = the **PlayBook 4G variant**, OMAP4460). Note: "Colt" in the startup sources is **not** a PlayBook revision — it was a separate cancelled RIM smartphone whose board-support code survives as leftovers (e.g. "Unable to determine the revision of a Colt board") |
| SoC | TI OMAP4430 (OMAP4460 on winchester2/4G), dual Cortex-A9 |
| Hardware ID | `0x06001A06` (64GB P100-16WF); siblings `0x0C/0D/0E001A06` (P150 = 4G variants). Vendor ID `0x1F8` |
| Security type | **HS (High Security)** — confirmed three independent ways |
| My units | 2× working 64GB (one on OS 2.1.0.1917), 2× bricked 32GB (see §1.3) |

### 1.1 HS status (why this matters)
- OMAP `CONTROL_STATUS` register @ `0x4A0022C4` reads `0x00000AE8` → DEVTYPE[10:8] = **2 = HS**. (Cross-checked: Droid BIONIC, also OMAP4430+HS, reads `0x00000AEF` — same DEVTYPE, low bits are SYS_BOOT strapping.) TI mapping: 0=TEST, 1=EMU, 2=HS, 3=GP.
- The boot chain's own strings say `"OMAP4430 HS"`.
- The NVRAM driver's `is_secure_cpu` (reads the bootrom-metrics security field, `0`=HS / `1`=GP) reports **secure**.
- **"GP" on the package silkscreen is not contradictory**: DEVTYP is an eFuse RIM blows in production; the marking reflects pre-fusion silicon class.
- Consequence: the boot ROM **cryptographically verifies** the first-stage loader. Replacing the ramloader with our own via USB peripheral boot is **not possible**. Everything below works *with* the signed chain.

### 1.2 Known unit failure states (the two bricked 32GB)
1. **"NVRAM signature error" unit** — the whole NVRAM was wiped during an experiment; the signed records are gone, so the chain refuses to continue ("NVRAM signature error"). A **full backup of the NVRAM taken before the wipe exists** → recovery = writing the backup image back over the NVRAM window (§8, flash surgery via CREAD/EE/F7 — restoring a complete image is simpler than surgical record editing). Note the wipe also destroyed the OS blocklist, so no downgrade lock on this unit.
2. **"RPMB error" unit** — an experiment set the device toward **factory mode** (done by adding a file to the RPMB region) but overlooked the **factory boot counter**, which has since "expired". This is the harder problem: RPMB uses a monotonic write counter (can only increment, never roll back), so a counter-based expiry can't simply be "written back". Needs research into how the factory boot counter is stored/checked (RPMB-omap4 driver, trustzone-omap4, and the loader's RPMB code — "Fail_to_write_RPMB" strings in the ramloader) before any recovery attempt.
- History note: the blocklist-downgrade bricking scenario = a BB10 OS (e.g. 10.1.x.x) that doesn't fully run on the PlayBook is flashed; it boots partially and its updater writes a 10.1.x.x floor into 0x2819, blocking downgrade to any working 2.x/10.0.x OS. The OS never writes *itself* into the blocklist — the floor is whatever the last-booted updater set (each OS sets its own floor, e.g. 2.1.0.1917's updater uses 2.1.0.1281).
- Radio blocklist (0x2852) is only relevant to the 4G winchester2 variant; my units are WiFi-only (P100).

### 1.3 cfp info — device ground truth
`cfp.exe info` (RIM programmer, works on Windows via RIM drivers — libusb CANNOT see the device there; use Linux for bbcread/creadtest):
```
Bootrom Version 5.27.0.20 · Security: Enabled (HS reconfirmed)
Hardware ID 0x06001A06 · BR ID 0xFF000000 · Metrics Version 6.24
Build ec_agent Mar 24 2011 15:31:02 (matches bootrom-metrics blob)
LDR Blocks: 0x80014674  ← matches the pointer in our bmetrics dump @+0x90
Bootrom Size 0x00020000 · Boot Mode: Product · Boot OS Count 5,0,5 / 2,0,0
Boot Count Info: NVRAM Signature Invalid   ← Unit A (wiped NVRAM) [confirm unit]
OS Version 2.1.0.1917 DEV (OS metrics ec_agent Mar 19 2014 04:05:03 = hd1@0x10002C ✓)
eMMC: SanDisk SEM32G, Boot0 1MB / Boot1 1MB / User ~30GB, 1024MB DRAM (Elpida)
Mem Config Table (ver 1.24):
  MCT block 0 · MFG Data blocks 1-4 · Bootrom blocks 5-12
  MBR 2053-2068 · OS Fixed 2069-18436 · OS NV 18437-18500 (64) · Branding 18501-18692
  FS Fixed 18693-524287 · QNX region 0: 2069-10244 · QNX region 1: 10261-18436
  NAND blocks 484869 × 64KB · RAM 0x80000000-0xBFFFFFFF (2 banks × 512MB; cfp reports 1024MB DRAM)
```
Notes: "OS NV" (64 blocks) ≈ the NVRAM window size; nvram_resource_manager maps `0x48000000..0x483FFFFF` = 64KB-blocks 18432-18463 — 5 blocks off from the MCT "OS NV" numbers; MCT block units look inconsistent per row, so **calibrate empirically**: CREAD-dump `0x48000000` and match it against `nvram-backup/nvram0.bin` (records at window+0x10000), or scan the dump for 'NVRE' to find the true delta.
"Boot Count Info: NVRAM Signature Invalid" is the loader/bootrom reporting exactly the Unit A damage — a clean diagnostic channel for verifying the restore later (should read valid again after restore).
cfp also uploads its own RAM image at 0x80100000 (end 0x80130230) — same ramloader family, same base as ours.

### 1.4 cfp flashinfo — eMMC ground truth (Unit A)
```
ExtCSD: SECTOR_COUNT 0x3B30000 (30.3GB) · BOOT_SIZE_MULT 0x8 (1MB boot0/boot1)
HC_ERASE_GRP_SIZE 0x4 → 4MB erase groups (the 4MB NVRAM window = exactly ONE erase group!)
CACHE 0 · HS_TIMING 1 · CARD_TYPE 0x7 · SEC_FEATURE 0x15 · SNDK health ok
Write-protect table:
  0-0 Boot0 None · 1-4 Boot1 None
  5-1028 User PERMANENT   ← first ~256MB of user area locked at factory
  1029-484869 User None
```
Implications: the factory-locked region covers MCT/MFG data/Bootrom metrics/MBR (MCT blocks 5-2052 = "MMC Enhanced") — explains why the boot chain can't be tampered with below the OS level. **The NVRAM window (block 18432 = 0x48000000) is NOT write-protected** → CREAD/$EE restore path is viable. The 4MB NVRAM window = one full eMMC erase group → the restore is: erase the group, write the full 4MB image (`nvram_restore_*.bin`), verify.

### 1.5 OMAP registers / memory of interest
- Boot ROM candidates: `0x00020000` (128KB) / `0x00028000` — **unverified; a plain-mmap read attempt at 0x20000 crashed the device** (see §9.3 for the safe method).
- OCMC SRAM @ `0x40300000` (boot-time remnants).
- x-loader region seen in bootrom metrics: pointer `0x80014674`, i.e. SDRAM base `0x80000000` is where the chain runs; the USB ramloader loads at **`0x80100000`** (PlayBook family; BB10 handsets use `0x80200000`).

### 1.6 cfp info — 64GB unit @ 2.0.0.4869 (the kexec test unit; captured 2026-08-29)
```
PIN [REDACTED] · BSN [REDACTED] · Bootrom 5.27.0.20 · Security Enabled · BR ID 0xFF000000
Hardware ID 0x06001A06 · Boot Mode: Product · Boot OS Count 1,0,0
LDR Blocks 0x80014674 · Bootrom Size 0x20000
HWV: BoardRev 0x01 · CPU Version 0x74 · POP Security 0x02 (HS) · Power Mgt HW 0x3E
eMMC: SAMSUNG MCGAFA (rev 0x10, mfg 2011-03), 60832MB total
  ExtCSD: SECTOR_COUNT 0x76D0000 · BOOT_SIZE_MULT 0x4 (512KB boot parts) · CARD_TYPE 0x7
  HC_ERASE_GRP_SIZE 0x1 (512KB groups) · HC_WP_GRP_SIZE 0x20 · SEC_FEATURE 0x15 · HS_TIMING 1
  Write-protect: Boot0/1 None · User blocks 5-260 PERMANENT · 261-973317 None
    (← vs the 32GB unit's 5-1028; on THIS unit only the first ~16MB is factory-locked)
RAM: 1024MB Elpida (rev 0x100), two banks: 0x80000000-0x9FFFFFFF + 0xA0000000-BFFFFFFF
IRAM Base: 0x40304000 (confirms the kexec stub location)
OS Version 2.0.0.4869 DEV (metrics 3.18, build "cfp Nov 15 2011 14:51:36")
  OS Address: 0x800FF800-0x803BF60F  ← where the OS image is expected in DRAM
```

### 1.8 Live register map — harvested via `pidin mem` of running drivers (2026-08-30, SSH; safe/read-only)
Physical bases each driver maps through /dev/mem (confirmed live + clocked by construction):
- **omap4430-wdtkick**: **0x4A314000** = OMAP4 **WDT2** (matches mainline omap4.dtsi `wdt2@4a314000`). Also opens /dev/i2c0 → it kicks the **TWL6030 PMIC watchdog too** (strings: "Invalid time for pmic watchdog timeout", "PMIC I2C device open failed"). Both watchdogs must be handled at jump time (OMAP WDT2 regs or keep wdtkick alive; TWL6030 WDT via I2C1/pmic driver — note mainline has a twl6030 watchdog). wdtkick has a "WDT is disabled now" exit path — killing it cleanly may disarm WDT2.
- **winchester-lc (LED)**: maps 0x4A310000 (odd: WDT1 region — likely LED heartbeat via WDT1 GPIO/pulse). LED hw on Rev:07 = **FAN5702** I2C LED controller (`led-fan5702.so`, vs led-gpio.so on Rev:00-02).
- **devb-mmcsd-winchester**: **0x480B4000 = MMC2 → the eMMC is on MMC2**; also 0x4A307000 (PRM), 0x4A009000, 0xBF0E1000 (?).
- **spi-master (touch)**: 0x480BA000 = **MCSPI3**.
- **rpmb-omap4**: 0x4A004000 (secure dispatcher/SCM), 0x4A30C000.
- **powerman (cpufreq)** maps nearly everything incl. **0x48240000 (16K → GICD/GICC/PL310 → PL310 @ 0x48242000 confirmed live)**, **0x4A326000 (PRM_MPU — CPU1 reset domain, live)**, full control module 0x4A000000+ and CM2 0x4A100000, DSS 0x58000000-family.
- **screen**: 0x58006000 (DSI2 proto engine region), 0x4A100000 (CM2), 0x48059000, PRM; big RAM mappings = framebuffers.
- **io-usb-dcd**: 0x4A0AB000 = MUSB OTG (matches its args).
- Post-boot free RAM: **582MB/1024MB free (137 procs)** — at env.sh/runlevel-0 time free RAM will be far higher (~950MB), so a 0x84000000 jump region is safely clear of QNX allocations; confirm at injection time.
- `pidin info`: Processor1+2 listed (both cores used by procnto).

### 1.9 SSH access to the 2.0 unit (2026-08-30)
`ssh -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o MACs=+hmac-sha1 -i rsa -l root 169.254.0.1` (USB RNDIS; key `rsa` in workspace; bozohttpd also serves 169.254.0.1:80/443). Root via the UFS payload chain (rootufstest/): env.sh (runlevel 0, inside filesystem_common) → background waiter → post-boot.sh (pf firewall off, devmode true, sshd AllowUsers root via /radio/scripts/ssh.sh). `launcher-insecure` (ELF) replaces launcher-secure via /base/sbin/launcher existence check in launcher_start(). Note: their env.sh also neuters check_sanity (returns 0).


### 1.10 kexec session findings — PRCM/monitor/WDT (2026-08-30, live reads + RE)
Safe per-address reads via `kexec/memdump3.c` (mmap_device_memory PROT_NOCACHE; a bad read = **SIGBUS contained to the process**, kernel survives — see CM2 test; only cacheable mappings kill the kernel).
**Corrected PRCM base map** (from trustzone-omap4 + setup-core-inactive RE, cross-checked with live reads):
- CM1/CM_MPU = **0x4A004000** (rev reg 0x40000100 read live ✓). trustzone-omap4 maps 0x4A004300 (0x100) and toggles **0x4A004304 |= 0x4000** around every SMC (CPU clock-gating control). 0x4A004308 reads 0x04000038 (CPU1 clkctrl-ish).
- CM2 = **0x4A009400** (rev 0x00040102 ✓; trustzone maps 0x4A009000 and flips CLKTRCTRL=SW_WKUP on 0x...5A0/5A8/5B0/5B8/5C0 around SMCs). The earlier SIGBUS at 0x4A007000 was a WRONG ADDRESS, not a firewall.
- PRM = 0x4A306000 (rev 0x40000100 ✓). PRM_MPU = 0x4A326000: **firewalled from NS except window ~0x4A326A00–0x4A326CD0** which contains secure-monitor code+data (dsb.sy @0x4A326C00, BTB-inval mcr, cmp/beq pen code, self-pointer @0x4A326A04 → 0x4A326B00). ROM AuxCoreBoot @0x4A326FCC/FD0 = garbage (firewalled). CTRL-module **AUX_CORE_BOOT0/1 @0x4A002E08/0C read 0/0** — QNX did NOT boot CPU1 through the ROM pen (Linux-style pen state absent).
- **/dev/trustzone raw SMC passthrough** (trustzone-omap4, root-only devctl):
  `dcmd 0xC0280501`, 40-byte payload `{u32 result_out; u32 fnid; u32 a1; u32 a2; u32 n; u32 w[4]; u32 pad[2]}` → SMC #1 with r0=fnid, r1=a1, r2=a2, r3=phys of driver-owned param block {n, w[n]}. libsecure_dispatcher uses it for services **0x12** (HWRNG), **0x2E** (HMAC), **0x2F** (AES), **0x31/0x32/0x33/0x37** (KDF/KEK/DEK). TI-standard monitor services likely present in 0x00–0x2D range (mainline omap-secure.h: 0x21 SETUP_CLOCK_DOMAIN, 0x22 SETUP_POWER_DOMAIN, 0x102/0x103 L2CACHE services) — the sanctioned NS→secure PRCM write path if RIM kept TI's monitor API. Probe carefully, known services first.
- **WDT2 @0x4A314000** (wdtkick RE): kick = **complement WTGR @+0x30** each tick (never writes WCRR). Exit path only *reads* WSPR @+0x48 and prints "WDT is disabled now" if WSPR != 0x4444 — **nothing in userland ever disables WDT2**. Live read: WSPR=0x4444 ⇒ RUNNING (TI OMAP4 enable seq 0xBBBB→0x4444; disable = 0xDDDD→0x0000, each write followed by WWPS@+0x34 pending-bit clear). The kexec payload must service or disable WDT2 itself. **[ERRATUM 2026-09-03: the 0xDDDD→0x0000 disable sequence was a session-1 guess with no source — the kernel's omap_wdt.c (session 7) gives enable 0xBBBB→0x4444 / disable 0xAAAA→0x5555 / kick = TGR complement; see newdocs/contradictions/session-01-probe-errata.md #2.]**
- **TWL6030 PMIC watchdog** (wdtkick `-P`): devctlv(/dev/i2c0, 0x80100505), slave **0x48**, write reg **0x2C = 0x7F** (127s window) per tick. Linux side must kick/disarm reg 0x2C too (mainline twl6030 watchdog driver covers this).
- setup-core-inactive = TI PRCM HAL port (DPLL/VC/SR/OPP init): **no** AUX_CORE_BOOT refs, no WFE/SEV, no PRM_MPU writes, no SMC. QNX's CPU1 bring-up lives in procnto/startup — no reusable parking state found there.
- **TRM swpu231ap.pdf mined (2026-08-30, /tmp/opencode/trm.txt) — CPU1 mechanism SOLVED**:
  - **AUX_CORE_BOOT_0/1 = 0x48281800/04** (WUGEN instance CORTEXA9_WUGEN, base 0x48281000 — NOT 0x4A002E08). Live: boot0=0x2, boot1=0x8010e0f4 (QNX's secondary startup ⇒ QNX boots CPU1 via ROM pen).
  - **WKG_CONTROL_0/1 = 0x48281000/0x48281400**: read-only per-CPU status; bit8=STANDBYWFI, bit9=STANDBYWFE, bit15=DOMAIN_RST etc. Live: 0x700 both cores.
  - **RM_PDA_CPU0/1_RSTCTRL = 0x4824340C/0x4824380C** (MPU local PRCM, inside powerman's live 0x48240000 16K map): bit0 RST = per-CPU software warm reset, **held until cleared by the other CPU** (TRM Table 4-30: "One CPU can set this bit to reset the other CPU"). Live: 0.
  - RM_PDA_CPUi_CONTEXT 0x48243408/0x48243808 (context-lost flags, W1toClr); CM_PDA_CPUi_CLKCTRL 0x48243414/0x48243814; PRM_PSCON_COUNT 0x48243204 (power chains).
  - TRM §27.5 documents the ROM monitor services (SMC #1, **r12 = function id**): 0x100-0x113 L2/TLB/SCU, 0x103/104/105 auxcoreboot ("kept for ES1.0 compat — registers are memory-mapped"), 0x106/0x107 WKG_CONTROL read/clear, 0x21+ PPA. RIM's monitor via the raw devctl uses **r0 = service id** (empirical: 0x31 driver-shape → OK; 0x103/104/105 → FAIL). CPU1 control does NOT need the monitor.
  - Monitor only touches memory inside its registered secure window (raw-SMC RNG with external MAP_PHYS dest: OK result, no write).


### 1.7 IFS boot script (`proc/boot/.script`, OS 2.0.0.4869 — dump in `dumped4869ifs/`)
Exact boot order with driver arguments (dev-order matters for the kexec quiesce list):
1. `setup-core-inactive` (SETUP-CORE-INACTIVE)
2. `resource_seed dma=0,31`
3. **`devc-seromap -o devperm=0600 -e -F -b115200 -c48000000/16 0x48020000` → /dev/ser1**
   ← **Debug UART = OMAP4 UART3 @ 0x48020000, UART clock 48MHz (÷16), 115200 baud.**
   Matches the bootrom's "Starting RIM bootrom with UART_DEBUG enabled". Linux side:
   `console=ttyO2,115200n8` + `earlyprintk` — the debug lifeline we thought we lacked.
4. `slogger`, `pipe`
5. `trustzone-omap4 -U99:102` → /dev/trustzone
6. i2c-omap35xx-omap4: i2c0 @0x48070000 irq88 · i2c1 @0x48072000 irq89 · i2c3 @0x48350000 irq94
7. **`omap4430-wdtkick -t 15000 -P`** ← watchdog kick period = **15s** (not 10s)
8. `pmic_twl6030_cfg` · `pmic-winchester-twl6030` (→/dev/pmic/regulator) · `pmic-twl6030-ctl_script` (TWL6030 confirmed as the PMIC, matches teardowns)
9. `random -p -l entropy.so:10:300000:256`
10. **`devb-mmcsd-winchester blk cache=10M,noatime,hookdll=vfs-hooks-rim.so cam quiet cache qnx6 crypto=enable`**
11. `spi-master -u3 -d omap4430 base=0x480ba100,irq=80,sdma=0` → /dev/spi3 (touch panel controller bus)
12. `sh /proc/boot/splash_script` (boot animation), hd3 mount, `backlight_win`
13. `rpmb-omap4` → /dev/rpmb · `nvram_resource_manager -U 99:0 /dev/hd0 0x48000000 0x483FFFFF` · `rfs_validator -c`
14. `sanityChecking test /dev/hd1 /dev/hd2 256 4 /proc/boot/sanity 99 /proc/boot/sanity.cfg` (OS-slot sanity)
15. **`ksh /base/scripts/startup.sh`** ← the env.sh injection point runs here; by this
    time devb, i2c, pmic, spi, UART, watchdog-kick, backlight and splash are all live.

---

## 2. Boot chain (as found on eMMC, dumped and verified)

Everything below was read out of the actual device (see §9). `/dev/hd0` LBA 0 is the RIM boot chain, *not* an MBR:

```
/dev/hd0 (raw eMMC user area)
0x000   flash dir: u32 size=0x4000, magic 0x00004D70 ("Mp"), name "MLO",
        {0xC00, 0x32B8}, name "PRIMAPP"
0x040   PRIMAPP / MLO          — TI x-loader equivalent (~16KB)
0x4000  "CertISW" (Certified Initial Software) + OMAP4430 Bootstrap
0x12488 strings "RIM-BootNUKE" / "RIM-BootLoader"; "Exceeded boot count (%d >= %d)"
0x148F8 loader tail cookie 0xD7C82D1F (image ends 0x14900)
0x14900 "RIM bootrom" stage (Thumb entry):
        "Starting RIM bootrom with UART_DEBUG enabled", "OMAP4430 HS",
        "BOOT_MODE: %s", "BootModeCount expended", "SW_RESTORE support enabled",
        "Leaving bootrom and entering OS"
0x1D044 bootrom-metrics blob (flash form, 0x2DC bytes):
        {0x00060018, 0x2DC, 0x051B0014, 0x06001A06}, "RIM BlackBerry Device",
        build host "ec_agent", "Mar 24 2011 15:31:02"
```

RIM cookie family seen on flash: `0xD7C82D1F` (image tail), `0xD7D32D1F` (USB loader image header), `0xD7B02D1F` (bootrom-metrics header, not found in the dumped heads — likely on the eMMC boot hw partitions hd4/hd5/hd6), `0xD7A82D1F` (OS-slot metrics variant).

### 2.1 OS slots
`/dev/hd1` and `/dev/hd2` are the two full OS slots (qnx6 partitions; identical content up to `0xF0000`, then diverge — two slot revisions):

```
0x000    QNX IPL MBR: "QNX v1.2b Boot Loader" (EB 10 90 … 55AA)
…        IPL / boot-region code
0xFFDD0  "QNXH-OS-1" block: 'QNXH' + u32 0x88 + hash data   ← signature trailer
0xFFE8C  "QNXL-OS-1" block
0xFFF48  "QNX-OS-1"  block: 'QNX' + u32 0x80 + hash/sig
0xFFFF8  tail cookie 0xD7C82D1F + FFFFFFFF
0x100000 ← signature-covered boot region ends exactly at 1MB
0x10002C OS metrics blob (0x1CC bytes; driver validates magic 0xC3D0E193 @+0x9C,
         OS type u32 @+0xD4; starts 0xD7A82D1F, "ec_agent Mar 19 2014 04:05:03")
0x12A000 IFS: imagefs/rifsboot tables, startup/boards/playbook/hwi_omap4430.c,
         "Loading IFS...", "INVALID IFS signature" / "FOUND valid IFS signature",
         "Restore IFS2 ... RIFS2 info", bootrom-metrics read strings
```

The **"QNXH"-family trailer blocks** at the end of the 1MB boot region are what gets transmitted as the 560-byte `SIGNATURE_TRAILER` when flashing (§4.5). The **IFS signature checker lives inside the IFS itself** (rifsboot/startup module) — the code that decides "INVALID IFS signature" vs "FOUND valid IFS signature" is QNX code we already have on disk.

### 2.2 The IFS (initial filesystem)
A tiny filesystem embedded in the OS slot containing the boot-time executables and drivers (extracted copy in `dumpedifs/`):
`procnto-smp-instr` (kernel), `devb-mmcsd-winchester` (eMMC), `devc-seromap`, `i2c-omap35xx-omap4`, `spi-master`/`spi-omap4430.so`, `fs-qnx6.so`, `io-blk.so`, `fs-dos/udf/cd.so`, `libc.so.3`, `nvram_resource_manager`, `rfs_validator` (rcfs/ifs validator), `rpmb-omap4`, `trustzone-omap4`, `libsecure_dispatcher-omap4.so`, `bmetrics`/`bmetrics_cli`, `winchester-bootreason`, `pmic-twl6030-*`, `random`, `entropy.so`, `ksh`, plus splash bmps. Board codename "winchester" throughout.

---

## 3. The OS (PlayBook OS 2.1.0.1917)

- QNX Neutrino 6.6. `scmbundle::2.1.0.1917` (also scmbundle0/1). OS 10.0.9.388 (BB10) also boots on the hardware and can be rooted the same way.
- Root achieved post-boot via an Android-runtime oversight; with root, `/dev/*` block devices are wide open.
- Partition/mount map (from the live device):

```
/dev/hd0            raw eMMC user area (boot chain @ LBA0, NVRAM @ 0x48000000)
/dev/hd1, /dev/hd2  OS slots; /dev/hd1t179 = qnx6 sub-partition → /base
/dev/hd3            root fs (qnx6) mounted on /
/accounts/devuser/fs1.qnx6  file-backed qnx6 image mounted on /q (user data)
```
- Root fs layout: `/base` (real files; `/bin`,`/lib`,`/usr`,… are symlinks into it), `/apps`, `/accounts/1000/…`, PPS objects under `/pps`, `.boot`/`.seed` dirs, `.rootfs.os.version`.
- PPS `/pps` device properties (security-relevant):
  `hardwareid::0x06001a06`, `drmhwfp::0x171418BB…`, `fingerprint::FWe7mi_NL6-…`,
  `vendorid::0x1f8`, `scmbundle::2.1.0.1917`.
- `libnvram.so.1` lives at **`/lib/libnvram.so.1`** on 2.x (BB10 keeps it in `/proc/boot`; the two builds are different — the 2.x one is 5,740 bytes and exports only the core nv_* API + `GetBootromMetrics`).
- QNX userland quirks: no `od`/`head`, `dd` rejects `1M`-style suffixes (use numeric `bs=`), no `ddev`. `pidin arg` (not `args`).

---

## 4. How flashing works

### 4.1 Image format (.signed / autoloader)
An autoloader `.bin` = encrypted, RIM-signed container. Unpacked with bb10mt into parts (syntax `name=offset,type`):
```
PlayBook.mbr =8192,2     ← MBR, type 2 (RAID-ish marker), at offset 8192
PlayBook.rcfs =192,0     ← base OS filesystem (read-only), signature-checked at boot
PlayBook.mbr =0,0        ← (second mbr entry, type 0)
PlayBook.sig =15,0       ← signature part
PlayBook.ifs =16,0       ← boot IFS (procnto + drivers), signature-checked
PlayBook.ufs =16640,0    ← user filesystem (modifiable)
```
bb10mt (`bb10mt-main/`, Pascal/libusb) can unpack/repack these, mount rcfs/ufs for editing, **and act as the host-side USB flashing client**. Only `.ufs` survives modification today; `.rcfs` and `.ifs` are verified (IFS by rifsboot in-IFS, rcfs by `rfs_validator`).

### 4.2 USB modes
The device enumerates with vendor id `0x0FCA` and product ids that identify the stage:

| USB PID | stage |
|---|---|
| `0x0001` | TI boot ROM (peripheral boot) |
| `0x8001` | loader running (RIM ramloader / "RIM-BootLoader") |
| other | running OS / reinit ("RIM REINIT", "RIM UPL", "RIM-BootNUKE" are mode strings) |

### 4.3 Bootrom protocol (USB PID 1)
Simple command channel (from bbusb.pas):
```
$F000 PING   $F001 READ_METRICS   $F002 EXIT       $F003 FRESHNESS_SEAL
$F004 WRITE_RAM                   $F005 EXECUTE_RAM
$F006 PASSWORD (challenge/salt/iterations — device may demand auth)
$F007 CHANGE_BPS   $F008 DEVICE_NUKE   $F009 WRITE_RAM_SETUP   $F00A WRITE_RAM_VERIFY
$F00B READ_MODEL_CODE
```
Flashing flow: wait for PID 1 → `Ping0` → `GetVar(2)` = bootrom metrics (TBRMetrics: modelID, loadAddr@+8, BuildUser/Date/Time, HWOSId, BRId) → validate a signed loader file for the model (header `0xD7D32D1F` @+4, tail `0xD7C82D1F` @len-8, nonzero signature region) → `SetMode(1)` → `PasswordInfo` (challenge/response; may be no-op) → `SwitchChannel` → `GetMetrics` → `SendLoader(loadAddr, data, 260-byte chunks)` → `RunLoader(loadAddr)` → device re-enumerates as **PID 0x8001**.

PlayBook loader load address: `0x80100000` (id suffix `$1a06`). The loader we have: `loader_06001A06-00.bin`, 238,128 bytes, ARM32 raw (`02 00 00 EA`), base `0x80100000` — this is "RIM-RAMLoader".

### 4.4 Loader (ramloader) protocol — USB PID 0x8001
**Packet framing** (loader channel 2, identical TX/RX) — TWO levels, confirmed by device testing:
```
wire:   u16 channel(=2) | u16 total(=innerLen+4) | inner        ← TBBUSB.SendData wrapper
inner:  u16 size (= dataLen + 8) | u16 cmd | data[dataLen] | u32 crc32(bytes[0..dataLen+4])
```
Send, get response echoing a response-id, then one extra frame is read (status/flush). For two-byte commands ($20EE etc.) the low byte is the opcode and the high byte arrives as `payload[1]` in the handler. **Handler payload offsets are relative to the inner frame's data start** (e.g. `$E4` reads `total` at payload[0:4], `$E5` reads `{addr, size}` at payload[0:8]).

**Opcode table** (client names from bb10mt, cross-verified against all three decompile dispatchers; ✱ = verified in the PlayBook loader build):

```
20 ✱ persistent data (req carries magic 0x3C6806C / 0x36159469)
21 ✱ bootrom log                     B1 ✱ flash IDs
AD ✱ PMIC info                       B2 ✱ DSP OS metrics
AE ✱ ERASE BLOCKED FW VERSION LISTS  B4 ✱ flash regions info (resp D2, 0xBC bytes)
AF ✱ write DWORD to 8023977C         B5 ✱ flash info (resp D3, 0x1EC bytes)
B0 ✱ bugdisp log (paged, ends D0)    B7 ✱ HASH_BOOTROM
B3 ✱ loader action log (appends to NVRAM record 0x2033)   BD/BE ✱ HW override ID
BF ✱ DRAM info (resp D9)             C4 ✱ READVERIFY (addr,size,val → erase+fill)
C0 40 ✱ COMPLETE (resp 4006)         C6/C8/D4/D5 factory/GRS/appstore/SUPER nukes
C3 ✱ log cfg                         D1 ✱ SVN · D2/D3 ✱ kernel/app metrics
D8 ✱ OS metrics (resp C8)            D9 ✱ get MCT (resp C9)
DA ✱ HIS (rec 0x2027)                DB ✱ vendor ID (resp CB, u16 @+2)
DD   FLASH_DUMP  — NOT implemented in loader_06001A06 (silently ignored)
DE ✱ BOOT_MODE (resp FA)             E0 ✱ remove installer
E4/E5 ✱ CREAD init/read (flash dump channel, encrypted — see §4.6)
E7 ✱ get OS version (FUN_80102668, resp D1, u32) — client calls it "PIN"
E8 ✱ WIPE_SECURITY (sets bits in NVRAM record 0x2814 bitmap — does NOT clear blocklist)
E9 ✱ boot count / OS region info (reads rec 0x2074, resp FB)
EA ✱ get BSN (resp FC)               EC ✱ read blocked OS list (rec 0x2819, resp FD, 0x7C bytes)
ED ✱ read blocked radio list (rec 0x2852, resp FE)        EF 80 ✱ reboot (resp 80C7)
F7/F8 ✱ WRITE DATA (SendBlock: u32 blockCounter + ≤0x3FEC data, resp DF)
F9 40 ✱ SIGNATURE_TRAILER (560 bytes, resp 4006)
FD ✱ stage-append variant (BB10-style write path; resp DF) FF ✱ DDR info (resp 0x78 bytes)
```

Dispatchers in the decompile: `FUN_801032ac` (queries/info), `FUN_80103b90` (actions incl. 0xAE gate), `FUN_8011ae30` (erase/stage/complete), main routing loop ~line 19560.

### 4.5 The flashing transaction
1. `PreFlash($15)` on PlayBook (=$20EE command: {x,0x28,[6]=2,[28]=1,[32]=2}) — selects/preps the flash target.
2. `SendBlock` ($F7) loop: 8-byte header (u32 counter, increments per block) + data, ≤`MAX_FLASH_BLOCK-8` ($3FF4-8) per block. The loader stages chunks (≤0x10000) and flushes to flash in 0x20000 units; **destination is decided by the loader's image-placement state machine**, not by explicit addresses.
3. `SendSignature` ($40F9): a 560-byte trailer. Real images carry one starting with "QNXH"; bb10mt falls back to a `dummy_signature` constant when the source file lacks it — **[!]** which suggests the trailer check may be lenient/deferred, worth auditing.
4. `Complete` ($40C0): finalizes (erase+commit via FUN_8011ad78; `FUN_801206c8` verifies + writes an NVRAM record).

### 4.6 CREAD — REINTERPRETED after device testing: loader-side PRNG keys, RSA-gated
Device testing (Linux, loader mode) proved the original model wrong:
- **`FUN_80105318` is NOT a USB read — it is the loader's PRNG read.** Evidence: `FUN_8012ACFC` = bit-stream generator (bit counter `>>3`, state scrubbing of 0x230/0xCC/0x20 buffers), a 256-byte entropy **ring buffer** (`readEntropyBuffer`/`writeEntropyBuffer`/fill-level helpers), "RNG assertion failure"/"RNG security assertion failure" strings, and **`FUN_80104F28` persisting the 0x418-byte RNG state to NVRAM record `0x201A` after every generation** (anti-reuse; matches the 0x201A records in our backup).
- Therefore the E4 handler GENERATES {AES-256 key, IV, HMAC key, 256B blob} from the loader's own PRNG. The init response = {IV plaintext, stream-enc(HMAC key), u32, u32}. **The session AES key only reaches the host via `$EB` AUTH2 = RSA-2048/PKCS1-type2(session key) under RIM's embedded public key N** (loader file 0x35CCE) — decryptable only with RIM's private key.
- Empirics: run 1 (material inside packet) → loader waited for nothing (total=0 bug) → our read timed out; run 2 (material as 4 framed writes) → `-7` write timeout: the loader never read them (it was generating keys), the OUT FIFO filled. The E4 init most likely SUCCEEDED loader-side (response sent, unread; RNG state saved to 0x201A).
- **Consequence: CREAD `$E5` dumps are not decryptable without RIM's private key. CREAD is dead as a dump path for us.** (It remains RIM-tooling-only by design.)
- Recovery note: the device may be left in loader mode after a failed init — power-cycle before re-running (the harness re-uploads from bootrom cleanly).

### 4.7 The WRITE path (now the primary recovery route) — signature check is CONDITIONAL
`$F9` SIGNATURE_TRAILER (FUN_801205CC): parses the 0x230-byte trailer (magic-checked vs DAT_80120A80/0x88); **never rejects** — resp 6 (parsed) or 0x2E (no/invalid sig) — both continue.
`$C0` COMPLETE → FUN_801206C8:
1. writes NVRAM record 0x2074 = {2, 0, seq} unconditionally (loader-side NVRAM write!)
2. checks record **0x207C**: `FUN_8010B7B8(0x207C)==0` (present) required, else → error 0x14 (**on Unit A 0x207C is missing — likely why reflashing doesn't recover it!**)
3. signature verify (ECDSA P-521, keys @0x35BC4/0x35C49) only runs when `DAT_80120A04[3] != 0` (set per-write in FUN_8011FDD8 from `FUN_801184AC()`/`FUN_8011E0F8()`); **if `FUN_801184AC()==0 || FUN_8011E0F8()==1` → return SUCCESS without any signature check**
4. the F7 path sets *param_2 = 0/1 after any write → C0 takes the FUN_801206C8 path; the non-F7 path (`*param_4 == -1`) does `FUN_8011ACCC()`+`FUN_8011AD78()` (erase @DAT_8011AE00 + write) with NO signature check
RE targets next: FUN_801184AC / FUN_8011E0F8 semantics (the skip condition — likely "signature required only for downgrades"), what creates 0x207C, F7/FD destination steering (registry id 0x39 via FUN_80100DA0; region table source), and `$AF` (writes a mode flag at 0x80118A00 consumed by F7 — not a pointer primitive).

### 4.8 NVRAM-touching paths in the loader
Loader NVRAM primitives: read `FUN_8010b954(id,buf,len)`, status `FUN_8010b7b8(id)`, write `FUN_8010b990(id,buf,len)`. Records the loader itself writes: 0x2033 (action log, via opcode B3), 0x2801 (password), 0x280E, 0x2814 (security bitmap via E8), 0x2821 ("ramimage_reset" creator id), 0x282F, 0x2074 (OS region flag). **There is no arbitrary-id NVRAM write opcode.** The blocklist-erase command (AE) is gated by a RIM signature over its payload (RSA via FUN_80104268/80133EA0/80134194, algo id 0x85) plus a valid current-OS version and readable 0x2819/0x2852 — no forging without RIM's key.

---

## 5. NVRAM (the crown jewel)

### 5.1 Where and how it lives
- A **4MB window of the eMMC user area at byte offset `0x48000000`** (≈1.15GB in), served by `nvram_resource_manager -U 99:0 /dev/hd0 0x48000000 0x483FFFFF` (a process in the IFS).
- Client access: open `/dev/nvram`, then devctl/devctlv — **or plain lseek/pwrite where the offset IS the record id** (that's how the OS-side deckard tooling writes records; xtype 5).
- On-flash format (nvre.pas, CRC-verified):
```
Block header (28B):
  u16 unk1 | u16 BlockNum | u32 Revision | u32 DataCrc (crc32 of data)
  u32 unk2 | u32 BlockLen | u32 DataLen  | u32 HdrCrc (crc32 of header-4)
[Data DataLen bytes] [u32 'NVRE' = 0x4552564E]
```
Records are revision-appended; newest revision wins. Encryption (flag bit4) applies at the driver layer, not in the block format.

### 5.2 Driver devctl interface (verified from 2.x libnvram.so.1 decompile)
```
0x40080501 record count (2×u32 out)
0x4008050D read bootrom metrics (n bytes written; 2.x blob = 0xE8 cached from flash)
0x4008050E get security mode (ONE u32 out: 1-is_secure_cpu; 0 = secure/HS)
0x40080513 read OS metrics (0x1CC; driver itself reads them from IFS @0x10002C)
0x4008050A/0B manufacturing data (12 bytes each)
0x40080510/11/12 misc (FUN_0002532C / FUN_0002474C / FUN_000238DC) [!]
0xC0080502 get record      — iov[0]={hdr8,8} hdr={out result, in id}; iov[1]={buf,size}
                             too-small buffer → EOVERFLOW(9), hdr carries needed size
0xC0080503 get permissions 0xC0080504 is_protected   0xC0080505 record exists
0xC0080506 delete secure   0xC0080507 delete insecure
0xC0080508 write secure record                    0xC0080509 write record
```

### 5.3 Write protection model (driver `NvUpdateRecord` FUN_0001DB70)
- Per-record flags: **bit0 = conditional-write-protected, bit4 = RIM_ENCR (encrypted at rest)**.
- bit0 blocks writes **only if NVRAM record `0x2050` (`NV_OSSTORE_NVR_CONDITIONAL_WRITE_NUM`) is non-zero** (cached at driver init). On my units 0x2050 is **absent** → protected records are freely writable from root. This is why blocklist clearing from a booted OS is trivial.
- 0x2819 reads back plaintext ⇒ not encrypted.
- On a "secure CPU" (ours), the driver **verifies NVRAM signatures** for a per-hardware-type set of records ("Secure device: verifying NVRAM signature"). Signature key selected by HW type: types 3/4/8 use **"NVRAM Sig-development"** (!); PlayBook's metrics word@4 = 3 → **[!] possibly using the development key set**. Key names: NVRAM Sig-r036/r057/r061/r072/r042/development.

### 5.4 Known record IDs
```
0x0001 (9 revs, 16B each: {0, u32 counter/seq?})   — boot-counter-ish [!]
0x2002, 0x2009, 0x2019 (2KB, 16 revs!), 0x201A     — system config family
0x2033 loader action log (string buffer, ≤0x3FE8)
0x2050 NV_OSSTORE_NVR_CONDITIONAL_WRITE_NUM (write-protect gate; absent here)
0x2058 33,048-byte record, 4 revisions (largest; plaintext [!check]) — persistent store?
0x2074 OS boot region flag (24-72B; rev1={2,…}, rev18={3,0,1,…}) — slot selection
0x2078/0x2079/0x207A/0x207C — OS-slot metadata family
0x2801 password record   0x2802   0x280E installer data (42KB)   0x280F   0x2812
0x2814 security-flags bitmap (128B; rev2 = {8,…} = bit3 set)
0x2819 NV_OSSTORE_NotSupportedOS    (OS version blocklist)  ← the downgrade lock
0x2821 ramimage-reset creator id          0x282F installer data
0x2852 NV_OSSTORE_NotSupportedRADIO (radio blocklist; only relevant to 3G/4G winchester2 units — mine are WiFi-only)
0xB000-0xB007 bootrom/boot-related record family (only in NVRAM, not exposed by nv_* names above):
  0xB006 = FULL 732-byte bootrom-metrics blob in flash form (no {0x2DC,3} prefix;
           identical structure to the hd0 flash copy; security u32 @+0x70 = 0 ⇒ HS)
  0xB007 = 16KB × 2 revs, opaque header "3b 6e 2a 38 | 02 | f0" + high-entropy body
           ⇒ RIM_ENCR-class secure record (RPMB/keys-related [!])
  0xB005 = 468B, 0xB000 = 20B, 0xB001-0xB004 = 6B each
```

### 5.4b NVRAM image layout (from the backup, `nvram-backup/nvram0.bin`)
- 4MB window; **first 64KB (0x0-0xFFFF) contains no record starts** (superblock/metadata area [!]).
- Records live at 0x10000-0x97000: **94 valid records, 30 unique ids**; rest of the 4MB is erased (0xFF).
- All records observed with `unk1 = 0x0100` (type/flag field).
- Parser/verifier: `nvram_parse.py` (CRC-validates DataCrc+HdrCrc, revision index, 0x2819/0x2852 decode, usage map, `--dump ID` extraction).

### 5.5 The OS blocklist (0x2819) — exact format
From deckard's `platform/blacklist.py` (QNX-side updater code):
- Payload = up to 10 entries of `OSRangeAndType(rangeStart, rangeEnd, osTypeBitMask)` as `struct '=III'` (12 bytes each, LE). 120 bytes max read.
- `STP_PROTOCOL_TYPE__SHIPPING_OS = 4` → shipping-OS mask `1<<4 = 0x10`.
- Semantics: an OS version is **blocked** if it falls inside any entry's `[rangeStart, rangeEnd]` for a matching type mask. An `EMPTY (0,0,0)` entry truncates the list; empty data = "missing" = no blocks.
- Version encoding in `rangeEnd` (two datapoints, rule: `u32 = (major<<24) | (minor<<16) | (build & 0xFFFF)`):
  - backup NVRAM: `0x0200229B` = **2.0.0.8859**
  - live 64GB unit: `0x02080501` = user2os([2,1,0,1281]) → decodes as (2, **8**, 1281) — **[!] the "8" implies the OS-internal minor for user 2.1.0 is 8 (deckard's user2os mapping), not a literal 1**
- Live 64GB unit @2.1.0.1917: `{0, 0x02080501, 0x10}` — floor 2.1.0.1281. Backup (32GB): `{0, 0x0200229B, 0x10}` — floor 2.0.0.8859.
- The ramloader's loader-side gate `FUN_80104180` (opcode AE) validates the signed replacement list against the current OS version before allowing the change.
- **Unbrick implication**: a unit that can't boot can't run the QNX tooling. The ramloader exposes no raw NVRAM write, so NVRAM repair on a bricked unit needs either (a) RIM-signed AE payload [no], or (b) raw flash surgery: `$EE` ERASE_SECTOR (address-driven!) + staged write via `$F7`/`$FD`, rewriting the NVRAM 4MB window (restoring a backup image, or clearing 0x2819 records). NVRAM flash offset `0x48000000` must still be translated into the loader's addressing — get it from `$B4` flash-regions info.

### 5.6 Bootrom metrics (the "bmetrics" blob)
- RAM form (devctl 0x4008050D, 0xE8 bytes visible): `{0x2DC(=full flash size 732), 3[!], 0x00060018, 0x2DC, 0x051B0014, hardwareid 0x06001A06, "RIM BlackBerry Device", "ec_agent", "Mar 24 2011", "15:31:02", …, security u32@~0x78 (0=HS, 1=GP) = 0, 0xB4, 0x14, 0xDC, 0x80, 0x10, ptr 0x80014674, 0x20000, 0x20, FF-padding}`.
- Flash form: at hd0 `0x1D044` (sans leading 8 bytes); the **cookie-headed** copy (`0xD7B02D1F` @base+0x34, blob at base+(hdr[12]−hdr[8])) — the one the NVRAM driver parses for the **registry** (id→value entries incl. 0x18 "Bootrom's starting address", 0x29, 0x39 flash-regions…) — has not been located in the dumped ranges yet; candidates: eMMC boot hw partitions (hd4/5/6) or deeper hd0 offsets.
- The loader's device-info struct is built from this blob (`FUN_80100DE0`: registry at struct+0x94); many loader behaviors key off registry ids (0x18, 0x29, 0x33, 0x35, 0x39, 0x2B/0x2C/0x31…).

---

## 6. Device access cheat-sheet (from a booted, rooted unit)

```sh
# NVRAM (tool: nvram-editor/nvram.c; compiles on-device; loads /lib/libnvram.so.1)
./nvram read 0x2819 [hex|dec|ascii]
./nvram partialread 0x2819 0 120
./nvram write 0x2819 0x00 0x00 …      # data: 0xNN bytes, 0xMMMM multi-byte LE, or strings
./nvram patch 0x2819 <offset> …       # read-modify-write
./nvram get_permissions 0x2819 / is_protected / record_exists
./nvram get_bootrom_metrics           # devctl 0x4008050D, 0xE8 bytes
./nvram get_os_metrics                # devctl 0x40080513, 0x1CC bytes
./nvram get_record_count / get_security_mode

# block devices (devcls 4): hd0 raw eMMC, hd1/hd2 OS slots, hd3 root, hd4-6 [!] boot hw parts
dd if=/dev/hd1 of=/accounts/devuser/x.bin bs=65536 count=32 ; sync   # numeric bs ONLY, then sync

# raw physical memory — ONLY with memdump.c v2 (mmap_device_memory, PROT_NOCACHE):
./memdump 0x4A0022C4 4 status.reg
```

Transfer off-device before any risky operation; qnx6 discards uncommitted cache on a kernel crash (learned the hard way — 8MB of dumps lost to a watchdog reset).

---

## 7. Security posture summary

| layer | protection | state |
|---|---|---|
| TI boot ROM | verifies CertISW/first loader (HS eFuse) | no bypass known; ROM dump pending |
| PRIMAPP/CertISW | RIM-signed, "Cert"ed | dumped; not yet analyzed |
| RIM-BootLoader / "RIM bootrom" | signed chain stage | dumped (hd0 0x4000-0x14900, 0x14900-0x1D044); not yet analyzed |
| OS slot boot region (1MB) | "QNXH/QNXL/QNX" trailer blocks | dumped; verifier not yet analyzed |
| IFS (rifsboot) | "INVALID/FOUND valid IFS signature" | checker code in hand (dumpedifs) |
| rcfs | `rfs_validator` (in IFS) | binary in hand |
| NVRAM records | conditional-write gate (0x2050=absent ⇒ open), RIM_ENCR (unused on our targets), per-HW-type signature set (possibly "development" keys **[!]**) | writable from root on booted units |
| Downgrade policy | NVRAM 0x2819/0x2852 OSRangeAndType lists, enforced by loader (AE gate) + OS-side deckard | fully decoded; clearable on booted units; bricked units need flash surgery |
| USB ramloader | no raw NVRAM write opcode; AE needs RIM sig; **CREAD dumps are RSA-gated** (loader-side PRNG keys, session key only via $EB RSA → RIM-only) | protocol fully mapped; **$C0 commit signature check is CONDITIONAL (§4.7)** |

Realistic attack surfaces for **unsigned OS**:
1. **The `$C0` commit signature check is conditional** (§4.7): skipped when `FUN_801184AC()==0 || FUN_8011E0F8()==1` — likely "signatures only required for downgrades". If controllable, **flashing an unsigned OS via the normal loader path may just work** — top-priority RE target.
2. rifsboot/IFS signature check (QNX code, in hand) — length/pointer bugs in hash-compare or trailer parse.
3. QNXH/QNXL/QNX trailer verification in the 1MB boot region (dumped, needs Ghidra).
4. The 560-byte SIGNATURE_TRAILER acceptance at flash time — the $F9 handler **never rejects** (invalid/missing sigs are logged and flashing continues) — bb10mt's `dummy_signature` **[!]** now has a mechanism behind it.
5. rfs_validator for rcfs.
6. Registry/metrics-driven loader logic (id 0x18 etc.) — HW-type confusion, "NVRAM Sig-development" key path.
7. Bootrom itself (dump pending; needs correct base — get from registry blob, then memdump v2).

## 8. Recovery plans (bricked units) — ON HOLD (user pivot: custom OS on a working unit)
### Unit A — "NVRAM signature error" (NVRAM wiped; full pre-wipe backup exists)
STATUS: recovery tooling built, but the plan changed after device testing — **CREAD dumps are RSA-gated (§4.6)**, so read-back verification via CREAD is off the table. Revised plan:
1. Connect (bootrom → upload loader_06001A06-00.bin → loader mode). Works (tested on Linux; Windows blocked by RIM drivers).
2. Safe probes: `$EC` (blocked OS list), `$E7` (OS version), `$B4` (flash regions — **the addressing we need**), `$B5` (flash info).
3. ~~CREAD dump~~ → replaced by: erase (`$EE`, address-explicit) + write the restore image ($F7/$FD steering — **RE needed on destination control, §4.7**) — write path needs NO crypto stream.
4. **Blocker found**: `$C0` commit requires NVRAM record `0x207C` present — missing on Unit A (wiped) → commits fail 0x14; likely why reflashing doesn't recover the unit. RE needed: what creates 0x207C / whether the skip-condition (`FUN_801184AC()==0 || FUN_8011E0F8()==1`) applies.
5. Verification WITHOUT CREAD: `$EC` (0x2819 content must show the backup's floor 2.0.0.8859), `$E9` (0x2074), `$ED`, and cfp info's "Boot Count Info: NVRAM Signature Invalid" → should read valid after restore.
6. **Restore images are built and verified** (`nvram-backup/restore/`): `nvram_restore_pristine.bin` (byte-identical backup), `nvram_restore_blocklist_empty.bin` (0x2819 → EMPTY entry), `nvram_restore_blocklist_erased.bin` (0x2819 region → 0xFF). All CRC-valid; empty/erased variants differ from pristine in exactly **one 4KB page (0x08F000)**.
   - NVRAM image rules: records packed 0x10000→0x97000 (first 64KB zero-filled, tail 0xFF); `BlockLen` = allocation to next record; HdrCrc covers header bytes 0–23 **including DataCrc** — any edit must recompute both CRCs (`nvram_restore.py`).
### Unit B — "RPMB error" (factory boot counter expired)
- Factory mode is entered by adding a file to RPMB; the factory **boot counter** has since expired. RPMB's monotonic write counter means a naive "write the old value back" is impossible. Needs RE of how the counter is stored/checked: `rpmb-omap4` + `trustzone-omap4` (in dumpedifs) and the loader's RPMB code ("Fail_to_write_RPMB"). No plan until that's understood — don't touch RPMB further on working units.
### Any unit — blocklist-downgrade lock
- Clearable on booted units (root + nvram write, 0x2050 absent ⇒ gate open). On unbootable units → flash surgery per Unit A.
- Last-resort for any unit: nukes ($C6/$C8/$D4/$D5/$E8) — destructive; E8 provably does NOT clear the blocklist.

## 9. Tooling & artifacts (workspace inventory)
- `SESSION-HANDOFF.md` — detailed session state (findings too fresh for this doc land there first)
- `loader_06001A06-00.bin` + `loader_06001A06-00.bin.out.c` — USB ramloader + full Ghidra decompile (45.6k lines)
- `nvram-editor/` — nvram.c (2.x-compatible), 2.x `libnvram.so.1` + its decompile, `nvram_resource_manager` + its decompile
- `dumpedifs/` — full IFS contents (rfs_validator, rpmb, trustzone, nvram_resource_manager, bmetrics, …)
- `bootchain-dumps/` — hd0/hd1/hd2 heads + `ANALYSIS.md` + extracted binaries (bootstrap_loader, chain_0x14900, qnx_bootregion)
- `nvram-backup/` — full 4MB NVRAM image (pre-wipe backup of Unit A) + extracted records + `restore/` (three verified restore variants from `nvram_restore.py`); parse/verify with `nvram_parse.py`, build restore images with `nvram_restore.py`
- `bb10mt-main/` — flashing tool source (Pascal): `ramloader.pas` (flow), `bbloader.pas` (opcode client + name table), `bbusb.pas` (wire protocol), `nvre.pas` (NVRE flash format), `uflash.pas` (CLI info/flash). PLUS our additions: `bbcread.pas` (CREAD client — **note: built on the pre-reinterpretation key model; the E5 decrypt path is RSA-gated, so it's useful as transport/framing reference only**), `creadtest.lpr/.exe/.cfg` (device harness, self-tests pass, works through loader-mode on Linux), `build-linux.sh`, `libusb-1.0-mingw32.dll`
- `cread_client.py` — Python CREAD reference — **SUPERSEDED**: built on the wrong key model (host-sent keys); the framing/packet layout and the AES/HMAC stream mechanics are still correct reference material, but the E4 key-material flow is not (loader generates keys)
- `bb10mt-probe-patch.md` — safe probe patch draft
- `bbcread-integration.md` — integration + build recipe (§0 = working creadtest build; device-test checklist; the framing fixes)
- `brly-2026-038-notes.md` — U-Boot advisory analysis (not applicable: loader is RIM-proprietary, not U-Boot)
- `memdump.c` (v2, mmap_device_memory), `device-dump-bootchain.sh` (v2, sync discipline), `device-enumerate.sh`, `device-probe.sh`
- `libs/` — cloned deps for building: `pas-libusb` (**libusb-1.0 branch**), `mORMot2` (build with `-dNOASMBLOCK`)

## 10. Hard-won operational lessons
1. **sync after every dd**; copy files off-device before risky ops (qnx6 drops uncommitted cache on kernel death).
2. **Never plain-mmap() device/unmapped memory** — use `mmap_device_memory(..., PROT_READ|PROT_NOCACHE, MAP_SHARED|MAP_PHYS, …)`; speculative reads to unmapped bus = external abort = watchdog reset (~10s).
2b. **Never `dd if=/dev/mem skip=…` to probe registers** (learned 2026-08-30, device rebooted): skip does not guarantee a seek — dd walked the physical bus from 0 up to the target offset and hit an unmapped region (same class of crash as lesson 2). Per-address reads MUST go through a compiled tool using mmap_device_memory v2 semantics, one specific address at a time, only after the address is proven live/clocked (e.g. via `pidin mem` of a driver that maps it).
3. QNX userland gaps: no od/head, dd numeric-only, `pidin arg`.
4. bb10mt's comment table covers the whole BB10 family — verify each opcode exists in the PlayBook loader build before relying on it (e.g., DD doesn't).
5. The 2.x and BB10 libnvram builds differ (sizes 0xE8 vs 0x4FC, missing wrapper exports) — never mix.

## 11. Kexec-era facts (2026-08-30, kexec project)
- **Boot time: 2–3 minutes** (normal cold boot to runlevel 2/SSH); WDT2 warm-reset recovery takes about the same. Do not mistake a slow reboot for a hang — wait ~3 minutes before concluding.
- The payload's jump buffer, flat page table (IRAM 0x40304000), trampoline/continuation (buffer +0x800/+0x830) and breadcrumbs (0x9FE00000 mirror) are all part of the working T2 implementation — see `kexec/README.md` and KEXEC-DESIGN §7b.
- eMMC part differs per unit: 64 GB unit = Samsung MCGAFA; 32 GB units = SanDisk SEM32G. Both boot chains identical otherwise.
- DRAM notes: QNX post-boot FreeMem 582 MB/1024 MB (137 procs); runlevel-0 (env.sh time) leaves far more free — jump region 0x84000000+ is safe.
