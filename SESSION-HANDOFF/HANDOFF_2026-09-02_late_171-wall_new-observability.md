# SESSION-HANDOFF — 2026-09-02 (late): "171 wall" — kernel boots deep, dies at taskstats_init_early; full observability overhaul

READ ORDER: this file → docs/03_DEBUGGING_SESSIONS.md (the "Late 2026-09-02" section)
→ SESSION-RECORD_2026-09-02_L2-confirmed_monitor-RE.md → PART_1/2/3 playbook (background).

## WHERE THINGS STAND (one paragraph)

The L2 disable WORKS (monitor SMC #0, r12=0x102, r0=0 — reliable when called
from the payload's C flow via mon_call(), 4/4; the enter_stub-inline shape
hung 9/9 — always use the C-flow shape). With the L2 bypassed, the kernel
(6.15.11, CONFIG_CACHE_L2X0=n) boots through start_kernel, setup_arch,
paging_init, mm_init, cgroup_init and stalls deterministically at
bc[1]=171 (post-cgroup_init) — 6+ consecutive runs. bc[2]=0x3E7 (999) is a
mystery write that always accompanies it. The initcall breadcrumbs
(bc[6]=level, bc[7]=fn) never land: the death is between cgroup_init and
do_initcalls, i.e. taskstats_init_early / delayacct_init / acpi_subsystem_init
/ arch_post_acpi_subsys_init / kcsan_init / rest_init / kernel_init's
pre-do_basic_setup stretch. run 31 also proved: a payload SIGSEGV (bad PRCM
write) does NOT reboot the device — QNX survives, SSH stays up — so payload
crashes are cheap now.

## THE THREE BUGS/CHANGES THAT GOT US HERE (all in this session)

1. **WDT2 kicks were no-ops — PA-encoding bug, FIXED in run 30.** head.S
   WDT2 section (VA 0xFEC00000 → PA 0x4A300000): I had copied the 0xFEB
   pattern's `orr r3,r3,#0x200` (which carries PA bit 0x48200000's bits!)
   — the PA became 0x4A500000 and every VA-kick (pb_bc/pbmarkv/PB_MMU_BC)
   wrote a harmless register instead of WTGR. All those "died at marker X"
   runs (17–29) were just the 58.6 s WDT2 window expiring wherever the
   kernel happened to be — the kernel was alive the whole time. Kick =
   any write to WTGR (0x4A314030) → CRR reloads from LDR (0xFFE2B400 =
   58.6 s @ 32.768 kHz, PTV=0). Verified against drivers/watchdog/omap_wdt.h
   (TGR=0x30 ✓, SPR=0x48 — the start/stop service is at 0x48, NOT 0x30).
2. **Console output vanished since run 24 — UART VA section missing.**
   CONFIG_DEBUG_LL_INCLUDE="debug/omap4bc.S" (the PlayBook's own DEBUG_LL
   — every char goes to UART3 *and* three DRAM rings). Its addruart
   returns virt 0xFED20000, claiming head.S early-maps section 0xFED —
   it didn't. Every MMU-on console write hit unmapped VA 0xFED20014 and
   the L3 swallowed it (async error) — silent. FIXED in run 32: head.S
   now maps VA 0xFED00000 → PA 0x48000000 (UART3 at 0xFED20000).
   NOTE: runs 24–31 also had the WRONG-PA kicks (0x4A514030) — if that
   address hit something UART-related, it may have contributed; re-test
   console output now that both are fixed.
3. **Payload crash ≠ reboot.** run 31: the payload's first PRCM write
   (CM2 CLKSTCTRL @ 0x4A008700) took a SIGSEGV — the PRCM is secure-
   filtered from NS — and QNX SURVIVED (SSH stayed up, WDT daemon kept
   running). Payload user-mode aborts are cheap; only jump-context deaths
   reboot. This makes iteration much faster: bad payload → crash → SSH
   straight back in.

## THE NEW OBSERVABILITY MODEL (huge — learn this first)

