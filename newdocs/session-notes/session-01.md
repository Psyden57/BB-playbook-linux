# Session 1 Notes (2026-08-30 — the original session: intake, live recon, the
# monitor/SAR RE, and T2 PASS)

Backfilled at session-01 documentation time (2026-09-03) from the session-1
conversation context. Session 1 spans everything from the KEXEC-PROMPT intake
to the T2 PASS verdict; session 2 (2026-08-31 daytime) took the T3 kernel work
from there. Much of this session's output lives in `SESSION-HANDOFF.md`
(Rounds 1-4), `PLAYBOOK-REFERENCE.md` §1.6-1.10/§11, and `KEXEC-DESIGN.md`
(§7b as-built) — this file holds the narrative, the debugging saga, the
micro-facts only this context can supply, and the errata for session-1 claims
that later sessions corrected.

Hardware/software context of the era: 64 GB unit on OS 2.0.0.4869 (the kexec
test unit), SDP 6.6 toolchain set up mid-session, no kernel tree yet (the
kernel work is sessions 2+). No canonical run numbers existed; "runs" below
are the T2 attempts in execution order.

## THE ARC (in order)

### Phase A — intake and prior-art research (no device access)
- KEXEC-PROMPT.md read; the user flagged it as "not 100% factual" — the
  claimed XDA-era PlayBook Linux kernels never existed. Web-verified: pmOS
  `linux-postmarketos-omap` = vanilla 6.15 + ~11 mostly-DTS patches (N900,
  Galaxy Espresso, Nook); Droid 4 (`omap4-droid4-xt894.dts`) in-tree;
  tmlind's droid4-kexecboot = the mechanics model; **no PlayBook DTS or
  device package anywhere in pmaports**. Espresso tablets are the closest
  OMAP4430+TWL6030 analogues. BOM from teardowns (WL1283, WM8994, CY8CTMA3,
  MPU-3050, BMA150, ISL9519) — most mainline-supported except touch/display.
- KEXEC-DESIGN.md drafted: Option A (direct jump) vs B (kexecboot stage) →
  recommended A-first; ATags-vs-DTB → **DTB** (r1=0xffffffff selects the
  `DT_MACHINE_START` desc, r2=DTB phys; `CONFIG_ARM_APPENDED_DTB` as
  belt-and-braces); memory map with the jump region at 0x84000000.

### Phase B — ground truth from the user
- `cfp info`/`flashinfo` for the 64 GB unit captured (§1.6): Samsung MCGAFA
  eMMC (the 32 GB units are SanDisk SEM32G), factory WP only blocks 5-260 on
  THIS unit, IRAM base 0x40304000, OS Address 0x800FF800-0x803BF60F, HWV POP
  security 0x02 (HS), Bootrom 5.27.0.20.
- `dumped4869ifs/proc/boot/.script` parsed (§1.7): the full boot order with
  driver arguments, including `devc-seromap -e -F -b115200 -c48000000/16
  0x48020000` (UART3 = console, killed the "no serial lifeline" assumption)
  and `omap4430-wdtkick -t 15000 -P` (15 s argument; TWL6030 PMIC watchdog
  also kicked).
- `rootufstest/` chain understood: env.sh runs at **runlevel 0 inside
  `filesystem_common`** (second service — before pps/powerman/screen/audio/
  usbmgr), spawns a runlevel-waiter → post-boot.sh → sshd; also neuters
  check_sanity() and installs a launcher-insecure via the /base/sbin/launcher
  existence check.

