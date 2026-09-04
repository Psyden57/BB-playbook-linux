# Troubleshooting Guide

## NEW (2026-09-04, session 9 — the session-9 failure modes; the sections below = earlier eras)

### The early-C death ladder (bc[1]=161/156/133/142/121) — ONE root class
- The "random wandering" early-C deaths (svm alloc W-25/31/32a, FDT walk
  W-26, map_lowmem pte alloc W-27, MMU-enable W-29, head.S tail W-35) =
  two root causes, both identified: (a) the stale-pv regime (CURED,
  build #106: the W-32c direct-store block in start_kernel, marker 163,
  tries in bc[19] slot 0xD000004C — if pv_off=0 EVER reappears in the
  console, the fix failed: investigate immediately), and (b) the stale
  pgd pair [VA 0xdfc/0xdfd] = bogus table descriptors (0xbfc1141e) → any
  allocation in the top 2MB of the linear map wedges on the walk
  (build #110 shaves the allocator limit by 2MB; untested as of this
  note).
- Distinguish the setup.c pb_bc pairs (130-136) from mmu.c's PB_MMU_BC
  numbers via the MIRROR channel (rule 17, docs/README): a mirror value
  of 0x46 with bc[1]=133 = setup.c's parse_early_param-done, NOT
  map_lowmem.
- **The head.S-tail death (bc[1]=142, ring 0, bc[2]=0x3E7)** = a payload
  window overlapping the zreladdr inflation region (W-35 — the only one
  ever; the placement guard now reserves [0xa0000000, 0xa1000000)).
- **The payload's own memtest/copy writes can kill QNX** (W-30: froze
  between bc 31 and 39, no jump, WDT2 reset ~1 min) — the sweep's
  "free" window overlapped live QNX-owned memory; bc[18] now records
  the placement BEFORE the memtest.
- **The external-abort freeze class** (W-28): a SIGBUS fltno=5 on a
  device register read (MMC2 MMCHS) = the box froze completely afterward
  (SSH dead, display dead; power-hold to recover) — NOT the cheap
  payload-crash class.
- **A dead devb (the --dmaquiet slay) breaks eMMC execs**: "cat: cannot
  execute - No such file" = an exec needing a devb READ — expected, NOT
  a fault. qnx6's write-back cache still absorbs file WRITES.

## NEW (2026-09-03, session 7 — the l2x0_of_init hazards, BEFORE the boot reaches init_IRQ)

### The boot dies (deadlock) at l2x0_of_init / init_IRQ — EXPECTED unless patched
- The old "guard cache-l2x0.c against the NS latency SIGBUS" warning is
  SUPERSEDED: the DT latency writes (l2c310_configure) route through
  l2c_write_sec → omap4_l2c310_write_sec → default (WARN+skip). No NS latency
  write ever happens. No patch needed.
- The REAL hazards in the same function: `l2c_enable` (cache-l2x0.c) does
  `__l2c_op_way(base + L2X0_INV_WAY)` — a **0x7FC by-way write = the
  on-device DEADLOCK op** (docs/06: by-PA 0x768 and sync 0x730 are NS-safe,
  by-way 0x7FC is not) — followed by `l2c_wait_mask`, a poll on the sync reg.
