# Debugging Session Records & Findings (2026-08-30 through 2026-08-31)

## Session 2026-08-30 (Live Device Work)
**Reference**: `SESSION-HANDOFF/PlayBook_Bootchain_RE_Session_Handoff/KEXEC_SESSION_LOG_2026-08-30_live_device_work`

### Key Findings
- T2 kexec implementation working: jump buffer, flat page table @ IRAM 0x40304000, trampoline @ buffer+0x800/+0x830, breadcrumbs @ 0x9FE00000
- Boot timing: 2–3 min to runlevel 2/SSH; wait ~3 min before assuming hang
- Per-unit eMMC variants: Samsung MCGAFA 64GB vs SanDisk SEM32G 32GB
- DRAM notes: jump region 0x84000000+ is safe at runlevel-0

## Session 2026-08-31 Evening (17-Run Marathon)
**Reference**: `SESSION-HANDOFF/SESSION-RECORD_2026-08-31_evening_kexec-T3.md`

### The Blockers
**Kernel dies silently at M=1 (MMU enable)** — bc=122 ceiling (`turn_mmu_on` entered, never returns).
- No abort, no further markers, WDT2 resets
- Probe's own M=1 (step 53) SURVIVES — executed from IRAM with flat table, uncached descriptors

### 7 Verified Fixes
1. **bc marker race**: `pbmark` stores missing DSB before PL310 clean+invalidate-by-PA (TRM requires it). All three macros (`pbmark`/`pbmark3`/`pbmarkv`) now `dsb` before CIPA. Readbacks trustworthy since.
2. **do_t3 never kicked WDT2**: Setup (file reads + 24MB NOCACHE copies + verify) raced wdtkick's random 15s remainder → random device resets during early head.S ("random death points" at 102/107/115/119). Now: early kick at bc-armed + late bare-register kick before enter_stub.
3. **jump.sh timeout 30s killed payload mid-setup** (SIGHUP on SSH timeout = "silent payload deaths"). Timeout now 120s. Payload setup takes ~40-60s with re-verify.
4. **earlyprintk abort pre-paging_init**: Kernel's earlyprintk banner flush (CON_PRINTBUFFER, parse_early_param) hit unmapped UART/PL310 VAs → abort. Fixed by rebasing debug VAs: UART 0xFEB20000→0xFED20000, PL310 0xFEB21000→0xFEB42000; head.S early-maps PL310 section (0xFEB00000→0x48200000).
5. **Pre-jump re-verify** in do_t3 (kernel/DTB/tramp/cont/probe/table through NOCACHE/IRAM views) — aborts jump on corruption.
6. **Display-stack quiesce** added to jump.sh (`slay splash backlight_win screen`).
7. **Abort trap**: cpt maps VA 0x00000000 + 0xFFFF0000 → PA 0x9FE00000 (4th mirror); handler writes 0xAB to bc[1] (VA 0xD00000A0), PL310 flushes, spins. Flag moved to bc[1] itself (0x90000004) after QNX reboots churned 0x900000A0.

### 6 Theories KILLED (with evidence)
| Theory | Evidence |
|--------|----------|
| Ring-map block in `__create_page_tables` | INNOCENT — marker 115 landed with it |
| DDR vs IRAM table location | Both hang at M=1 — walk path location innocent |
| Cacheable vs uncached early descriptors | Both hang — linefill-snoop via descriptor attrs innocent |
| ACTLR.SMP/snoop-deadlock | Cleared ACTLR.SMP in `__enable_mmu` — still hangs. QNX ACTLR=0x41 (SMP|broadcast) benign; diag=0x810 (errata bits) |
| Stale-TLB | DEAD — zImage decompressor issues TLBIALL×6 before kernel runs (compressed/head.S) |
| Cache fills | C=I=0 post-M still hangs → fills innocent |

### Prime Suspect: DRAM Integrity
- **Observed on-device**: DTB magic 0xedfe0dd0 in bc page read back as 0xadfe0dd0 — **single-bit flip, bit30**
- Explains: placement-dependent kernel/image/table corruption, silent wild-execution hangs, run-to-run variance, probe/cont surviving (they run from IRAM), "determinism per placement"
- Payload intermittent silent deaths during file reads/copies + RNDIS SSH timeouts without reboots = device (this 64GB unit) degrading/flaky

### Marker Ladder (Current Build)
```
100 stext · 101 · 105/106 smoke · 107 addruart · 108 senduart · 109 busyuart
102 lookup · 103 atags · 107 fixup_smp · 108 fixup_pv · 114 ring-block
115 cpt done · 119 cpt-returned · 121 enable_mmu · 122 turn_mmu_on
124 first post-M store (VA 0xD0000004) · 123 post-M flushed
120 mmap_switched · 117 mmap_switched end · 118 start_kernel
110 post-paging_init · 111 post-setup_arch
Flag: bc[1]=0xAB = abort handler fired post-M
```

### zImage Pivot (Decided Tonight)
- Uncompressed Image at arbitrary address = architecturally broken (PHYS_OFFSET derivation)
- zImage decompresses to zreladdr 0x80008000, PHYS_OFFSET=0x80000000, consistent
- zImage reached bc=119/122 (decompressor works; kernel enters)

### Ops Notes
- Payload setup ≈ 40-60s with re-verify; jump.sh timeout 120s
- Early WDT2 kick (bc-armed) + late bare-register kick (pre-enter_stub)
- QNX ACTLR=0x41, diag=0x810 (captured; benign)
- Payload crash takes device down (procnto dies → WDT2 → reboot — confirmed). bc=30 post-reboot = surviving pre-crash write.
- Deaths cluster in file-read/copy phase (after early WDT2 kick, before/during memtest+copies). Prime suspect: RAM corruption or driver interaction (devb 10MB cache? live DMA) in placed buffer.
- USB/RNDIS timeouts observed = WDT2 reboots themselves.

## Session 2026-09-01 (Memtest Payload Built)
**Reference**: `SESSION-HANDOFF/SESSION-RECORD_2026-09-01_memtest-payload_handoff.md`

### Action
Built standalone memtest payload (`memtest.c`, `memtest` binary) per mission:
- 24MB buffer sweep via NOCACHE (A5/5A/FF/00), all mismatches reported with PA
- Canary sweep of 4 bc/ring pages (0x88/0x90/0x94/0x9FE) — observed flip was in 0x9FE00000
- 2 rounds default to catch intermittents
- Added to build.sh

### Result (executed same day)
**CLEAN — 0 errors** (2 rounds + 4 canary pages). DRAM ruled out.

---

## Session 2026-09-01 Evening (M=1 WALL BROKEN — ring-map bug fixed)
**Reference**: `SESSION-HANDOFF/SESSION-RECORD_2026-09-01_ring-map-fix_early-C-wedge.md`
Run-by-run narrative, dead-end reasoning chains, and micro-decodes (the
'K'/leading-NUL ring decode, the 0x60540100 probe residue, the 0x5a15 ring3
header decode, the pre-sweep placement rejections, the invalid-constant
assembler detour): `newdocs/session-notes/session-04.md`.

### The breakthrough
**Root cause of the 122-ceiling found and fixed**: head.S PlayBook ring-map block
shifted PA>>12 values (`0x88000` etc.) with `lsl #SECTION_SHIFT` (=20) instead of
`lsl #12` → shifted values truncated to 0 → rings/PL310/trap-vectors aliased to
PA 0x00000000 (boot ROM) / PA 0x20000000. Post-M markers lost, CIPA hit GPMC,
sync-poll spun, trap vectors pointed at ROM ("no abort" was an artifact).
Fixed with `lsl #12`; verified in rebuilt vmlinux disassembly. Kernel now reaches
**start_kernel C: bc 145/168/150** (zImage path, probe-verified chain).

### New diagnostics built this session
- Heartbeat markers 42-50 in do_t3 setup phase (payload setup proven clean)
- Flush-free breadcrumb channel proven (SO stores reach DRAM, no CIPA/SMC);
  all fine markers converted to flush-free (pb_bc v6)
- Panic notifier → "PB-PANIC: msg" into the rings via printascii (pre-console)
- Fine start_kernel ladder: 118→150→151→142→144→145→143→140→141→136→130-135→110→111
- cgroup_init_early bisect markers (160/161/0x100+i/0x120+i/164-170)