### Phase C — live recon over SSH (all read-only)
- `pidin arg`: 137 procs / 418 threads, FreeMem 582 MB/1024 MB post-boot.
- **`pidin mem` harvest** (the single most productive recon step — every
  driver's mapped physical bases, all live/clocked by construction):
  eMMC = **MMC2 @0x480B4000**, touch = MCSPI3 @0x480BA000, WDT2 =
  0x4A314000, powerman maps 0x48240000 (16 K → GICD/GICC/PL310) +
  0x4A326000 (SAR/PRM_MPU), rpmb 0x4A004000/0x4A30C000, io-usb-dcd
  0x4A0AB000 (MUSB), screen 0x58006000/0x4A100000, winchester-lc 0x4A310000.
- The LED driver on Rev:07 = FAN5702 (I2C) vs led-gpio.so on Rev:00-02.

### Phase D — tooling and the two incidents
- `memdump3`/`memw32` written: single-address `mmap_device_memory` with
  PROT_NOCACHE. Empirical safety property: a bad address gives a **contained
  SIGBUS** (kernel survives) — unlike a plain cacheable mmap, which kills the
  machine via speculative access.
- **Incident 1**: `dd if=/dev/mem skip=…` to "read a register" — dd walked
  the physical bus from 0 upward; device rebooted. Rule 2b born: never sweep;
  compiled per-address tools only.
- **Incident 2**: a blind SMC service sweep (~50 ids, empty param lists)
  hung the secure monitor → full freeze → WDT2 reset. Rule: sweeps banned;
  RE the callers instead. (Later sessions found the deeper reason — see
  ERRATA #1: the sweep's call shape was wrong too.)

### Phase E — the monitor / watchdog / CPU1 RE (static + live)
- `wdtkick` RE: kick = **complement WTGR @+0x30** (never WCRR); WSPR @+0x48
  reads 0x4444 = enabled (enable seq 0xBBBB→0x4444); the "WDT is disabled
  now" string is printed when WSPR != 0x4444 — wdtkick NEVER disables WDT2.
  Also kicks the **TWL6030 PMIC watchdog** via devctlv(/dev/i2c0, 0x80100505),
  slave 0x48, reg 0x2C = 0x7F (127 s window).
- `setup-core-inactive` RE: a TI PRCM HAL port (DPLL/VC/SR/OPP init) — **no**
  CPU1 parking logic, no AUX_CORE_BOOT refs, no WFE/SEV, no SMC.
- `trustzone-omap4` RE (first pass): raw SMC passthrough devctl **0xC0280501**,
  40-byte payload `{result, fnid, a1, a2, n, w[4], pad[2]}`, handler marshals
  `ldmib r5,{r0-r3}` + stack words; one SMC site (r6=0xFF/ip=0 shape).
  Session 7's later RE refines the identity (crypto resmgr, one SMC site).
- `libsecure_dispatcher` RE: rng_hwRNGen passes **phys-first** (mmap
  MAP_ANON|MAP_PHYS + mem_offset64), payload `{res,0x12,0,4,2,phys,len}`;
  monitor accepted the call but never wrote an external buffer → the
  "registered secure window" theory (unproven, see ANALYSES #2).
- Monitor call-shape experiments: 0x31 driver-shape → OK; lib-shape → 3;
  0x103/104/105 FAIL; **0x101/0x103 probes crashed**; blind sweep → incident
  2. NOTE — all of these used the devctl passthrough with fnid in r0 and
  r12=0; the conclusions were shape artifacts (see ERRATA #1).
- **PRCM base corrections from the trustzone/setup RE**: CM_MPU = 0x4A004000,
  CM2 = 0x4A009400 (the earlier 0x4A007000 SIGBUS was a wrong address, not a
  firewall), PRM = 0x4A306000, PRM_MPU = 0x4A326000 firewalled from NS
  EXCEPT the monitor window (~0x4A326A00-CD0).
- **SAR RAM decode** (the CPU1 warm-reset answer): the real AUX_CORE_BOOT
  regs are WUGEN **0x48281800/04** (not 0x4A002E08); CPU1 warm reset runs a
  SAR trampoline @0x4A326B00-CD0 (persistent across boots): I-cache inval →
  smc#0 0x26/0x27 → MPIDR test → full context restore (per-mode SP/LR,
  CPACR, TTBR0/1, TTBCR, DACR, PRRR/NMRR, CONTEXTIDR, VBAR read, SCTLR last)
  from tables @0x4A326C50 (CPU1) / 0x4A326D50 (CPU0) → resume. Context is
  monitor-verified (services 0x26/0x27) → **cannot forge** → CPU1 warm reset
  always rejoins QNX; the WUGEN pen regs are consumed only by the initial
  cold boot. Live proof: releasing CPU1 with stale boot1 rejoined QNX
  seamlessly (system survived); our first --hello run's bc stayed at 12
  because of exactly this.
- **devpm-omap4 RE** (subagent): the CPU1 offline blueprint —
  `omap4_cpu_offline_prep` **memcpys the 0x150-byte wake trampoline from the
  .so (offset 0xbf68) into SAR 0x4A326B00 via a plain NS mmap** — the
  "monitor-protected" blob is NS-written memory; `wait_for_interrupt` saves
  the full context to the SAR tables then smc#0 (r0=0, r1=0xFF, r12=0x108)
  → WFI → power-off; wake = blob restore, PC = saved lr @0x4A326CD0.

### Phase F — the master key
- `procnto` has **no SMC emulation** (undef handler knows only QNX magic UDFs
  + VFP; userland `smc #0` = SIGILL, live-tested). The trustzone driver's SMC
  works because **`ThreadCtl(_NTO_TCTL_IO_PRIV /*=1*/, 0)` puts the thread in
  System (privileged) mode** (root has PROCMGR_AID_IO=31 by default) — after
  that, all CP15 and SMC are legal natively. Verified with live probes
  (CONTROL_STATUS = 0xAE8 matched the doc).

### Phase G — the T2 debugging saga (the failure chain, in order)
1. First `--t2`: console output truncated mid-line ("stub vad"), session
   hung. Learned: **USB output truncation at packet boundaries = console
   lag, not an execution stop** — the payload kept running silently.
2. Breadcrumbs (armed at 0x9F000000 then) showed the payload reaching the
   pre-jump marker but the stub never marking; the CPU1-release `--hello`
   variant revealed the SAR restore (above).
3. **TTBCR.N missing**: after TTBR0 switch, VAs ≥ QNX's TTBR1 boundary still
   walked QNX kernel tables → identity fetches aborted. Fix: trampoline sets
   **TTBCR = 0** (all VAs → TTBR0). Still froze.
4. **Hand-encoded TLBIMVA 0xEE080F31 was a garbage instruction** (hand-
   assembling CP15 ops) → SIGILL, silent with the console dead. Fix: ALL
   stub code moves to real assembly (`stub3.S`); never hand-encode.
5. **Pointer-arithmetic corruption**: `memcpy((void*)(nc + 0x830), …)` on a
   `uint32_t*` wrote at byte offset 0x20C0 — onto the IRAM page table
   (entry 0x830) → SIGSEGV in memcpy, then a corrupted-table jump death.
   Fix: byte pointers for every raw copy.
6. **mapdev after IRQs-off deadlocks** (procnto unreachable with no
   interrupts; possibly on the held CPU1) → all maps precede the IRQs-off
   window.
7. **XN discovery**: calling a heap buffer = SIGSEGV with ip = the heap
   address (code=2) → QNX enforces XN on data/anon pages; **PROT_EXEC is
   honored** on MAP_ANON|MAP_PHYS (probe 2) and on device maps (probe 3,
   IRAM executed). Jump buffer remapped with PROT_EXEC. (ERRATA #5: this
   was necessary but NOT the frozen-run cause.)
8. **The identity-branch offset trap**: `tt[va_section] = pa_section` is
   exact only when `(VA & 0xFFFFF) == (PA & 0xFFFFF)`; the frozen-at-70 runs
   had mismatched offsets → post-switch fetches landed on wrong physical
   bytes. Session-1 fix: reach the continuation by its **physical address**
   (TLBIMVA + `bx r12`, DRAM sections are identity) — sidesteps the
   assumption entirely. Later sessions generalized this into
   `buf_placement_bad` (SESSION-HANDOFF §line-322) and confirmed the same
   bug class was the historical freeze.
9. **L1 flush shifts wrong**: first derivation gave way_shift=13/set_shift=5
   → the "flush" cleaned almost nothing (stale I-fetch garbage). Correct
   derivation: **way_shift = clz(assoc−1) = 30, set_shift = way_shift −
   log2(numsets) = 22** (A9 L1: 4-way, 256 sets, 32 B lines).
10. **L2 staleness of the continuation**: continuation bytes written through
    the cached view sat dirty in L1/L2; after MMU-off the SO fetch may
    bypass the stale line → executed garbage. Fix: continuation copied
    through the **NOCACHE view** (straight to DDR), like the blob.
11. **Ordering discipline** (each earned by a freeze): no I/O after the
    CPU1 hold or GICD off (console dead, printf blocks forever); all
    kernel calls (mapdev) before IRQs-off (procnto unreachable → deadlock).
12. **Final PASS run**: trail 39/40 → 37 → 32 → 41 → 70 → 61-64 → 21 → 23 →
    3/4 → both cores self-held → WDT2 warm reset (~15 s) → clean reboot,
    full trail read back from the 0x9FE00000 mirror. **T2 PASS.**

### Phase H — the recovery design that made T2 repeatable
- The blob holds **both** cores at the end (CPU1 via its RSTCTRL — already
  held — and CPU0 via its own RSTCTRL bit, "a CPU can reset itself, held
  until the other active CPU clears" — nobody clears) → nobody services
  WDT2 → warm reset ≤15 s → **DRAM preserved** → breadcrumbs readable.
  This dodges the **TWL6030 PMIC watchdog's 127 s power-off** (observed
  live once: the device "simply shut down" — a power-off, not a reset,
  which destroys RAM and lost that run's verdict).
- The user's power-button reset also churns RAM (single-bit flips observed
  in the bc magic) — warm resets preserve, power events don't.

### Phase I — bb10tools intake (user-supplied, QNX8-oriented)
- `procnto/procnto_patch.c`: **live kernel patching** — syspage "bootram"
  asinfo region → mmap MAP_PHYS → find the procnto ELF by xxHash32 of the
  first 256 bytes → patch signatures in place → `msync(MS_SYNC)`. Two
  patches: `ker_ring0` (NOP @+0x10, 0xE320F000 — enables a userland ring-0
  exec gate) and `pathmgr_trust_handler` (PathTrust unlock).
- `unsorted/mmu*.c`: `__Ring0(fn, arg)` runs a function in kernel mode for
  MMU dumps — **the macro definition is not in the repo** (ask the fellow
  developer). Hint quoted: "check the memory map (mmu*.c); you might not
  even need to mess with TrustZone."
- For QNX 6.6 our System-mode access covers the same ground; the QNX8 ring-0
  patch was never needed for the jump.

## CONFIRMED (session-1 facts later sessions build on)

1. **System mode is the privileged-execution key** (ThreadCtl(
   `_NTO_TCTL_IO_PRIV`=1); userland smc = SIGILL; procnto has no SMC
   emulation — grepped, and live-tested both directions).
2. **TTBCR = 0 is required for the identity walk** — QNX splits TTBR1 for
   high VAs; without N=0 the post-switch walk uses QNX kernel tables.
   (Current ARCHITECTURE.md lists the step without the rationale; this is
   the rationale.)
3. **DACR = all-manager** sidesteps AP-format questions (QNX's DACR/AP
   format was unknown at switch time).
4. **Anything executed after MMU-off must be in DDR** — write it through a
   NOCACHE view. Cached-written bytes sit in L2 and SO fetches may bypass
   the stale line.
5. **The jump buffer needs PROT_EXEC** — QNX enforces XN on anon/data pages
   (heap probe SIGSEGV, ip = the heap address); PROT_EXEC is honored on
   MAP_ANON|MAP_PHYS and on device maps (IRAM executed via one).
6. **L1 flush shift derivation**: way_shift = clz(assoc−1) (30 for 4-way),
   set_shift = way_shift − log2(numsets) (22); A9 L1 = 4×256×32 B. The
   first derivation (13/5) silently cleaned nothing.
7. **CPU1 hold semantics**: RSTCTRL bit0 holds CPU1 in warm reset
   indefinitely; releasing with stale SAR state re-enters QNX seamlessly;
   releasing without a valid SAR/pen target crashes. WUGEN AUX_CORE_BOOT
   regs are cold-boot-only.
8. **The monitor surface**: crypto/KDS services exist (0x12/0x2E-0x33),
   devpm uses SCU_PWR 0x108 + PPA 0x26/0x27, the SAR flow uses 0x26/0x27,
   and the TI auxcoreboot/L2 table (0x100-0x113) is reachable — but ONLY
   with the service id in the correct register (see ERRATA #1).
9. **Two watchdogs**: WDT2 (kick = WTGR complement; window from the live
   registers, measured 58.6 s in session 5 — the `-t 15000` argument is not
   the hardware window) and the TWL6030 PMIC watchdog (i2c0, slave 0x48,
   reg 0x2C = 0x7F, 127 s, action = power-off).
10. **eMMC = MMC2 @0x480B4000**; env.sh = runlevel 0; boot = 2-3 min;
    `/tmp` wiped per reboot; `on -C 0` pins to CPU0 (0-based).

## ERRATA (session-1 claims later corrected — do not re-trust)

1. **The monitor probe FAILs were call-shape artifacts.** Session-1 concluded
   "0x103/0x104/0x105 FAIL, 0x101 crashes, the monitor route is exhausted"
   from probes sent through the trustzone devctl passthrough with fnid in
   **r0** and r12=0. The service id belongs in **r12** (TI HAL shape); the
   devctl-passthrough shape only works for the crypto set the driver
   addresses via r0. Session 6/7 established the C-flow `mon_call()` shape
   and verified **0x100-0x113 all work — 0x101 (L2 clean+inv by PA) is now
   the payload's flush mechanism**. Never cite the session-1 FAILs as
   evidence about service existence.
2. **"WDT2 disable = 0xDDDD→0x0000" is wrong.** That was session-1's guess
   from the observed WSPR=0x4444 state. The kernel's omap_wdt.c (session 7,
   grepped): enable = 0xBBBB→0x4444, **disable = 0xAAAA→0x5555**, kick =
   TGR complement. The stale claim is in PLAYBOOK-REFERENCE §1.10 and
   SESSION-HANDOFF Rounds 3/4 (erratum markers added 2026-09-03).
3. **"0x108 = context saved/arm SAR"** — session-1's interpretation of
   devpm's call. Session 7 identifies 0x108 = **SCU_PWR / omap4 suspend**
   (exact mainline sleep44xx.S shape). The call's role in devpm's offline
   flow is real; the name was wrong.
4. **"WDT2 window = 15 s"** — from the wdtkick `-t 15000` argument. Session 5
   measured **58.6 s** from the live registers. (Already noted in
   session-02 ANALYSES #2; repeated here because session-1's own docs carry
   the 15 s figure.)
5. **"The frozen-at-70 runs were XN"** — half right. XN on data pages is
   real (probe 1) and PROT_EXEC is required, but the freezes were the
   section-alias offset mismatch (later root-caused; SESSION-HANDOFF
   §line-322, `buf_placement_bad`). PROT_EXEC stays in the code for the
   correct reason.
6. **"The monitor only touches a registered secure window"** — plausible
   (rng accepted a phys arg and never wrote it) but unproven; session 7's
   RE of the crypto surface supersedes. Do not build on it.

## ANALYSES (session-1-only, with dispositions)

1. **USB console truncation is a lag artifact, not an execution stop**:
   output dies at deterministic packet boundaries while the payload keeps
   running silently (proven repeatedly — the trail always continued past
   the truncation point). Debugging rule: never infer death from missing
   console output; use breadcrumbs.
2. **Stale-breadcrumb ambiguity**: the mirror can hold the previous run's
   value (38 vs 39 confusion cost a cycle). Later sessions formalized the
   run-nonce practice (bc[3] = buffer phys); session-1 hit the problem
   first.
3. **The devpm SAR-overwrite "spare hook"** (overwrite the SAR wake blob so
   CPU1 runs our code on its next power-gate wake) — designed, never tested;
   superseded by the CPU0-jump design but remains a documented possibility.
4. **IRAM is not scrubbed between runs** ("iram pre: 00000000 0110f000" —
   leftovers of earlier stubs). Harmless (everything is rewritten), but a
   stale-content reminder for anyone reading IRAM forensically.

## DEAD ENDS (do not retest)

- **Blind SMC sweeps** — hung the monitor (incident 2); with the wrong call
  shape they were doubly meaningless. RE callers instead.
- **Heap/data-page execution** — XN-enforced (probe 1). Use PROT_EXEC
  mappings.
- **Userland `smc` without System mode** — SIGILL; procnto does not emulate.
- **trustzone devctl passthrough for HAL services** (0x100-0x113) — wrong
  register shape; use the C-flow mon_call with r12.
- **0x4A002E08 as AUX_CORE_BOOT** — wrong address; the real regs are WUGEN
  0x48281800/04.
- **Forging the SAR context** — monitor-verified (services 0x26/0x27);
  CPU1 warm reset cannot be redirected.
- **`dd if=/dev/mem skip=…`** — bus sweep; incident 1.
- **Kernel calls (mmap_device_memory) with IRQs off** — deadlock.

## WHAT SESSION 1 PASSED FORWARD

- The proven jump core and every rule needed to keep it working
  (kexec/README.md carries the operational form).
- The debug toolchain and its semantics (memdump3's contained-SIGBUS,
  mirror discipline, no-I/O zones, run nonces' ancestor: the stale-bc
  lesson).
- The register map, monitor surface, SAR decode, and watchdog model that
  sessions 2-7 refined.
- The kernel plan (pmOS 6.15 baseline, appended DTB, MMC2, UART3 console)
  that session 2 executed first.
- The documentation structure itself (README / kexec README / DESIGN /
  REFERENCE / HANDOFF) — written in this session for GitHub and
  continuation.