- Fix when the wall is broken: patch l2c_enable to skip the by-way op on
  winchester, or CONFIG_CACHE_L2X0=n (keep QNX's L2 setup). Details:
  docs/03 session 2026-09-03.

### The PPA probe (--ppa) died mid-probe (WDT2, ~59 s)
- bc[1] names the hung call: 49 = after 0x25 (return), 50 = 0x26 in flight,
  51 = 0x27 in flight, 52 = 0x23 L2_POR in flight, 53 = all done.
- 0x23 (L2_POR) re-inits the L2 in secure mode — under live QNX that can
  wedge or kill QNX; WDT2/hard-reboot recovers. 0x26/0x27 are devpm's
  suspend pair — devpm calls them in normal suspend, so they should be safe.

## NEW (2026-09-02 NIGHT, session 6 — the current failure modes; the 171-wall section below = HISTORICAL, solved)

### The boot dies at bc[1]=127 (paging_init: map_kernel done) — THE CURRENT WALL
- **Symptom**: --l2on boots reach 127 with ~1100-1200 chars of clean console
  in ring1, then die at dma_contiguous_remap: `BUG: not creating mapping for
  0xa1000000 at 0x1f7f0000 in user region` (a corrupted dma_mmu_remap[0].base
  — 0xbe800000 → 0xa1000000). Deterministic per placement, both L2 modes.
- **Cause**: the machine's silent DRAM corruption under QNX's secure-domain
  L2 config (PL310 data latency 0x111 = 1/1/1 cycles, live; NS write =
  SIGBUS; no monitor latency service; L3/EMIF auto-idle also secure-only).
- **Fix**: RE trustzone-omap4 for a latency/PPA service; the payload sets
  3-cycle latencies pre-jump. See 05_NEXT_STEPS.md and
  HANDOFF_2026-09-02_night_SMC-flush-pv-wall.md. Do NOT chase the mapping
  code or the pv-fixup — the corruption is upstream of them.

### The console dies mid-line (~900-1200 chars, any content)
- **Symptom**: ring1's log ends mid-line at a varying point; earlier boots
  printed fine.
- **Cause (FIXED)**: per-char DEVICE (SO) stores to the ring sections wedged
  with the L2 on (the device-store traffic cliff). The ring/bc sections are
  now CACHEABLE (r7) with batched SMC 0x101 flushes per console chunk. If it
  recurs, suspect the flush batch (pb_flush_rings) or a reintroduced
  device-store path.

### The 0x102 disable SMC hangs (rare)
- **Cause**: an IRQ arriving mid-CTRL-write (the "FLAKY mid-flight"). The
  payload's GICD-off before the SMC = the proven guard. The enter_stub-inline
  SMC shape also hung 9/9 — C-flow only.
- **Fix**: retry after the WDT2 reboot; keep the GICD-off/CPU1-hold recipe.

### Ring text looks garbled (|0x80 bytes, pair-swapped words)
- **Cause**: the OLD corruption era (the uncached kernel / the bypass path).
  With the current cacheable-ring + SMC-flush console the text is clean.
- **Decode note**: memdump3 prints words BIG-ENDIAN-formatted — reverse each
  4-byte group. Unwritten ring slots read 0xaa/0x55 patterns.

### bc[2] = junk (e.g. 0x3E7/999), bc[3]/bc[4]/bc[12] = 0xa1xxxxxx pointers
- **Cause**: QNX-boot leftovers in the bc page below the payload's armed
  slots — NOT kernel wild writes. Only fresh-run nonces + values the current
  kernel/payload demonstrably wrote are trustworthy.

## HISTORICAL (2026-09-02 late): the 171 wall / late-boot failure modes — SOLVED (the bypass cliff + the uncached kernel)

### The kernel stops at bc[1]=171 (post cgroup_init) — THE 171 WALL
- **Symptom**: 6+ consecutive runs; bc[1]=171 exactly; the fine markers inside
  taskstats_init_early (181/182) and kmem_cache_create (183/184/185) never land;
  the initcall breadcrumbs (bc[6]/bc[7]) never land; bc[2]=0x3E7 (999) always
  co-occurs (a wild write — pb_bc would write bc[2]=427).
- **Cause (SOLVED session 6)**: the L2-off bypass path wedges under sustained
  traffic (the --l2test A/B proof) + the head.S C/B/S strip left the kernel
  UNCACHED. The slab_mutex/slab_caches theories were symptoms of the corrupted
  transactions, not the disease.
- **Debug**: see HANDOFF_2026-09-02_night_SMC-flush-pv-wall.md.

### No console output / the ring3 log is frozen
- **Symptom**: ring3's count (0x94000080) identical across runs; the content =
  an old boot's log.
- **Cause (FIXED run 32)**: omap4bc.S's addruart returns UART virt 0xFED20000
  but head.S never mapped section 0xFED — MMU-on console writes were silently
  dropped by the L3 (async error, no abort taken).
- **Verify the fix**: after a run, ring3's content should show the CURRENT
  kernel's output (check the Linux version string against the built kernel).
- **If still silent**: check that earlycon0 registers (add a bc[10] marker in
  setup_early_printk) and that early_write is called (bc[10]=second value).

### The monitor's ring3 count/index cannot be cleared
- **Symptom**: the payload zeroes 0x94000080/84; the next readback shows the
  old values (e.g. count=0x5A15=23061).
- **Cause**: ring3 = the secure monitor's UART3 capture buffer; the count
  lives in the secure world and is re-written on every capture.
- **Rule**: use the bc[15] nonce (bc page) for freshness; read ring3's CONTENT
  (the 1 KB char window at 0x94000100) and look for the current boot's output.

### PRCM (CM1/CM2) writes from NS → SIGSEGV
- **Symptom**: payload dies with a data abort at the first CLKSTCTRL write
  (e.g. 0x4A008700); **QNX SURVIVES** — SSH stays up, the WDT daemon keeps
  kicking (run 31: the payload crashed, NO reboot).
- **Cause**: the PRCM clock-management registers are secure-filtered.
- **Rule**: clock-domain control requires the monitor's PPA services
  (SMC#1 family). Direct NS writes are impossible.

### The watchdog fires "too early" / kicks seem ignored
- **Check 1**: the head.S 0xFEC section's PA encoding — the 0xFEB pattern's
  `orr #0x200` carries PA bits; with it the WDT section decoded to PA
  0x4A500000 and the kicks were no-ops (runs 24–29, FIXED run 30).
- **Check 2**: read the live WDT state: `memdump3 4a314028 0x8` — CRR (0x28)
  vs LDR (0x2C): CRR close to LDR = recently kicked ✓.
- **Check 3**: the window is 58.6 s (LDR 0xFFE2B400 @ 32.768 kHz) — a "60 s"
  boot death at a deterministic marker = the window expiring, not a code bug
  (this masked the real state for runs 17–29).

## NEW (2026-09-01): PL310 / Secure-Filter Failure Modes

### NS write to PL310 control (0x48242100) → BUS HANG
- **Symptom**: payload freezes mid-block; bc frozen; device reboots 60 s later
  (WDT2). No SIGBUS, no output.
- **Cause**: PL310 control register is secure-filtered; the NS write hangs the bus.
- **Rule**: never write PL310 control/aux/latency from NS. Use the TI monitor
  (real service IDs pending RE; 0x101 = clean+inv by PA is verified-good).

### NS write to PL310 data-latency (0x4824210C) → SIGBUS
- **Symptom**: `Process terminated SIGBUS code=3 fltno=5` in the payload; device
  stays up (procnto catches it).
- **Cause**: synchronous data abort on the secure-filtered register.
- **Rule**: same as above — monitor only.

### NS by-way clean+inv (0x7FC) → DEADLOCK
- **Symptom**: payload freezes in the sync poll right after the by-way write;
  bc frozen; reboot after 60 s. Happened with CPU1 active AND held.
- **Cause**: by-way is a BACKGROUND operation — deadlocks under concurrent
  access (PL310 r3p2 erratum 727915 class).
- **Rule**: use only by-PA (0x768) foreground ops from NS. Never 0x7FC.

### Monitor SMC 0x105 is NOT "L2 disable"
- **Symptom**: SMC returns 0, control readback still 1, and the subsequent jump
  chain breaks (enter_stub's bc=70 never lands; probe never runs; LED stuck blue
  ~2.5 min then reboot).
- **Rule**: 0x105 = the auxcoreboot-addr service (mainline omap-smc.S). The real
  L2 CTRL service is 0x102 — but it is FLAKY mid-flight: only the payload's
  C-flow `mon_call` shape works (4/4); the enter_stub-inline shape hung 9/9.
  See the SMC hang matrix in docs/03.

### Payload SIGSEGV ≠ reboot (2026-09-02)
- **Symptom**: the payload dies with a data abort (e.g. a PRCM write); QNX
  SURVIVES; SSH stays up; the bc page holds the pre-crash state.
- **Rule**: user-mode aborts in the payload are cheap (no reboot). Only
  jump-context deaths (post-GICD-off) reboot. Iterate fast on payload bugs.

### The earlycon0 console writes vanish (2026-09-02, FIXED run 32)
- **Symptom**: the kernel's console output (ring3/UART3) stops after the smoke
  test; the boot log never appears.
- **Cause**: omap4bc.S's addruart uses UART virt 0xFED20000 but head.S never
  mapped section 0xFED — the MMU-on writes hit unmapped VA and the L3
  swallowed them (async error, no abort taken).
- **Fix**: head.S maps 0xFED00000 → 0x48000000 (run 32). Also note ring1's
  alias in omap4bc.S (0xCC000080) is wrong (should be 0xC8000080) — its
  writes are silently dropped; ring3/ring2 work.

## NEW (2026-09-01): early-kernel silent stops
- **Symptom**: kernel markers wander/stall in early start_kernel C (bc 145/168/
  150), same binary gives different points; no abort (trap flag 0), no panic
  (rings clean); device reboots exactly 60 s after the last LED-off.
- **Status**: open. Leading theory = L2/interconnect stall under cacheable WB
  traffic with QNX's 1-cycle data latency. Fix path = monitor-based L2 config.

## NEW (2026-09-01): payload hang in bc_snapshot_file (sync)
- **Symptom**: payload output stops after "probe placed"; bc=40; no "jumping"
  print; ssh times out at 120 s; device may not reboot immediately.
- **Cause**: sync()/eMMC stall (known device flakiness pattern). Transient —
  one retry of the same jump succeeded.
- **Rule**: if a run hangs pre-jump at bc 39-41, just retry the run.

## Device Won't Boot / SSH Unreachable

### Symptoms
- `ssh root@169.254.0.1` times out
- `ping 169.254.0.1` fails
- USB/RNDIS interface not showing

### Checks
1. **USB cable** — must be data cable (not charge-only). Try different cable/port.
2. **Device power** — hold power button 10s to force off, then power on. Wait 3 min for runlevel 2.
3. **RNDIS driver** — host should show `usb0` or `enp0s20f0u1` interface with 169.254.x.x address.
4. **Known host key** — `ssh-keygen -R 169.254.0.1` if key changed.

### Force Power-Cycle
```bash
# On device (if SSH works):
reboot -f
# Or hold power button 10 seconds
```

## Payload Crashes / Device Reboots Unexpectedly

### Symptoms
- `qnx2linux` runs but device reboots before kernel enters
- Breadcrumbs show step < 100 (payload setup phase)
- Post-reboot bc[1] = low value (e.g., 30, 31, 37, 41)

### Causes & Fixes
| bc at Reboot | Phase | Likely Cause | Fix |
|--------------|-------|--------------|-----|
| 30–31 | Buffer placement | DRAM corruption in buffer region | Run memtest; blacklist/swap |
| 37 | CPU1 hold | Reset ctrl write failed | Verify RSTCTRL_CPU1 mapping |
| 32 | L1 clean+inval | CP15 fault | Check System mode (ThreadCtl IO_PRIV) |
| 41 | GICD off | GICD mapping | Verify GICD_CTLR=0x48241000 |
| < 100 | Any setup | WDT2 timeout | Early + late WDT2 kicks (already fixed) |
| 47–54 | Re-verify | DMA corruption | Re-verify catches this; aborts jump |

### Diagnostic
```bash
# Read breadcrumbs after reboot
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 90000000 0x24"
# bc[0]=magic, bc[1]=step, bc[2]=count/errs, bc[3]=nonce, bc[4]=cpu0ticks, bc[5]=magic2
```

## Kernel Hangs at M=1 (bc=122 Ceiling)

### Symptoms
- Breadcrumbs reach 122 (`turn_mmu_on`)
- No further markers
- No abort (bc[1] ≠ 0xAB post-reboot)
- WDT2 resets board
- Probe's M=1 (step 53) survives

### Already Eliminated (Do Not Re-Test)
- Ring-map block in `__create_page_tables`
- DDR vs IRAM table location
- Descriptor cacheability (C/B bits)
- ACTLR.SMP / snoop deadlock
- Stale TLB from QNX
- Cache line fills post-M

### Remaining Debug Avenues
1. **Heartbeat bc in payload setup** — pin whether crash is in payload or kernel
2. **Kernel earlyprintk via DEBUG_LL** — read rings post-reboot:
   ```bash
   ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 88000080 0x20"
   ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 90000080 0x20"
   ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 94000080 0x20"
   ```
3. **Minimal kernel config** — disable SMP, all drivers, modules; static only
4. **Compare T2 vs T3** — T2 jumps to MMU-off blob and survives; T3 enables MMU and dies. Difference = MMU enable.
5. **Instrument kernel** — add `pbmark` in `__turn_mmu_on` (requires kernel rebuild)

## Memtest Reports Errors

### Errors in 24MB Buffer Only
```
VERDICT: DRAM ERRORS total=47
  Region [A4000000]=23, [A8000000]=12, [AC000000]=12
```
**Action**: Add bad regions to `buf_placement_bad` protected list:
```c
// In qnx2linux.c, buf_placement_bad():
static const uint64_t prot[] = {
    0x88000000ull, 0x90000000ull, 0x94000000ull, 0x9FE00000ull,
    0xA4000000ull,  // 64MB bad region
    0xA8000000ull,  // if also bad
    0xAC000000ull,  // if also bad
};
```
Re-run memtest to confirm clean.

### Errors in Canary Pages (0x9FE00000 etc.)
```
canary page 9fe00000
  ERR r1 canary pat=FFFFFFFF pa=9fe00004 got=7FFEFFFF xor=80010001 bits=2
```
**Action**: **Switch to second unit (SanDisk 32GB)**. The bc/ring pages are fixed addresses — cannot blacklist. This confirms the observed bit30 flip was real hardware degradation.

### Errors Everywhere / High Count
```
VERDICT: DRAM ERRORS total=1247
```
**Action**: Unit severely degraded. Swap hardware.

### Intermittent Errors (Round 2 ≠ Round 1)
```
round 1: 0 errs
round 2: 3 errs at pa=A5C00010
```
**Action**: Marginal cells. Soak test: `memtest 10` or `memtest 20`. If persistent → blacklist/swap.

## Build Failures

### "arm-unknown-nto-qnx6.6.0eabi-gcc: command not found"
```bash
source ../qnx-env.sh
# Verify:
which arm-unknown-nto-qnx6.6.0eabi-gcc
```

### Assembly Errors (stub3.S, probe.S)
- Check `.arch armv7-a` and `.arch_extension sec` directives
- Verify symbol names match C externs (`tramp_pos_start`, `cont_start`, etc.)
- Ensure `.global` exports

### Linker Errors (undefined reference)
- `clean_inval_l1_all` — defined in cacheops.S, linked in build.sh
- `mon_call_full` — defined in qnx2linux.c as extern, implemented in... check stub3.S?
- `read_ccsidr_decode` — same

## SSH / SCP Failures

### "Permission denied (publickey)"
```bash
# Verify key exists
ls -la ../rsa ../rsa.pub
# Fix permissions
chmod 600 ../rsa
chmod 644 ../rsa.pub
# Re-copy authorized_keys on device (if accessible)
```

### "Connection refused" / "No route to host"
- Device not at runlevel 2 yet — wait 3 min after boot
- USB cable unplugged
- RNDIS interface down on host: `ip link show` → check `usb0` up

### SCP "stalled" / slow
- Normal for large files over RNDIS (12 Mbps theoretical)
- `qnx2linux` ~20 KB, `zImage` ~8 MB — expect 10-30s

## "random death points" in Early head.S (Fixed)

### Old Symptoms (Pre-2026-08-31)
Deaths at bc 102, 107, 115, 119 — varying per run

### Root Cause
`do_t3` never kicked WDT2 during 40-60s setup. `wdtkick` daemon's 15s cycle had random remainder.

### Fix Applied
```c
// In do_t3, after bc_armed:
wdt2_kick();  // Early — covers setup

// In do_t3, after GICD off (bare register, no kernel calls):
{ uint32_t g = wdt[0x30/4]; wdt[0x30/4] = ~g; }  // Late — covers kernel
```

### Verify Fix
Run should now consistently reach bc=122 (M=1 hang) or further, not random early deaths.

## Earlyprintk Abort Pre-paging_init (Fixed)

### Old Symptoms
Kernel abort before `paging_init`, no breadcrumbs past early boot

### Root Cause
Kernel's `earlyprintk` banner flush (`CON_PRINTBUFFER`) runs **before** `paging_init`, hits unmapped UART/PL310 VAs.

### Fix Applied
Rebased debug VAs to section-compatible addresses:
- UART: 0xFEB20000 → 0xFED20000 (section 0xFED)
- PL310: 0xFEB21000 → 0xFEB42000 (section 0xFEB)
- head.S early-maps PL310 section: 0xFEB00000 → 0x48200000
- `debug_ll_io_init` re-maps same VAs later

## bc Readback Garbage (Fixed)

### Old Symptoms
bc markers inconsistent; readbacks didn't match writes

### Root Cause
`pbmark`/`pbmark3`/`pbmarkv` macros missing `dsb` before PL310 clean+invalidate-by-PA. PL310 TRM requires DSB.

### Fix Applied
All three marker macros now `dsb` before CIPA write.

---

## Emergency Recovery

### Device Bricked / Won't Boot At All
1. **Force power-off**: Hold power 10s
2. **Wait 30s**, power on
3. **Wait 3 min** for SSH
4. If still dead: hardware issue (battery, PMIC, eMMC)

### QNX Corrupted
- Boot ROM loads IFS from eMMC partition
- If IFS corrupted: need JTAG or eMMC reprogram (not covered here)
- Our payload runs from QNX userspace — requires working QNX

### Lost SSH Key
- If `../rsa` lost: cannot SSH
- Recovery: need console access (UART3 on dock connector?) or JTAG
- **Always back up `rsa` and `rsa.pub`**

---

## Debugging Checklist Per Run

### Before Jump
- [ ] Device power-cycled (if memtest or after instability)
- [ ] SSH up, `pidin mem` shows DRAM regions
- [ ] Display stack quiesced (`slay splash backlight_win screen`)
- [ ] jump.sh timeout 120s
- [ ] Early WDT2 kick in payload

### After Reboot
- [ ] Read bc primary (0x90000000)
- [ ] Read all 3 mirrors (0x94/0x88/0x9FE)
- [ ] Read ring1/2/3 (0x88/0x90/0x94 + 0x80)
- [ ] Check bc[1] for step, bc[2] for count/errors, bc[4] for cpu0 ticks

### If Hang
- [ ] bc[1] = ? (122 = M=1; <100 = payload; 0xAB = abort)
- [ ] Ring chars show kernel earlyprintk?
- [ ] Abort trap fired? (bc[1] == 0xAB)

---

## Common Misconceptions

| Misconception | Reality |
|---------------|---------|
| "Payload crash = kernel bug" | Payload crash → procnto dies → WDT2 reboot. Kernel never runs. |
| "bc pages always clean" | QNX reboots churn 0x900000A0; bc[1] flag moved to bc[1] itself (0x90000004) |
| "Uncompressed Image works" | Architecturally broken — PHYS_OFFSET derivation fails at arbitrary load |
| "ACTLR.SMP causes hang" | Cleared ACTLR.SMP — still hangs. QNX ACTLR=0x41 benign. |
| "TLB stale from QNX" | zImage decompressor does TLBIALL×6 — no QNX entries survive |
| "Can test DRAM with dd" | `dd if=/dev/mem` with skip doesn't seek — use `memtest` or `memdump3` |