### The new front: random silent stops in early start_kernel C
- Same binary wanders 145/168/150 — NOT deterministic
- No abort (trap now functional, flag 0 every run), no panic (rings clean)
- DISPC scanout blanked pre-jump → wedge unchanged (ruled out)
- ACTLR at kernel entry = 0x1 (SMP bit 6 already clear) → broadcast ruled out
- WDT2 window = **60 s exact** (user-timed twice) — NOT 15 s

### PL310 secure-filtering discoveries (all on-device)
- r3p2 (0x410000C4); control=1, aux=0x1E070000, tag-lat=0, **data-lat=0x111 (1-cycle!)**,
  prefetch=0x5
- NS writes: control → **bus hang**; data-latency → **SIGBUS (fltno=5)**;
  by-way 0x7FC → **background-op deadlock** (fired twice); NS reads = fine
- Monitor SMC 0x105: returned 0, did NOT disable, **broke the jump chain**
  (enter_stub never ran, probe never ran) — reverted
- Kernel rebuilt with **CONFIG_CACHE_L2X0=n** (never touches PL310 config)

### Leading theory
Kernel early C = first sustained cacheable WB traffic through QNX's marginal
L2 config (1-cycle latency) → random interconnect/eviction stalls → silent CPU
stall until the 60s WDT2. Also plausibly the cause of QNX's historic flakiness
(RNDIS timeouts, sync() hangs).

### Next (in order)
1. RE monitor L2 service IDs: unpack `trustzone-omap4` (QNX-packed — dump from
   live device via /proc/<pid>/as), find SMC dispatch (r12 constants, 0x100 range)
2. Monitor-based L2 reconfig/disable pre-jump; verify via NS readbacks
3. Re-jump the no-L2X0 kernel; if wedge gone → re-enable L2 properly later

### Eliminated today (do not re-test)
- DRAM corruption (memtest clean)
- DISPC scanout contention
- ACTLR.SMP broadcast (already 0 at entry)
- Monitor 0x105 as "L2 disable"
- NS PL310 control/latency writes (secure-filtered)
- NS by-way clean+inv (deadlock)

## Session 2026-09-01/02 Late (L2 THEORY CONFIRMED bare-metal + monitor RE + SMC hang matrix)
**Reference**: `SESSION-HANDOFF/SESSION-RECORD_2026-09-02_L2-confirmed_monitor-RE.md`

### HEADLINE
**The early-C wedge is THE MACHINE, not the kernel.** The probe.S long-loop
diagnostic (run 10) stalled silently at pass ~11000/16384 (~11 GB of pure CPU
cacheable WB stores, no kernel/QNX/monitor/DMA involved) with PL310 control=1
and data-latency=0x111 — no abort, rings clean, WDT2 60 s later. The kernel's
early-C deaths (bc 145/168/150) are the same phenomenon: first sustained WB
traffic through QNX's marginal L2 configuration. Also explains QNX's historic
flakiness AND the monitor's own flaky 0x102 disable.

### Monitor service table (RE'd — authoritative, from mainline omap-secure.h +
### omap-smc.S + sleep44xx.S; trustzone-omap4 has NO SMC — it's the secure-
### debug/crypto resmgr; the SMC services live in the TI ROM monitor)
SMC #0, r12=service: 0x100 L2X0 DBG_CTRL w · **0x101 L2 clean+inv by PA
(r0=PA, r1=size — verified)** · **0x102 L2X0 CTRL write** · 0x103/0x104/0x105
auxcoreboot read/modify/addr (0x105 mystery SOLVED — not L2-related) ·
0x108 SCU_PWR w · 0x109 L2X0 AUXCTRL w · 0x113 L2X0 PREFETCH w.
**No tag/data-latency service exists** — the 0x333-latency plan is impossible
via SMC. GP/HS: 0x4A0022C4 = 0xae8 → HS device confirmed.

### SMC 0x102 disable — hang matrix (r12=0x102, r0=0)
| Run | Context | Result |
|-----|---------|--------|
| 2 | pre-hold, GICD on, IRQs on, MMU on, r0/r1=0 (mon_call) | returned, ctrl=0 |
| 3 | post-hold, GICD on, IRQs on, MMU on (mon_call) | returned, ctrl=0 |
| 4 | cont, MMU OFF | never returns |
| 5 | enter_stub, GICD off, cpsid if, garbage r1 | never returns |
| 6/7 | enter_stub, GICD on, IRQs on; r1 garbage (6) / zeroed (7); GICD write via unmapped PA-as-VA (my bug, both) | died before bc=70 |
| 8/9 | enter_stub, perfect context, r0-r3=0, mirror markers 70/71 | **hangs IN the SMC (70 without 71)** |
Verdict: 0x102 is nondeterministically unsafe mid-flight (~2/7 returns) —
secure-side background op racing foreground traffic (727915 class). Do NOT
retry blind. The disable is still the right fix; needs a reliable path
(pre-drain via 0x101 over DRAM, or PPA L2 POR service, or controlled retry).

### Other findings (runs 1-9)
- "QNX-packed" trustzone-omap4 was never compressed — LOAD segment is plain
  ARM code; the section table is just stripped. Disassembled fully: zero SMC.
- jump.sh hardening: payload now launched DETACHED (>/tmp/jump.log &, no
  SIGHUP death — the 120 s timeout killed it in run 1), live bc polling with
  transient-SSH tolerance, mirror readbacks (mirror0 carries enter_stub 70/71).
- Kernel: ALL CIPA sites guarded on PL310 control (skip flush when CTRL=0 —
  bare stores are DRAM-durable with the L2 off): head.S pbmark/pbmark3/
  pbmarkv + 2 hand-rolled __turn_mmu_on blocks + main.c 118/111 + setup.c 110.
  Neutral when L2 on. KEEP for the L2-off runs.
- GAS quirk: a numeric label on the SAME line as .endm inside a macro =
  "unexpected end of file in macro definition". Put .endm on its own line.
- pmaports omap kernel config (reference for L2-on endgame):
  CONFIG_CACHE_L2X0=y + PL310_ERRATA_588369 + PL310_ERRATA_727915 +
  ARM_L1_CACHE_SHIFT_6 + ARM_ERRATA_798181 + SMP/SMP_ON_UP. No PlayBook
  port exists in pmaports (wiki codename only) — we are first.
- droid4-kexecboot (same SoC, TI maintainer): kexecboot-as-bootloader +
  mainline kexec daily; "stock 3.0.8 kernel does not survive kexec" —
  mainline does. Different chain (flashed boot partition) — nothing to port.

### Next (in order)
1. Reliable L2 disable: pre-drain the whole L2 by-PA via 0x101 (foreground,
   proven) before 0x102, and/or accept retry-until-works (each failure =
   one WDT2 reboot, ~2-3 min). Verify via probe bc[8]=0/bc[9]=0x111.
2. Re-jump no-L2X0 kernel with L2 OFF → early-C wedge should VANISH
   (bc past 143→136→130→110→111). Then decide L2-on strategy (L1-only may
   be acceptable short-term; monitor has no latency service).
3. Alternative if SMC proves unusable: never disable — instead accept the
   flakiness rate and target the kernel's own tolerance (retry boots).

### Eliminated this round (do not re-test)
- Monitor 0x102 "context sensitivity" (runs 5-9: no context makes it reliable)
- r1-garbage theory (runs 8/9 zeroed all args, still hung)
- GICD-off/cpsid-if as SMC-hang factors (run 9: GICD on, IRQs on, still hung)

## Session 2026-09-02 Late (runs 23–32: L2 disable WORKS via C-flow; the 171 wall; the observability overhaul)
**Reference**: `SESSION-HANDOFF/HANDOFF_2026-09-02_late_171-wall_new-observability.md`
**Kernels**: #51 (run 23) → #52+ (runs 24–32)

### HEADLINE
**The L2 disable works (C-flow mon_call shape, 4/4) and the L2-off kernel boots
through start_kernel, setup_arch, paging_init, mm_init, cgroup_init — dying
deterministically at bc[1]=171 (post cgroup_init), inside taskstats_init_early →
kmem_cache_create (before the SLUB mutex).** 6+ consecutive runs at exactly 171.
bc[2]=0x3E7 (999) co-occurs — an unexplained wild write (NOT the pb_bc pair,
which would be bc[2]=427).

