# kexec/ — tooling and the jump, technical reference

Everything needed to rebuild and rerun the QNX→Linux jump experiment.

## Environment

- Host: Linux PC with the **QNX SDP 6.6** unzipped at `/home/psyden/qnx660-master`
  (`source ../qnx-env.sh` from this directory sets `QNX_HOST/QNX_TARGET/PATH`;
  the stock `qnx660-env.sh` points at /opt — ours relocates it).
- Device: PlayBook 64 GB, OS 2.0.0.4869, reachable over USB RNDIS:
  `ssh -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o MACs=+hmac-sha1 -i rsa -l root 169.254.0.1`
  (key file `rsa` in the workspace root; USB RNDIS, 169.254.0.1).
- `/tmp` on the device is wiped by every reboot — redeploy binaries each session.

## Build & deploy

```sh
source ../qnx-env.sh && ./build.sh          # assembles stubs, builds all tools
scp qnx2linux hello.bin memdump3 memw32 root@169.254.0.1:/tmp/
```

## The jump sequence (what `qnx2linux --t2` does, in order)

1. `ThreadCtl(_NTO_TCTL_IO_PRIV /*=1*/, 0)` → the thread runs in **System
   (privileged) mode**. This is the master key: CP15 and SMC become legal.
   (QNX has *no* SMC emulation; userland `smc` in user mode = SIGILL.)
2. Breadcrumbs armed at **0x9FE00000** (mirror — survives warm resets; the
   primary 0x9F000000 gets churned by the reboot) plus a synced file snapshot
   `/accounts/devuser/kexec-bc.log`.
3. Target blob copied into a `MAP_ANON|MAP_PHYS` buffer (pinned, contiguous),
   verified through a second NOCACHE mapping of the same physical range.
4. Flat 1-level page table written to IRAM 0x40304000 (16 KB): identity sections
   for 0x403 (IRAM) and 0x800–0xBFF (DRAM).
5. Trampoline + continuation (from `stub3.S`, linked 0x40308000+) copied into
   the executable jump buffer (trampoline at +0x800, continuation at +0x830).
6. `clean_inval_l1_all()` — full L1 clean+invalidate by set/way (CP15).
7. **CPU1 held in warm reset** (`RM_PDA_CPU1_RSTCTRL` bit0 @ 0x4824380C).
8. GICD disabled (0x48241000 = 0).
9. `cpsid if` → trampoline: TTBCR=0, DACR=all-manager, TTBR0=flat table,
   TLBIMVA(continuation), branch → continuation (VA==PA): TLBIALL,
   SCTLR M/C/I off → `bx r9` → target blob with **r0=0, r1=0xffffffff,
   r2=DTB-phys** (ARM Linux boot convention, DT machine selector).

The T2 target blob (`hello.S`) writes breadcrumbs, streams `KEXEC-HELLO` on
UART3 (may be externally invisible), then holds both cores so the WDT2 warm
reset fires ~15 s later — preserving the breadcrumbs for readback.

## Running

```sh
on -C 0 /tmp/qnx2linux --t2        # T2: jump test (maxcount via hello.bin build)
on -C 0 /tmp/qnx2linux --t3        # T3: real kernel (zImage + appended DTB)
```

### T3 — booting the real kernel (next milestone)

Kernel: mainline 6.15.11, `omap2plus_defconfig` + `ARM_APPENDED_DTB`,
`ARM_ATAG_DTB_COMPAT`, `DEBUG_LL` via **`DEBUG_PLAYBOOK_BC`** (custom
`debug/omap4bc.S`), `EARLY_PRINTK`, SERIAL_8250_OMAP console. Built on the host with
the Bootlin armv7-eabihf GCC 14.3 toolchain at
`/home/psyden/toolchains/armv7-eabihf` (kernel tree: `/home/psyden/kernel/linux`).

```sh
./mkkernel.sh zImage          # or: ./mkkernel.sh Image (uncompressed, 18 MB)
scp kernel/zImage kernel/omap4-winchester.dtb qnx2linux \
    root@169.254.0.1:/tmp/
on -C 0 /tmp/qnx2linux --t3 /tmp/zImage /tmp/omap4-winchester.dtb
```