- **ring3 (0x94000100, header count @0x94000080, index @0x94000084) is
  the monitor's UART3 capture buffer.** The secure monitor taps UART3's
  TX and appends every byte to ring3 with a cumulative count. It is
  persistent across WDT2 resets (secure RAM bookkeeping) and CANNOT be
  cleared by the payload (zeroing the headers just gets re-written by
  the monitor on the next capture). count=0x5A15 (23061) has been frozen
  since run 23 because **no kernel since run 24 has emitted a single
  UART3 byte** — the console was dead (bug #2 above). run 23's kernel
  (#51) emitted 23 KB: its full boot log is preserved in ring3 and was
  recovered to SESSION-HANDOFF/ring3-recovered-log-2026-09-02.txt.
- After run 32's fix, the NEXT run's ring3 tail should contain the
  current kernel's console output up to its death — **this is the
  primary readback now** (dump 0x94000100, find the tail, read the last
  lines before the gap).
- early_printk.c was also patched (run 28) to mirror console output into
  ring3 via the 0xD4000100 alias with a bounded index at 0xD4000084 —
  that index stayed 20 in runs 28–32, which (combined with the frozen
  monitor count) proves the console wrote nothing since run 23.
- bc page: bc[15] = run nonce (time(NULL)^phys — varies per run ✓).
  bc[6]/bc[7] = initcall level/fn breadcrumbs (working — but never
  reached). bc[10] = free. The 1 KB char windows: ring1 0x88000100,
  ring2 0x90000100 (INSIDE the bc page, +0x100..+0x4FF), ring3 0x94000100.

## THE DETERMINISTIC "171 WALL" (the current blocker)

The kernel reaches pb_bc(171) (post cgroup_init) and dies. Fine markers
176/177/178/179/180 (post taskstats/delayacct/acpi/arch_post_acpi/kcsan),
181/182 (inside taskstats_init_early: post KMEM_CACHE / post per-cpu init)
and 183/184/185 (inside __kmem_cache_create_args: post mutex_lock / post
merge scan / post create_cache) NEVER land. So the death is inside
taskstats_init_early → kmem_cache_create → before the mutex_lock completes.
Leading hypothesis: slab_mutex appears contended (corrupted lock word from
a lost writeback / the same silent-corruption class as the bare-metal
long-loop stall) → mutex slowpath → schedule() with IRQs disabled and no
scheduler yet → eternal silent stall → WDT2 reset 58.6 s after the last
kick. bc[2]=0x3E7 (999) is an unexplained write that always co-occurs.

## WHAT RUNS 23–32 ESTABLISHED (the dead-end map)

- run 23 (kernel #51): booted FARTHEST — 23 KB of console output preserved
  in ring3 (recovered, see above). Its bc ladder stopped at 171.
- runs 24–29: zero UART3 output (console dead — bug #2) + no-op kicks
  (bug #1) → bc[1]=171 every time, indistinguishable from a hang.
- run 30: kicks fixed → STILL 171 → the 171 death is REAL.
- run 31: the payload's PRCM auto-idle fix aborted at bc=50 (PRCM is
  secure-filtered) — device survived (no reboot). Auto-idle disable via
  direct NS writes is IMPOSSIBLE; the block was removed. If auto-idle
  corruption needs fixing, the path is the monitor's PPA clock-domain
  service (SMC#1 family — the fnid table in PLAYBOOK-REFERENCE §1.10),
  not direct register writes.
- run 32: +0xFED UART section → ring3 still frozen at 23061 → the console
  STILL didn't output. **Open question: why did run 23's kernel emit
  23 KB but run 32's kernel (with the UART section finally mapped) emit
  nothing?** Deltas to check next session: (a) is earlycon0 actually
  registering? add a bc[10]=0xC0DE0001 marker inside setup_early_printk();
  (b) is early_write being called at all? add a one-shot bc[10]=0xC0DE0002
  at its top; (c) the wrong-PA kicks (0x4A514030 writes, runs 24–29) may
  have clobbered something UART-related that persisted — but the device
  rebooted since, so unlikely; (d) check whether the monitor's capture
  only starts counting UART3 traffic AFTER some monitor event, or whether
  console=ttyO2 vs earlycon0 changes the output path.

## FILES TOUCHED THIS SESSION (all built and deployed at least once)

payload (kexec/qnx2linux.c): mon_call C-flow L2 disable at post-cgroup-era
position (bc 51/52 markers, readbacks bc[2]=ctrl bc[4]=dlat bc[6]=status),
pre-clean of globals+stack via NS by-PA (l2c_ns_clean_range), all mapdevs
pre-hold, GICD off pre-SMC, enter_stub reverted to the plain proven form
(+8th arg gicd view, unused), run nonce bc[15], the aborted auto-idle block
removed. kexec/stub3.S: cont SMC removed (MMU-off SMC never returns).
kexec/probe.S: bc[8]/bc[9] = PL310 ctrl/dlat readbacks + the long-loop
diagnostic (markers 80/81, 1000 passes). kexec/jump.sh: detached payload
launch (no SIGHUP), live bc polling, mirror readbacks, 0x40 dump.
kexec/retry-jump.sh: retry harness (marker compare uses hex 00000046/47!).
kernel head.S: WDT2 section 0xFEC (PA FIXED), UART section 0xFED (new),
r1/r2 save/restore around the stext smoke test (CRITICAL — the smoke test
clobbers the boot args: r1=0x20/r2=0 = the "invalid dtb" death), CIPA
guards in pbmark/pbmark3/pbmarkv, WDT kicks in all three macros.
kernel init/main.c: pb_bc WDT kicks, markers 171/172/173/174/175/186/187,
initcall breadcrumbs + per-initcall WDT kicks in do_initcall_level,
panic notifier writes ring3 via 0xD4000100.
kernel arch/arm/kernel/setup.c: CIPA guard + WDT kick in pb_bc.
kernel arch/arm/mm/mmu.c: PB_MMU_BC dual-channel markers (134/133/127/126/
125/99/98/97/96/95/129/128), fill_pmd_gaps+pci_reserve_io #if 0'd, 0xFEB/
0xFEC PMD-clear skip in devicemaps_init.
kernel kernel/taskstats.c: markers 181/182. kernel mm/slab_common.c:
markers 183/184/185. kernel kernel/cgroup/cgroup.c: markers 160–170 (pre-
existing) — NOTE the subsys markers go 164→170 per iteration.

## KEY REGISTER/ADDRESS FACTS (new this session)

- WDT2 @ 0x4A314000: TGR(trigger/reload)=0x30, SPR(start/stop)=0x48,
  CRR=0x28, LDR=0x2C, CLR(prescaler)=0x24. LDR=0xFFE2B400 → 58.6 s
  (32.768 kHz, PTV=0). Kick = ANY write to TGR (value ignored).
- PRCM: CM1 @ 0x4A004000 (MPU inst +0x300), CM2 @ 0x4A008000 (CORE inst
  +0x700: L3_1 +0, L3_2 +0x100, SDMA +0x300, MEMIF +0x400, D2D +0x500,
  L4CFG +0x600, L3INSTR +0x700; L3INIT inst +0x1300, L4PER +0x1400,
  ALWAYS_ON +0x600). CLKSTCTRL = instance + 0. CLKTRCTRL bits[1:0]:
  2 = SW_WKUP. ALL SECURE-FILTERED from NS (write = SIGSEGV).
- Device GP/HS check: 0x4A0022C4 = 0xae8 → HS device.
- Monitor SMC #0 service table (r12=service): 0x100 L2X0 DBG_CTRL,
  0x101 L2 clean+inv by PA (r0=PA,r1=size — reliable), 0x102 L2X0 CTRL
  (FLAKY mid-flight — use only from the C-flow mon_call shape),
  0x103/4/5 auxcoreboot, 0x108 SCU_PWR, 0x109 AUXCTRL, 0x113 PREFETCH.

## NEXT SESSION'S TASK LIST (in order)

1. **Read ring3's tail after every run** — the console log is the primary
   evidence now. memdump3 94000080 0x8 (count/index) then dump
   0x94000100+ (the 1 KB window, wrapped) — the TAIL = the death point.
2. **Why does the console emit nothing?** Add bc[10] markers inside
   setup_early_printk (registration) and early_write (first call) in
   arch/arm/kernel/early_printk.c — one run answers it. (Suspect: the
   console registers but early_write's ring mirror index frozen at 20
   means early_write ran once with ~20 chars = the CON_PRINTBUFFER replay
   of... something — or never ran.)
3. **The 171 wall**: if the console now works, the log will show the death
   directly. If it's the slab_mutex/schedule theory, try: (a) replace
   KMEM_CACHE(taskstats, SLAB_PANIC) with a kzalloc stub to skip it;
   (b) or dump the slab_mutex word into bc before the lock.
4. **L2 on, later**: with the console alive, try flipping L2 back ON via
   SMC 0x102 r0=1 at a safe point (the kernel has CONFIG_CACHE_L2X0=n;
   the L1-only machine is the current mode — see the pmOS config notes
   in docs/03 for the eventual L2-on endgame).
5. **Update docs/03 + this file after every run.** Keep the nonce discipline.

## OPERATIONAL RULES (unchanged, plus new)

- Ask before every device run. jump.sh from kexec/. WDT2 = 58.6 s.
- Payload SIGSEGV ≠ reboot (run 31) — SSH survives, iterate fast.
- The ring3 count/index CANNOT be trusted as fresh (the monitor re-writes
  them) — use the bc[15] nonce for the bc page and the content gap for
  ring3.
- memdump3 4a314028 0x8 (WDT CRR/LDR) = read the live watchdog state.
- Hard power-button reset wipes DRAM (the user confirmed) — use it when
  stale-content ambiguity blocks a readback.