### The three session bugs/fixes
1. **WDT2 kicks were no-ops (FIXED run 30)**: the head.S 0xFEC section (WDT2)
   had a PA-encoding bug — the copied 0xFEB pattern's `orr #0x200` carried PA
   bits, decoding the section to PA 0x4A500000 instead of 0x4A300000. Every
   VA-based kick (pb_bc/pbmarkv/PB_MMU_BC) was a no-op → the "died at marker X"
   runs 17–29 were the 58.6 s window expiring wherever the kernel was. After
   the fix (run 30): the kicks work, the kernel is unbounded — and the death at
   171 became REAL.
2. **Console output dead since run 24 (FIXED run 32, first retest pending)**:
   omap4bc.S's addruart returns UART virt 0xFED20000, claiming head.S
   early-maps section 0xFED — it never did. Every MMU-on console write hit
   unmapped VA 0xFED20014; the L3 swallowed it (async error, no abort) →
   silent. head.S now maps 0xFED00000 → 0x48000000.
3. **Payload SIGSEGV ≠ reboot (run 31)**: the auto-idle fix's first PRCM
   CLKSTCTRL write (CM2 CORE L3_1 @ 0x4A008700) took a data abort — the PRCM
   is secure-filtered from NS — and **QNX survived** (SSH stayed up, the
   WDT daemon kept kicking). Payload user-mode crashes are cheap now. The
   auto-idle block was removed; the PRCM needs the monitor's PPA services.

### The run map (23–32)
| Run | Kernel | Change | Result |
|-----|--------|--------|--------|
| 23 | #51 | L2 disable C-flow + WDT-kick attempt (broken PA) | bc=171; ring3 captured its 23 KB console log (the only console-emitting kernel) |
| 24–29 | #52+ | markers 171-187, 0xFEB-skip, WDT-kick attempts (broken PA) | ALL bc=171 (kicks = no-ops; 58.6 s expiry at the same deterministic spot) |
| 30 | #52+ | WDT PA fixed | STILL 171 → the 171 death is REAL |
| 31 | #52+ | + PRCM auto-idle fix (NS CLKSTCTRL write) | payload SIGSEGV at bc=50; QNX survived (no reboot!) |
| 32 | #52+ | − auto-idle block, + 0xFED UART section | bc=171; ring3 still frozen (console output still not reaching it) |

### The new observability model
- **ring3 (PA 0x94000100, count @0x94000080, index @0x94000084) = the secure
  monitor's UART3 capture buffer.** The monitor taps UART3's TX and appends
  every byte; the buffer is persistent across WDT2 resets (secure-side
  bookkeeping) and CANNOT be cleared by the payload (zeroing the headers gets
  re-written on the next capture). The count = cumulative UART3 bytes.
- run 23's kernel emitted 23 KB of console output (its full boot log through
  the "dram barrier" print + a partial "r." line) — preserved in ring3 and
  recovered to SESSION-HANDOFF/ring3-recovered-log-2026-09-02.txt.