- **Blob layout in the 24 MB jump buffer**: zImage at +0 (padded to 8), DTB
  appended right after; `r2` = appended DTB phys (belt and braces with
  `CONFIG_ARM_APPENDED_DTB`), `r1` = 0xffffffff, entry = zImage base.
- **No self-reset**: the kernel runs; WDT2 (armed, un-kicked) warm-resets the
  board ≤15 s later whatever happens — RAM content survives.
- **Breadcrumbs moved (ROUND 5B)**: primary bc = **0x90000000**, mirrors =
  0x94000000 / 0x88000000 / 0x9FE00000 (scatter-proven survivor band
  0x85–0xA1; 0x9F/A4+ get churned nondeterministically by QNX reboot — see
  `scatter.c`). All four pages are `no-map` in the DTS.
- **DEBUG_LL ring capture**: every debug char goes to UART3 *and* into three
  rings (in the bc pages: count @+0x80, index @+0x84, 1024 chars @+0x100).
  **PL310 clean+invalidate-by-PA is issued per char** (0x48242000+0x768,
  static-mapped at 0xFEB21000 for kernel-proper) — without it the hot ring
  lines never evict from L2 and the warm reset destroys the capture (this
  defeated the first attempts; direct PL310 access is proven on this unit).
- **Trampoline/continuation live in IRAM** (0x40308000/0x40308040): the jump
  no longer depends on where QNX placed the jump buffer; post-TTBR0-switch
  stores all use physical addresses.
- Readback after reboot:
  `on -C 0 /tmp/memdump3 90000000 0x18` (bc trail) and
  `on -C 0 /tmp/memdump3 88000080 0x480` / `90000080` / `94000080` (rings:
  count, index, then chars as ASCII text).
- DTS: `omap4-winchester.dts` (1 GB @0x80000000, UART3 stdout, TWL6030 on
  I2C1, eMMC on MMC2 8-bit non-removable, bc pages reserved `no-map`).
- Watchdogs: mainline `omap_wdt`/`twl6030_wdt` only kick when opened — until
  userspace does, a surviving kernel still gets WDT2-reset at 15 s. That is
  the intended T3a recovery + readback path.

`on -C 0` (CPU0, 0-based) is mandatory-ish: the payload must not live on CPU1.

Verdict readback after the automatic watchdog reboot:

```sh
/tmp/memdump3 9FE00000 14          # breadcrumb mirror (the reliable one)
```

Trail values: 39/40 prep · 37 CPU1 held · 32 L1 flushed · 41 GICD off ·
70 pre-blx · 61-64 trampoline · 21 identity · 23 MMU off · 3 blob ran ·
4 blob done. `[2]` = UART message count, `[3]` = run nonce (the buffer phys).

## The tools

| Tool | Purpose |
|---|---|
| `memdump3.c` | safe single-address physical reads (`mmap_device_memory` PROT_NOCACHE). Bad address → contained SIGBUS, device survives. |
| `memw32.c` | single 32-bit physical register write, optional read-modify-write mask. |
| `secure-probe.c` | raw SMC passthrough via `/dev/trustzone` devctl `0xC0280501` (40-byte payload `{result,fnid,a1,a2,n,w[4],pad[2]}`; monitor reads service id from r0, SMC #1; param block = `{count, params...}` at r3-phys). |
| `xntest.c` / `xntest2.c` | executability + cache-maintenance probes (XN enforcement, msync flags). |
| `qnx2linux.c` | the payload itself (`--t2` jump test; `--hello` legacy CPU1-release variant). |
| `stub3.S` | trampoline + continuation (assembled, linked at 0x40308000). |
| `hello.S` | T2 target blob (UART + breadcrumbs + controlled reset). |
| `cacheops.S` | `clean_inval_l1_all()` (CCSIDR-decoded set/way loop) + `read_ccsidr_decode()`. |

## Hard-won rules (violating any of these costs a 3-minute reboot cycle)

1. **Never sweep the bus.** One specific address per read/write, only addresses
   proven live (a driver maps them — check `pidin mem`) or TRM-confirmed
   always-on. `dd if=/dev/mem skip=` walks the bus and crashes the device.
2. **No I/O after the CPU1 hold or GICD off.** The console (USB RNDIS) dies;
   `printf` blocks forever; the payload never proceeds.
3. **All kernel calls (mmap_device_memory) before IRQs are disabled.** Blocking
   kernel calls with IRQs off deadlock the payload.
4. **Byte pointers for raw copies.** `uint32_t* + 0x830` = byte offset 0x20C0
   (corrupted a page table once).
5. **Pinning:** `on -C 0` (0-based) puts the payload on CPU0. A shell on CPU1
   dies when CPU1 is reset.
6. **Breadcrumbs at 0x9FE00000 survive warm resets; 0x9F000000 does not.**
   The blob must force its own reset (hold both cores) — otherwise the TWL6030
   PMIC watchdog (127 s, power-off) or a manual power cycle destroys the RAM.
7. QNX enforces **XN on data/anon pages** (heap SIGSEGVs when called) but honors
   `PROT_EXEC` on `MAP_ANON|MAP_PHYS` — jump buffer must be mapped with it.
8. QNX has **no SMC emulation**: userland `smc` = SIGILL. `ThreadCtl(
   _NTO_TCTL_IO_PRIV, 0)` (System mode) makes CP15 and SMC legal — this is how
   the trustzone driver works.
9. **WDT2 = 58.6 s** (LDR 0xFFE2B400 @ 32.768 kHz), kick = any write to
   0x4A314030 (TGR). The "15 s" = QNX's wdtkick PERIOD.
10. **Payload user-mode SIGSEGV ≠ reboot** (2026-09-02): QNX survives, SSH
   stays up. Only jump-context deaths (post-GICD-off) reboot. BUT: PRCM
   (CM1/CM2) register writes = SIGSEGV (secure-filtered) — and PRCM writes
   from the JUMP context take the machine down.
11. **UART pads: none accessible** (2-hour hunt, 2026-09-02). The console
   readback = **ring3 (0x94000100), the secure monitor's UART3 capture** —
   persistent across reboots. Also see docs/06 (the WDT2 register map) and
   docs/03 (the SMC hang matrix).

## Key register map (see PLAYBOOK-REFERENCE §1.6-1.10 for the full picture)

| Register | Address | Notes |
|---|---|---|
| UART3 (console) | 0x48020000 | 115200 8N1, 48 MHz clock |
| WDT2 | 0x4A314000 | kick = complement WTGR (+0x30); WSPR +0x48 (0x4444=enabled; disable = 0xDDDD→0x0000) |
| GICD / GICC | 0x48241000 / 0x48240100 | |
| PL310 | 0x48242000 | **secure-only from NS** (direct access aborts) |
| WUGEN AUX_CORE_BOOT_0/1 | 0x48281800/04 | ROM pen (cold boot only) |
| WKG_CONTROL_0/1 | 0x48281000/1400 | per-CPU status (STANDBYWFI bit 8) |
| RM_PDA_CPU0/1_RSTCTRL | 0x4824340C / 0x4824380C | bit0 = per-CPU warm reset, held until cleared |
| CM_MPU / CM2 | 0x4A004000 / 0x4A009400 | NS-accessible |
| PRM_MPU | 0x4A326000 | firewalled; SAR RAM context restore lives there |
| SAR RAM | 0x4A326000+ | CPU1 warm-reset context restore (monitor-verified, cannot forge) |
| OCMC IRAM | 0x40304000+ | stub location (all zeros at runtime, 56 KB region) |

## Known constraints / open items

- **CPU1 cannot be redirected** (SAR RAM context restore is monitor-verified);
  it is held in reset across the jump. SMP later = small `omap4_boot_secondary`
  kernel patch (deassert RSTCTRL + pen regs) or an IRAM WFE pen.
- **PL310 is not NS-accessible** → for a real kernel boot, L2 hygiene relies on
  the boot chain's own L2 init after the WDT2 warm reset; a monitor-mediated
  range flush (TI service 0x101) is **not** implemented on RIM's monitor.
- **UART3 external pads unknown** — breadcrumbs are the working debug channel;
  probe method: `echo x > /dev/ser1` while scoping candidate pads.
- eMMC = MMC2 (0x480B4000) — first DTS fact.
- Boot takes 2–3 minutes; the watchdog warm-reset path is the recovery for any
  jump-phase mistake.