- runs 24–32's kernels emitted ZERO UART3 bytes (the count froze at 23061) —
  the console was dead (bug #2). With run 32's 0xFED fix the next run's ring3
  tail should show the current kernel's console output up to its death —
  **the primary readback channel now**.
- early_printk.c also mirrors console output into ring3 via the 0xD4000100
  alias (bounded index at 0xD4000084) — that index froze at 20, confirming
  the console emitted nothing since run 23.
- **UART pad hunt: DEAD** — 2 hours, no accessible pad found. The monitor's
  capture replaces the UART-pad plan entirely.

### The watchdog truth (from the live registers + omap_wdt.h)
- TGR 0x30 (trigger: ANY write reloads CRR from LDR — value ignored),
  SPR 0x48 (start/stop: 0xBBBB/0x4444 = start, 0xAAAA/0x5555 = stop),
  CRR 0x28, LDR 0x2C, CLR 0x24 (prescaler).
- LDR = 0xFFE2B400 → **58.6 s** @ 32.768 kHz, PTV=0 (the "60 s" was approximate).
- QNX's wdtkick daemon: `omap4430-wdtkick -t 15000` (15 s period) — the
  fallback kicker. After the jump, the kernel's per-marker/per-initcall kicks
  are the only thing keeping the device alive.

### The 171 wall — what we know
- The death is DETERMINISTIC (6+ runs at exactly 171) and happens with
  working kicks (run 30+) — it is NOT the watchdog.
- The window: pb_bc(171) → taskstats_init_early() → KMEM_CACHE(taskstats,
  SLAB_PANIC) → kmem_cache_create → [SLUB markers 183/184/185 never land]
  → death. Also inside this window: the mystery bc[2]=0x3E7 (999) write
  (NOT the pb_bc pair, which would be bc[2]=427 — a wild write co-occurring
  with the death).
- Fine markers 176 (post taskstats) NEVER land → the death is inside
  taskstats_init_early, before its post-KMEM_CACHE marker → i.e. inside
  kmem_cache_create itself, before the SLUB mutex unlock (marker 183 is
  after mutex_lock and never lands either — the death is at/before the
  mutex_lock completes).
- Leading hypothesis: slab_mutex appears contended (corrupted lock word —
  the machine's silent-corruption class) → mutex slowpath → schedule() with
  IRQs disabled and no scheduler → eternal silent stall → WDT2 58.6 s later.
- Alternative: a hard stall inside the SLUB merge scan (find_mergeable walks
  slab_caches — a corrupted list node = infinite loop).
- The run 23 kernel (#51) passed this exact code (23 KB of console output
  exists past it) — the deltas between #51 and #52+ are the marker/kick
  additions themselves, OR the deterministic corruption needs the exact
  memory layout (phys placement varies per run — the corruption may be
  placement-dependent!).

### Next (in order)
1. **Console visibility test**: add bc[10] markers inside setup_early_printk
   (registration) and early_write (first call) — one run answers whether the
   console registers/outputs at all post-0xFED-fix.
2. **Read ring3's tail after every run** — the console log is the primary
   evidence (the monitor captures UART3 regardless of the DRAM rings).
3. **The 171 wall bisect**: (a) replace KMEM_CACHE(taskstats, SLAB_PANIC)
   with a kzalloc stub to skip the create; (b) dump the slab_mutex word into
   bc before the lock; (c) if the merge scan is the loop, bound it.
4. **Re-test with run-23's memory placement** — the corruption may be
   placement-dependent (phys varies per run; run 23's placement worked).
5. **L2-on endgame later**: pmOS config reference (docs/03 §pmaports) —
   CONFIG_CACHE_L2X0=y + the PL310 errata, once the boot is stable.

### Eliminated this round (do not re-test)
- Direct NS writes to the PRCM (CM1/CM2 CLKSTCTRL) — secure-filtered, write
  = SIGSEGV (run 31). The auto-idle fix needs the monitor's PPA services.
- The WDT2 kick register: TGR 0x30 = trigger (any write reloads) — the
  payload's `~WTGR` complement-write was always VALID; the broken piece was
  the head.S section PA (fixed).
- "The kernel dies from the watchdog" (runs 30+): the kicks work; the 171
  death is real.

---

## Summary of Eliminated Theories (Do Not Re-Test)
1. Ring-map block in `__create_page_tables` — **was in fact THE M=1 bug (shift
   truncation), now FIXED**; the old "marker 115 landed with it" test was
   meaningless (115 is pre-MMU)
2. DDR vs IRAM table location
3. Descriptor cacheability attributes
4. ACTLR.SMP / snoop deadlock (SMP bit already 0 at kernel entry)
5. Stale TLB entries from QNX
6. Cache line fills post-MMU-on
7. DRAM corruption (memtest clean, 2026-09-01)
8. DISPC scanout contention (blanked, wedge unchanged)
9. Monitor SMC 0x105 as "L2 disable" (wrong semantics, breaks jump)
10. NS writes to PL310 control/aux/latency (secure-filtered: hang/SIGBUS)
11. NS by-way clean+inv 0x7FC (background-op deadlock)

## Current Hypothesis Priority (2026-09-02 late)
1. **The 171 wall** (prime): deterministic death in taskstats_init_early →
   kmem_cache_create, before the SLUB mutex completes. Suspects: corrupted
   slab state (silent lost-writeback), SLUB merge-scan list cycle, or a
   placement-dependent corruption. Bisected to a single function — now needs
   console visibility + the SLUB-internal markers.
2. **Silent lost-writebacks on the L2-off bypass path** (the corruption
   source theory): the bare-metal long-loop stall (L2 ON, 11 GB) proved the
   machine can stall under WB traffic; the L2-off bypass may lose/corrupt
   transactions at a lower rate. Explains: the wild bc[2]=999 write, the
   deterministic-but-placement-sensitive deaths, the historic single-bit flip.
3. **L3/EMIF auto-idle** (the corruption *source* candidate): auto-idle
   transitions losing in-flight transactions. NOT fixable via NS (PRCM is
   secure-filtered, run 31) — needs the monitor's PPA clock-domain service.
4. Other concurrent DMA masters (eMMC ADMA, WiFi SDIO) — untested; the
   controllers are idle-but-enabled post-jump.

## Summary of Eliminated Theories (Do Not Re-Test)
## 2026-09-02 Evening (runs 33-50: L2-on pivot, console breakthrough, pv-fixup wall)

The session pivoted hard. Console-visibility test (task 1) cascaded into a
complete rework of the boot's memory/cache posture. Run map (all --l2on mode
unless noted; kernel #77-#80):

- Run A (console test): handler never ran (bc[10]=0xaa), boot to 171.
- Run B (hard reboot, parse probes bc[10-13]): handler RAN, died inside the
  first console write (bc[1]=132, ring1 count=20). cmdline len 73.
- Payload --l2test (x3): phase A (L2 on, device stores) clean; phase C (L2
  off) WEDGED the machine in the pattern loops -> **the L2-off bypass path
  cannot sustain device traffic. The 171 wall = the same cliff.** Payload
  fixup: GICD-off/CPU1-hold recipe; 0x102 with GICD live = hang (known).
- L2-on runs (strip still in): bc[1]=120 forever — head.S stripped C/B/S so
  the kernel ran UNCACHED; the bss clear wedged the L2. SMP-clear removal
  did NOT fix it. **ROOT CAUSE: the C/B/S strip (M=1-era diagnostic, never
  reverted).**
- Strip removed + device sections (r6): boot reached 133 (parse done,
  console registered, cmdline 102 forced) then died at the first UART3
  write — posted store to a dead/gated UART3 (L4PER auto-idle) stalls the
  store buffer; the next ring load hangs. **Fix: rings-only console
  (senduart/LSR drain removed from omap4bc.S).**
- Rings-only: 133 -> arm_memblock_init panic "Failed to steal 0x00100000 at
  omap_secure_ram_reserve_memblock" — memblock.current_limit=0
  (adjust_lowmem_bounds: vmalloc_limit(0x30000000) <= region base
  0xa0000000; one run had a corrupted pv-patch making it sane). Fixes:
  forced limit in arm_memblock_init + PB-ADJ/PB-MEM prints.
- Rings-only + limit fix: boot to 127 (map_kernel done) — dma_contiguous_
  remap/early_fixmap: **BUG: not creating mapping for 0xa1000000 at
  0x1f7f0000 in user region** (dram barrier remap; __va delta = broken
  pv-patched PHYS_OFFSET ~0x41810000 while the trim's __pa printed sane
  0xa0000000). Reproducible x2. **CURRENT WALL.**
- One run regressed to 132/ring=70: the setup_arch LED-off block had the
  LAST UNBOUNDED PL310 sync poll — replaced (pb_bc_put). LED-on block too.
  **ALL flushes now go through pb_smc_flush = monitor SMC 0x101 (C-flow,
  synchronous, no polls). Rings are CACHEABLE (r7); flushes are batched
  per console chunk (pb_flush_rings).**
- Observability now: ring1 = full boot log (3840-char window, word-reversed
  in memdump3 output), bc ladder via pb_bc_put (SMC-flushed), PB-PANIC
  notifier works, LED GPIO1_13 (0x194=SETDATAOUT, 0x190=CLEARDATAOUT) = a
   FAN5702 indicator-LED enable, NOT the backlight (user-confirmed).

Key discoveries/rules:
1. L2-off is DEAD as a mode: bypass path wedges under sustained traffic.
   --l2on is the mode; kernel-side L2X0 driver = the endgame.
2. Sustained DEVICE (SO) stores wedge with L2 on (ring writes per char!).
   Cacheable stores + batched SMC flush = the pattern.
3. UART3 is dead post-idle (L4PER auto-idle): never write it from the
   kernel. The monitor's ring3 capture needs UART TX -> currently unused;
   the DRAM rings are the console.
4. Unbounded sync polls are forbidden (boot-wide audit done).
5. memdump3 output words are BIG-ENDIAN-formatted; decode by reversing each
   4-byte group. Ring windows: base+0x80..base+0xF80 (3840 chars).
6. Display confound: payload "display quiesced" does NOT blank the screen;
   DISPC scanout continues (contention through the whole boot). Confirm
   screen state at quiesce on every run.
7. CONFIG_CMDLINE_FORCE=y is set (earlyprintk etc. always present).

### Late-night addendum (the final runs of session 6)

- --l2lat proved the corruption source: PL310 data latency = 0x111 (1/1/1
  cycles) live; the NS write = SIGBUS (secure-filtered). No monitor service
  for the latencies in the RE'd table. The corruption root = secure-domain
  config (latencies and/or L3/EMIF auto-idle) -> fix = RE the monitor.
- Two consecutive runs: deterministic BUG at dma_contiguous_remap —
  dma_mmu_remap[0].base corrupted 0xbe800000 -> 0xa1000000 — with L2 ON
  (SMC-flush era) AND L2 OFF (normal mode). The corruption = the
  secure-domain issue, present in every mode.
- omap_barriers_init DISABLED (#if 0 in omap4-common.c) — the barrier stack
  struct was corrupted and the unmapped dram_sync write aborted.
- CONFIG_CACHE_L2X0=y + the pl310 DTS node added. WARNING: the driver's NS
  latency write = SIGBUS on this machine — guard cache-l2x0.c before the
  boot gets past the current wall.
- The full handoff: SESSION-HANDOFF/HANDOFF_2026-09-02_night_SMC-flush-pv-wall.md

## Session 2026-09-03 (session 7: monitor RE complete — NO latency service exists; PPA probe built)

No device runs this stretch; the full static RE of every QNX binary with an
SMC or PL310 footprint, plus the payload --ppa probe (built, not yet run).

### The monitor RE — the task from the handoff, now CLOSED

Every candidate binary disassembled end to end (capstone+pyelftools — the box
has no ARM objdump; the QNX ELFs are "architecture UNKNOWN" to binutils):

- **trustzone-omap4** (61 KB): it is the `/dev/trustzone` resource manager —
  a secure-CRYPTO resmgr (ECDH/secp521r1/SHA512/RPMB/KEK; strings confirm),
  one SMC site (0x7c4c, the r6=0xFF/ip=0 shape), maps PRCM (wkupCtrlVirtBase)
  — but NO L2/PL310 code and NO dispatch table. Confirms the session-01/02
  verdict: the SMC services live in the TI ROM monitor, not in any QNX binary.
- **libsecure_dispatcher-omap4.so.1**: crypto/KDS library (aes_oneshot,
  hmac_oneshot, rng_hwRNGen, kds_set_kek_sw/kds_select_kek/kds_set_dek) —
  talks to procnto via MsgSendv/devctl; ZERO SMCs, zero coprocessor ops.
  Re-disassembled properly (old .dis was 0 bytes; lsd.dis = a raw text dump).
- **procnto** (143k lines): ZERO SMCs, ZERO PL310 accesses (no 0x48242000
  constants anywhere). The three `mov ip, #0x104` hits are stack-buffer data,
  not SMCs. QNX NS never programs the L2 — its config comes from the secure
  boot chain (monitor), consistent with the secure-filtering SIGBUS/SIGSEGV.
- **devpm-omap4** = the ONLY NS SMC client on the box. Its suspend path:
  - 3× `SCU_PWR 0x108` calls (r0=0/r1=0xFF enter, r0=3/r1=0, r0=0/r1=0
    post-WFI) — the exact mainline sleep44xx.S omap4 suspend shape;
  - 2× **PPA calls, services 0x26 and 0x27** — NOT in mainline's PPA list
    (0x21 SVC_0 / 0x23 L2_POR / 0x25 CPU_ACTRL_SMP). Shape (devpm 0xbf9c/
    0xbfc8): r0=idx, r1=0 (process), r2=4 (flag), r3=pargs pointer, r6=0xFF,
    ip=r12=0, BPISI, dsb, smc #0. pargs = one word = 1. Called at OSWR entry.

**Verdict**: no tag/data-latency SMC exists — the 0x100-0x113 table (docs,
2026-09-01/02) stands as-is and is now known to be complete for the monitor.
The remaining unexplored secure surface = the **PPA services** (0x21/0x23/
0x25 + devpm's 0x26/0x27). PPA 0x23 L2_POR = a full SECURE-side L2 re-init —
if anything can fix the QNX L2 config (latencies or worse), it's this; and
0x26/0x27 (a suspend-pair in the PM driver) could be the L3/EMIF auto-idle
controls. Both testable from the payload with readbacks.

### cache-l2x0.c audit (the SIGBUS warning is superseded; a REAL hazard found)

- The DT latency writes (l2c310_configure, cache-l2x0.c:575-578) route through
  l2c_write_sec → omap4_l2c310_write_sec (board-generic.c .l2c_write_sec) →
  default case = WARN+skip. **No NS latency write ever happens. The "guard
  cache-l2x0.c against SIGBUS" task is a NO-OP — already safe.**
- The REAL hazard at l2x0_of_init (init_IRQ): `l2c_enable` (cache-l2x0.c:112)
  does `__l2c_op_way(base + L2X0_INV_WAY)` — a **0x7FC by-way write = the
  on-device DEADLOCK op** (docs/06) — plus `l2c_wait_mask` (a poll on sync).
  If/when the boot reaches l2x0_of_init, patch l2c_enable first (skip the
  by-way op on this machine, or CONFIG_CACHE_L2X0=n and keep QNX's L2 setup).
  Moot until the bc=127 wall is broken.

### Payload: the --ppa probe (built 2026-09-03, pending device run)

- `ppa_call(idx, process, flag, pargs)` added to cacheops.S — the devpm shape
  with r12 REALLY zeroed (NOTE: mainline-style `mon_call_full(r0,r1,r2,r3,r6,
  r12)` ignores its r6/r12 args — hardcodes r6=0xFF/ip=0 and leaves r12 =
  caller garbage; unusable for PPA. Do not trust its comment).
### Run PPA-1 (2026-09-03): the --ppa probe — no secure-side L2 fix exists

PAYLOAD_MODE=--ppa (payload-only; NO jump — the box stays in QNX, no reboot,
that is by design). The probe ran clean to completion: bc[1]=53 DONE, all
magics valid, bc[2..6] = the returns below. Mirrors/ring1 stale from the last
kernel boot (expected — the probe writes neither).

| Call | ret | Verdict |
|------|-----|---------|
| PPA 0x25 CPU_ACTRL_SMP (pargs=0) | 0xFF02 | REJECTED — not in this monitor |
| PPA 0x26 (devpm shape, pargs=&{1}) | 0x0 | accepted |
| PPA 0x27 (devpm shape, pargs=&{1}) | 0x0 | accepted |
| PPA 0x23 L2_POR (pargs=0) | 0xFF02 | REJECTED — no secure L2 re-init |

PL310 readbacks before/after every call, IDENTICAL:
ctrl=00000001 aux=1e070000 tag=00000000 data=00000111 prefetch=00000000.
0xFF02 for both unimplemented indices (0x25/0x23) vs 0x00 for the devpm pair
= the monitor distinguishes implemented/rejected services; the BlackBerry
monitor implements 0x26/0x27 (its suspend pair) and NOT the mainline PPA set.
**Conclusion: the secure-side fix path is EXHAUSTED.** No latency SMC, no
PPA L2 service, nothing in the monitor changes the PL310 config. The
corruption/wall fix has to be kernel-side (or payload-side).

### The bc=127 wall ROOT-CAUSED (all facts grepped, not recalled — 2026-09-03)

Two independent defects, both proven from the tree. **Neither is machine
corruption.** (Credit: the user's pushback on recall-vs-grep forced every
constant here to be grepped; do not trust recalled source from earlier docs.)

**Defect 1 (primary, deterministic): the kernel load placement vs the DT
memory bank mismatch.**
- The payload loads the Image at `want[]+0x8000` ∈ {0xA4080000, 0xA8080000,
  0xAC000000} (qnx2linux.c:613, blob_off=0x8000); head.S derives the runtime
  PHYS_OFFSET = entry-0x8000 (ARM_PATCH_PHYS_VIRT=y, CONFIG_*_PHYS_OFFSET
  unset — PHYS_OFFSET = __pv_phys_pfn_offset << 12).
- The DTS memory node = `reg <0x80000000 0x40000000>` (1 GB from
  0x80000000) — memblock spans the WHOLE bank, including **576 MB BELOW the
  kernel's PHYS_OFFSET** (0x80000000..0xa3ffffff) — a zone that can never be
  linear-mapped (VAs would be < PAGE_OFFSET).
- memblock bottom-up allocs land in that dead zone: dram_sync steal =
  arm_memblock_steal (omap-secure.c:111-116) → 0xa1000000 observed; CMA at
  some placements → 0xa1000000 (bottom) vs 0xbe800000 (top).
- dma_contiguous_remap (dma-mapping.c:270) maps dma_mmu_remap[].base:
  `__phys_to_virt(0xa1000000)` = 0xBD000000 with a CORRECT pv; TASK_SIZE =
  PAGE_OFFSET-16MB = **0xBF000000** (memory.h:44) → mmu.c:979
  `md->virtual < TASK_SIZE` → **"BUG: not creating mapping ... in user
  region" fires even with a perfect pv fixup.** THE WALL, guaranteed.
- Same class explains: the memblock limit-0 panic (vmalloc_limit vs region
  base), "cmdline placement-flaky", the dram_sync steal failure, and
  placement-dependent behavior generally.

**Defect 2 (secondary, observed once, mechanism open): an UNPATCHED pv
stub site.** The observed BUG printed VA 0x1f7f0000, which is NOT the
correct-pv value (0xBD000000) — it is
`0xa1000000 - 0x81810000`, and **0x81810000 = the unpatched stub placeholder**
(asm/memory.h:183-184: __PV_BITS_31_24 0x81000000 + __PV_BITS_23_16
0x810000). phys2virt.S/head.S fixup code is byte-identical to mainline v6.15
(diffed). A site holding the placeholder = the pv-table walk missed ≥1 entry
in that run (how is unknown). The SAME placeholder value appeared in the
session-6 "barrier stack struct corruption" (0xfe600000 → 0x1f7f0000 =
__va(0xa1000000) via the unpatched stub) — so at least one earlier
"corruption" was this too, not memory loss.
- Discriminator for next run: PB print in dma_contiguous_remap of base,
  __pv_offset low word, and __phys_to_virt(base). Variable correct + VA
  wrong → site unpatched; both wrong → fixup input (r8) wrong.

**Fix plan (run-ready):**
1. DTS: memory node reg = `<0xa4000000 0x1c000000>` (bank = the load region,
   448 MB). PHYS_OFFSET_eff then == bank base; every memblock alloc lands
   inside the linear map.
2. Payload: want[] = {0xA4000000} ONLY — abort cleanly if QNX won't hand
   over that 24 MB (no silent fallback onto a mismatched placement).
3. Kernel: the dma_contiguous_remap PB print (also watches Defect 2).
4. mkkernel.sh rebuild (DTB is appended), re-jump --l2on.

### Run W-2 (2026-09-03, after clean hard-reset boot) + the eMMC secure-boot blob discovery

PAYLOAD_MODE=--l2on again aborted pre-jump: even the v2 fallbacks failed —
"FATAL: no usable 2MB-aligned 24MB placement". The box stayed in QNX (correct),
and the delayed "self reboot" = the armed WDT2 firing (the payload kicks WDT2
at arm time, BEFORE the placement attempt; an abort leaves it armed — v3
disarms WDT2 on the abort path, SPR 0x48 = 0xAAAA/0x5555 sequence per the
kernel's omap_wdt.c). The scrambled bc tail / zeroed mirror2 on readback =
QNX boot trampling (known; magic + bc[1]=43 survived).

v3 payload (built): the placement search now SWEEPS ALL 2MB-aligned slots of
the upper bank (129 hints, 0xa0000000..0xb0000000 step 2MB; bank >= 256MB),
then a generic loop, and PRINTS failure counters per reason (mmap/moved/frag/
unaligned/bank/protected/alias) so a refusal is diagnosable. cfP facts
(user-provided flashinfo) now on record: DRAM = TWO 512MB banks
(0x80000000-9FFFFFFF / 0xA0000000-BFFFFFFF, Elpida, 64MB ranks) — the p >=
0xa0000000 constraint keeps the kernel inside one bank; Bootrom 5.27.0.20;
IRAM Base 0x40304000; OS 1.0.7.2670 DEV.

### Run W-3 (2026-09-03): the sweep counters crack the placement problem

`placement sweep: 129 2MB slots tried, failures: mmap=0 moved=0 frag=129
unaligned=12 bank=0 protected=0 alias=0 -> NONE`. Every hinted mmap SUCCEEDED
and the hint was HONORED (moved=0) — but contig < 24MB for all 129 (frag);
the generic loop FOUND 12 contiguous 24MB blocks, ALL at unaligned addresses.
Conclusion: QNX's free pool simply has no 2MB-aligned 24MB run — the
alignment must move INSIDE the buffer. v4 payload: accept any aligned-enough
buffer (range/alias/protected checks only) and place the kernel at the first
2MB-aligned offset inside it (kern_off < 2MB, ~6MB slack in a 24MB buffer);
kern_phys drives blob_off, dtb_phys, the entry passed to enter_stub/params,
and the DTB bank base. Also: wdt2_kick now runs the full ENABLE sequence
first (SPR 0xBBBB/0x4444 per omap_wdt_enable) because wdt2_disable() on an
abort leaves the watchdog OFF and a TGR write on a disabled WDT is a no-op
(a later jump would run with NO recovery window). User note: "wdt2 disabled"
appears only on abort paths; after an abort the box stays up with no WDT2
until the next run/reboot.

### The eMMC secure-boot blobs (dumped 2026-09-03 — device-binaries/)

`dd if=/dev/hd0` of the first 1MB (sectors 0-2047) reveals a mini-FAT MBR
listing named blobs, carved into device-binaries/:
- `bootblob_KEYS.bin` (0x200 @0x960) — TI secure-boot cert chain
  (CertPK_/PK_SIG_INFO_ROM_PKC, CertPPA/PPA_SIGINFO_ROM_PKC).
- `bootblob_PRIMAPP.bin` (0xc00 @0x32b8) — 3KB THUMB, NO signed header:
  ~112 instructions of real code then cert data. The PPA/primary app.
- `bootblob_MLO.bin` (0x4000 @0x4d70) — signed-header second stage.
- `bootblob_arm9000.bin` (84KB, eMMC 0x9000-0x1e000) — **the QNX initial
  loader** (ARM vector table at +0): contains the NS-side SMC wrappers
  `push; movw ip, #SERVICE; smc #1; pop` for services **0x102, 0x109, 0x100
  and 0x112 TWICE** (0x10394/0x103a8/0x103bc/0x103d0/0x103e4 blob offsets)
  plus the r6=0xFF PPA shape at +0x94.
- **0x112 is NOT in the documented API** (mainline + our table stop at
  0x100/0x102/0x108/0x109/0x113). Two wrapper shapes (r1-r3 args / r2-r3
  args). No direct BL callers found (ARM or Thumb scan) — the stubs are
  likely published in IRAM for later stages or called via literals.
  NEXT RE STEP: full .dis of bootblob_arm9000.bin; find 0x112's callers and
  arguments. If 0x112 = the L2 tag/data-latency write (the observed QNX
  latency VALUE is 0x111 — suspicious proximity to the service number),
  the payload can call it with sane values pre-jump. Probe candidate for a
  --ppa-style on-device test with PL310 readbacks (0x108/0x10C) either way.
- The MBR file the user had (PlayBook_2.0.0.4869.0.mbr, 64KB, "QNX v1.2b
  Boot Loader") = the disk boot sector stage — distinct from this blob.
- cfp says Bootrom lives at MCT blocks 5-12; the carved blobs came from
  user-area sectors 5-240 — matching the Bootrom/PRIMAPP region per the
  MCT map.

### Run W-4 (2026-09-03): THE 127 WALL IS BROKEN — boot reached bc=171; but the DTB was lost en route

PAYLOAD_MODE=--l2on, kern_off placement: the jump HAPPENED (SSH died t=60s,
the full LED sequence: blue on during setup, off, then red at the WDT2
reboot — timings pending from the user's video). Post-reboot readbacks:
- **bc[1] = 171** — past 127 (dma_contiguous_remap SOLVED by the placement/
  bank fix), past paging_init, at the OLD session-6 wall
  (taskstats_init_early / kmem_cache_create).
- bc[8]=1 / bc[9]=0x111 = the probe's PL310 readbacks: L2 stayed ON.
- The console (ring1, 1098 chars, decoded): Linux banner OK, but
  **"Warning: Neither atags nor dtb found" TWICE** and
  **PB-MEM m[0]=a0800000+1000000** = the NO-DTB 16MB fallback
  ("cma: Failed to reserve 16 MiB"). The kernel ran with NO DTB and 16MB —
  and STILL reached 171.
- Address math closed: kern_phys = 0xa0800000, dtb_phys = 0xa19378b8
  (matches the probe's bc[4] r2 echo EXACTLY). The payload delivered the
  DTB correctly; the probe VALIDATED it (bc[6] = 0xedfe0dd0 magic OK, LED
  chain ran, sanity passed) — **then the kernel rejected it.**
- Prime suspect: the probe's **1 GB long-loop diagnostic** runs BETWEEN the
  probe's DTB check and the kernel's vet — 1GB of WB stores through QNX's
  1-cycle-latency L2 (the ORIGINAL machine-corruption suspect). If it
  scribbled on the DTB region, the kernel's vet saw garbage → no DTB →
  16MB fallback → and the 171 death is plausibly the SAME corruption later
  in the boot. This re-opens the lost-write theory with REAL evidence
  (the dtb died between two verified reads of the same address).
- Console stopped after "PB-ADJ" while bc continued to 171 — secondary
  mystery (printascii silencing post-memblock), not blocking.

W-5 test: --t3 (NO probe, no 1GB loop) — the cont hands r2 = params[1]
directly to the kernel. parp[] now written UNCONDITIONALLY (the old code
wrote params[1] only in the probe path — a latent --t3 no-DTB bug). If the
DTB survives --t3 ("Machine model: BlackBerry PlayBook" + full memory
appears), the probe loop = the corruptor and the 171 wall = the same
machine corruption. If the DTB dies WITHOUT the loop too, the corruption is
  in the jump path itself.

### Run W-5 (2026-09-03): --t3 direct jump — the probe loop is EXONERATED; park the Image path

--t3 (no probe, no 1GB loop, cont hands r2 = params[1] straight to the
kernel): the ring shows the IDENTICAL failure — "Warning: Neither atags nor
dtb found" ×2, the 16MB fallback (PB-MEM m[0]=a0400000+1000000 — a different
kern_phys this run, kern_off rounding worked), cma failed, death at
bc[1]=126 (one marker before map_kernel done; W-4 with the probe reached
171 — the no-DTB fallback differences shift the death point, the DTB loss
is the common factor). The probe's 1GB loop did NOT corrupt the DTB —
**the r2 chain itself (cont→kernel entry→head.S save/restore→vet) loses it
on the uncompressed-Image path.** Params were verified correct (W-4 math:
dtb_phys 0xa19378b8 == the probe's bc[4] echo, reverify passed).

**THE KEY REALIZATION (W-5 post-mortem): we have been jumping the
UNCOMPRESSED Image — jump.sh's default — where the r2 chain must be
hand-delivered. Session 6's proven path was the ZIMAGE** (docs/04: "zImage
— correct path"): CONFIG_ARM_APPENDED_DTB=y + CONFIG_AUTO_ZRELADDR=y mean
the decompressor natively (a) finds the appended DTB and SETS r2, and
(b) computes zreladdr from its own load address (512MB-granular → PHYS_OFFSET
512MB-aligned → pv-safe). Session 6's recovered log proves the whole thing
worked: "OF: fdt: Machine model: BlackBerry PlayBook" + "OF: fdt: Ignoring
memory range 0x80000000 - 0xa0000000" — **the fdt memory trim to
PHYS_OFFSET is NATIVE and the original 1GB DTS bank was never a problem on
the zImage path** (the W-series bank patching is belt-and-braces, harmless).
The uncompressed-Image r2 mystery is parked — zImage is the documented
correct path.

### W-series conclusion + next session's first run

- 127 wall: SOLVED (placement/DTB-bank mismatch — the fix stands, and the
  zImage fdt-trim provides the same guarantee natively).
- Next run: `PAYLOAD_MODE=--l2on ./jump.sh zImage` — expect the DTB to
  arrive ("Machine model: BlackBerry PlayBook" in the console, full ~448MB
  memory, cma reserved) and the boot to proceed past the 16MB-degraded
  state. The 171 wall then gets retested with FULL memory and a real DTB.
- The unpatched-pv-stub placeholder (0x81810000 delta) remains unexplained
  but unreproduced since the W-series fixes; the PB-CMA print is in place
  to catch it.

### Run W-6 (2026-09-04): the zImage pivot's first test — early death INSIDE __fixup_pv_table; bc[2]=0x3E7 returns

PAYLOAD_MODE=--l2on ./jump.sh zImage (fresh build, kernel #85: PB-CMA print +
current DTS + config). SSH died t=40s (jump happened). Readbacks:

- **bc[1] = 107** — the AMBIGUOUS marker (two sites: addruart-done @head.S:210
  AND post-__fixup_smp @:248). Resolved by ring count: ring1 = 0x14 = 20 =
  19 smoke chars + 'K', and the 'K' is ring-committed at busyuart (109) —
  so the ENTIRE UART diag block passed, the r1/r2 restore ran, and 107 =
  **post-__fixup_smp**. Death = between head.S:248 and :252 → **inside
  `__fixup_pv_table`** (line 250).
- **bc[2] = 0x3E7 (999) — the session-5/6 wild-write signature is BACK.**
  Historical correlation: runs 23-32 (the last zImage era) showed exactly
  this value; the Image-path W-4/W-5 runs showed other values (0x163/0x17e).
  bc[2]'s writer remains unidentified (KNOWN_ISSUES #1 — the probe's
  totalsize echo would be 0x19550100 for the current 87KB DTB, not 0x3E7).
- bc[3]=0xa1400000 (buffer/kern base), bc[4]=0xa1957080 = the probe's r2
  echo (dtb_phys — the chain HELD this run), bc[5]=0xe1a00000 = **zImage
  word 0** (`mov r0,r0` NOP — confirms the probe chained into the zImage),
  bc[6]=0xedfe0dd0 (DTB magic OK), bc[8]=1/bc[9]=0x111 (PL310 on, latency
  as before), bc[12]=0xa1957080 (the r7 chain = dtb_phys — chain held).
- ring1 = smoke ONLY (20 chars): zero kernel-proper console output —
  consistent with a death before start_kernel's first prints.
- mirror0 = 0x46 (70, "jump started", without 71 = NORMAL).
- Auto-zreladdr math: load 0xa1408000 → zreladdr 0xa0080000 (no
  self-relocation — no overlap with the 5.5 MB compressed image) →
  PHYS_OFFSET 0xa0000000 → pv delta 0xE0000000 (2MB-aligned, passes the
  fixup's alignment check).
- The decompressor does NOT run the kernel's pv fixup (grepped — only DTB
  relocation), and the decompressed kernel lands OUTSIDE the payload buffer
  (0xa0080000-0xa128xxxx vs buffer 0xa1400000+) — written through the L2 by
  the decompressor, unverified by anything.

**Interpretation**: the W-4/W-5 Image path survived `__fixup_pv_table`
(delta 0xE0400000/0xE0C00000-class); the zImage path (delta 0xE0000000,
kernel at 0xa0080000, decompressor-written DRAM) dies inside it. The delta
VALUE itself is encodable either way — the difference is the environment:
decompressor-written DRAM (L2-dirty, never verified) vs payload-NOCACHE-
copied DRAM (memcmp-verified). The lost-write/corruption ledger
(contradictions/machine-corruption-vs-code-bugs.md) gains its strongest
datum yet: a verified-channel death in a function whose only state is the
pv table in DRAM.

**Next-step candidates** (need user approval, all = 1 run each):
1. Make the 100-110 markers unique (101-109 renumbered) + add markers at
   __fixup_pv_table entry/exit + store the computed delta to a bc slot —
   pinpoints the death to the check, the table walk, or the str_l stores.
2. Dump the pv-table region + __pv_offset from DRAM post-mortem (the
   decompressed kernel's layout is computable from System.map).
3. Re-run the SAME Image-path binary for a control (it died at 171 — the
   fixup_pv death is zImage-specific so far).

### Run W-8 (2026-09-04): first run with CIPA-flushed markers — the fixup_pv death is CONFIRMED cleanly

PAYLOAD_MODE=--l2on ./jump.sh zImage (kernel #87: pbmark/pbmark3 CIPA-flush
every marker; kern_phys 0xa1500000 this run). SSH died t=70s. Readbacks:
- bc[1] = **130** (post-__fixup_smp, FLUSHED — trustworthy) with nonce
  0xcbca5e14 fresh.
- **No 131** (post-__fixup_pv_table, also flushed, absent) → with the
  eviction-luck variable eliminated, the death between head.S:248 and :252
  = **inside `__fixup_pv_table` is now cleanly confirmed**, consistent with
  W-6/W-7.
- **bc[2] = 0x3E7 for the THIRD consecutive zImage run** (W-6/W-7/W-8; the
  Image-path runs showed other values). zImage-correlation holds.
- bc[3]=0xa1500000 (buffer/kern base), bc[4]=0xa1b56570 (the probe's r2
  echo; NOTE — the dtb-phys arithmetic has not closed exactly in ANY zImage
  run; session 8 should re-derive it with the objdump-assisted build info
  rather than hand-math), bc[5]=0xe1a00000 (zImage word 0 ✓), bc[6]=
  0xedfe0dd0 (the probe's DTB magic — see the caveat below), bc[8]=1/
  bc[9]=0x111, bc[12]=0xa1b56570 (r7 chain held), ring1 = smoke only.
- CAVEAT on bc[6]/140/141: the phys2virt instrumentation stores (bc[6]=r8,
  markers 140/141) have NO CIPA flush (r0+ip budget) — they are L2-dirty
  and can be discarded by QNX's L2 re-init, exactly like the pre-W-8
  markers. So bc[6] still showing the probe's magic does NOT prove the
  fixup never ran its first store — it proves the store's value did not
  survive to DRAM. The reliable discrimination stays 130-vs-131 (both
  flushed): the death is inside the fixup.
- Combined with W-7's post-mortem (fixup bytes intact in DRAM, the bl
  intact, __pv_offset = 0): the boot dies inside a function whose code is
  verifiably intact and whose bl is verifiably intact. Session 8's task.

**Session-8 candidate experiments** (one run each):
1. Add the CIPA flush (pbmark3-style, fire-and-forget) to the phys2virt
   instrumentation stores — makes 140/141/bc[6] trustworthy.
2. Order swap: call __fixup_pv_table BEFORE __fixup_smp for one run —
   positional/ordering test (does the SECOND call site die, or does
   fixup_pv die wherever it is?).
3. Inline-test: temporarily copy the fixup's first stores into head.S
   directly (head.o region, proven stores) — isolates "the function" from
   "the stores".
4. The W-8 arithmetic check: re-derive bc[4] (dtb_phys) exactly from the
   build (zImage size, padding, kern_phys) using the arm-linux-objdump —
   if the payload's computed dtb_phys ≠ the probe's r2 echo, the params
   block itself is implicated.

### Run W-1 (2026-09-03): the pin refused the jump — placement made adaptive

PAYLOAD_MODE=--l2on: the payload ABORTED pre-jump exactly as designed —
"FATAL: 24MB at phys 0xa4000000 unavailable (QNX owns it?)" (bc[1]=43,
QNX alive, no reboot, LED on but no jump). 0xa4000000 was simply busy; the
old fallback list existed for a reason. v2: the payload now PATCHES the DTB
memory node's reg at runtime to match the actual placement (fdt_patch_memory:
a minimal FDT walk; device_type=="memory" -> reg rewritten in place, same
8-byte size). Constraints kept: 2MB-aligned phys (pv delta alignment),
p in [0xa0000000, 0xc0000000-24MB), buf_placement_bad guards. The pinned
DTS bank (0xa4000000) remains as the baked default; the copy handed to the
kernel via r2 is the patched one. Host-tested: reg (0xA8000000,0x18000000)
round-trips correctly. One walker bug found+fixed on the way (node-name NUL
is not word-aligned — byte-scan then round up).

### Run W-7 (2026-09-04): [RECONSTRUCTED 2026-09-04 from the session-8
bootstrap and PROJECT_STATE — this run never got its own record; appended
here per the append-only protocol, facts as recorded in those sources]

PAYLOAD_MODE=--l2on ./jump.sh zImage (the run between W-6's kernel #85 and
W-8's #87 — the pbmark/pbmark3 CIPA-flush build or its immediate
predecessor; the exact build number and this run's bc[3]/bc[4]/ring1
readbacks are NOT recoverable). Recorded facts (bootstrap + PROJECT_STATE):
- bc[1] = 130 post-__fixup_smp, never 131 — same as W-6 (whose ambiguous
  107 was renumbered to unique 130/131) and W-8; ring1 = smoke test only.
- bc[2] = 0x3E7 — the zImage-correlated value, second of three (W-6/W-7/W-8).
- THE W-7 POST-MORTEM (this run's real contribution, technique now in
  newdocs/COMMANDS.md "Post-mortem"): dumped the decompressed kernel from
  DRAM after the WDT2 reboot before QNX trampled it — System.map
  `c00085c8 T __fixup_pv_table` → PA = VA − 0x20000000 (zImage-path
  PHYS_OFFSET 0xa0000000). Findings: the fixup code bytes were BYTE-INTACT
  in the decompressed image, the `bl __fixup_pv_table` intact, and
  `__pv_offset`/`__pv_phys_pfn_offset` read back 0 (the pre-fixup state —
  either the fixup died before its .data stores, or the stores were
  L2-dirty and discarded).
- The lost instrumentation stores from this post-mortem exposed the
  persistence model: SO bc stores only DIRTY the PL310; QNX's L2 re-init
  after the warm reset discards dirty lines. W-8 then CIPA-flushed the
  pbmark/pbmark3 markers by PA (0x768/0x730) — the fixup's OWN stores
  stayed unflushed until the session-8 phys2virt.S edit (commit e842c33).

### Run W-9 (2026-09-04): flushed fixup instrumentation — the fixup NEVER
### ENTERED; death pinned to the pre-entry window; the L2 variable sharpens

Build: kernel #88 (phys2virt.S W-8 edit, commit e842c33): entry bc[6]=r8 +
marker 140 now CIPA-flushed; the 141 site additionally stores the computed
delta to bc[7] and flushes the __pv_offset .data line(s); zImage packed
5,566,497 B (raw +2,751 vs W-8 — matches ~90 new words + LZMA shift); the
shipped vmlinux objdump-verified (c0008884 region: CIPA constants, bounded
polls, bc[7] store all present). NOTE: an initial "+2,744 B vs raw" panic
was an apples-to-oranges comparison (packed vs raw) — sizes are consistent.

Run: PAYLOAD_MODE=--l2on ./jump.sh zImage. SSH gone t=45s; reboot ~2 min
(LED timings pending — the user extracts them from video; device clock is
GMT-3, host is UTC — convert when comparing).

Readbacks (nonce 0xcb9a6f11 fresh; W-8 was 0xcbca5e14):
- bc[1] = 130 (post-__fixup_smp, flushed) — 131 absent, same shape as
  W-6/W-7/W-8.
- **bc[6] = 0xedfe0dd0 — the probe's DTB magic, NOT r8.** The fixup's
  entry store is CIPA-flushed now; had the fixup executed its first three
  instructions, bc[6] would read 0xa0000000 (r8 = PHYS_OFFSET).
  **The fixup's entry instrumentation never landed.**
- bc[7] = 0x410000c4 (probe residue, not the delta) — the 141 site never
  ran either. (W-8 candidate 4 is CLOSED independently: the dtb-phys
  arithmetic closes EXACTLY for W-8 (buffer 0xa1500000 → kern_off
  0x100000 → kern_phys 0xa1600000 → dtb_phys 0xa1b56570 = bc[4]) AND for
  W-6 (buffer 0xa1400000 is itself 2MB-aligned → kern_off 0 →
  0xa1408000 + 0x54e570 = 0xa1957080 = bc[4]). The "never closed" in the
  W-8 record came from bc[3]'s ambiguous "buffer/kern base" label — the
  code (qnx2linux.c:935) writes the BUFFER base. Params block exonerated.)
- bc[2] = 0x3E7 (4th consecutive zImage run); bc[3]=0xa1000000 (buffer);
  bc[4]=0xa1557028; bc[5]=0xe1a00000 (zImage word 0 ✓); bc[8]=1/bc[9]=
  0x111; bc[12]=0xa1557028 (cont chain); bc[14]=0x8be8eae6 (garbage);
  ring1 count 0x14 = smoke only; mirror0 = 70-without-71 = normal.

**W-9's structural finding: the death is between pbmark-130's CIPA and the
fixup's own CIPA** — ~25 instructions (pbmark tail: sync-poll exit, dsb,
kick; bl; mov/orr; str r8; dsb; the mov/orr CIPA constant chains). Every
instruction class in that window executed successfully multiple times
earlier in the SAME boot (bc stores to 0x9000000x, CIPA to 0x48242768,
sync reads 0x48242730, WDT kicks). W-7's post-mortem found these bytes
byte-intact. With flushed markers, "dies INSIDE __fixup_pv_table"
(W-6/W-8 reading) is now WRONG — the fixup never meaningfully ran; all
zImage deaths are consistent with the same pre-entry window.

bc[10]/bc[11] writers IDENTIFIED (grep, not recall): main.c:780 writes
bc[11]=0xC0DE0010 (parse_early_param entered, any call); main.c:792/799
write bc[10]=cmdline-head / bc[12]=0xC0DE0020 (first-call completion);
head-common.S:112-115 writes bc[10]=bss-start/bc[11]=bss-len pre-memset.
W-9's bc[11]=0xC0DE0010 with bc[12]=dtb_phys (cont's pre-kernel write) and
bc[14]=garbage (the double-call latch never fired) = **W-4-era residue,
not fresh** (W-4 reached the C world). bc[10]=1 matches no writer —
trample/stale. The C world was NOT reached this run.

ring2 dumps (KNOWN_ISSUES #1 follow-up): 0x90000200+ = W-4-era console
text (banner tail "...#4 SMP Thu Sep 3 02:05:36 UTC 2026" — that is the
kernel's BUILD timestamp from the host clock (WSL/UTC), not the device
clock — plus "[    0.0000..." / "CPU: ARMv7 Processor [411fc09...");
ring2 count @0x90000080 = 0x14 — SANE (this run's 20 smoke chars; the
banner text at 0x200+ is residue beyond the written prefix).
**The ring2-wild-index theory for bc[2]=0x3E7 weakens** — the index is
healthy. bc[2]'s writer remains unidentified.

Post-mortem impossible this run: 0xa0088000 and 0xa0088884 both zeroed
(QNX's trample won; W-7's dump was a faster race). New operational fact:
the user can power-hold hard-reset the device at will — useful PRE-run for
clean residue (bc_arm sanitizes only bc[0..5]); it cannot prevent the
post-reboot trample (QNX must boot for SSH readback).

**The era-matrix sharpening**: runs 23-32 (zImage, UNCACHED, L2 OFF,
DTB + full memory) PASSED the fixup 8+ times (died later at 171);
W-6/7/8/9 (zImage, CACHEABLE, L2 ON) died in the pre-fixup window 4/4
times. The death is {L2-on + cacheable}-correlated. The untested
isolation cell: **CACHEABLE kernel + L2 OFF** = `PAYLOAD_MODE=--t3
./jump.sh zImage` (the proven 0x102 mon_call disable; the mode W-5 ran) —
no code changes, one run.

Updated session-8 candidate list:
1. `--t3` zImage L2-off A/B ← the new lead (isolates the L2 variable).
2. Inline-test: copy the fixup's first stores into head.S right after
   pbmark 130 (isolates the bl/call from the stores; W-8 candidate 3).
3. Order-swap fixup_pv/fixup_smp (W-8 candidate 2).
4. Add an IMMEDIATE 0xa0088884-region dump to jump.sh's readback burst
   (race the trample like W-7 did).
