# SESSION RECORD — 2026-09-02: L2 WEDGE THEORY CONFIRMED BARE-METAL; monitor RE complete; SMC 0x102 hang matrix

## HEADLINE
**The early-C wedge is THE MACHINE, not the kernel.** Run 10's probe.S
long-loop diagnostic — bare-metal, no kernel, no QNX, no monitor call, no
DMA of ours — stalled silently at pass ~11000/16384 (~11 GB of pure CPU
cacheable write-back stores through L1→L2→DRAM) with PL310 control=1 and
data-latency=0x111. No abort (flag 0), rings clean, WDT2 60 s later.
The kernel's early-C deaths (bc 145/168/150) are the same phenomenon: the
kernel was merely the first sustained WB traffic generator. Also explains
QNX's historic flakiness (RNDIS timeouts, sync() hangs) and the monitor's
own flaky 0x102 disable.

## RUN LOG (this session, all with the ring-map fix + fine markers in place)
| Run | Change tested | Result (bc readback) |
|-----|---------------|----------------------|
| 1 | payload: pre-hold SMC 0x102 disable + post-disable printf | wedged IN printf post-disable (stranded dirty L2 lines); bc=51, bc[2]=0 (disable worked!) |
| 2 | disable moved post-hold, readbacks bc[2]/bc[4] | bc=52 landed (disable OK), wedged at post-disable GICD mapdev — same mechanism |
| 3 | all mapdevs pre-hold, disable LAST before WDT/enter_stub | bc=52, bc[2]=0, bc[6]=0 — clean disable; then died before tramp bc=63: post-disable L1 misses on stranded dirty lines (clean_inval had written L1 dirty INTO L2) |
| 4 | disable moved into cont (MMU off) | **cont bc=23 landed, bc=24 never — SMC with MMU OFF never returns** |
| 5 | disable in enter_stub (MMU on) after cpsid if, garbage r1 | mirror0=70 (pre-SMC) — SMC hung |
| 6/7 | enter_stub, r1 zeroed (7), GICD-off via constant PA-as-VA | died before bc=70 — **my bug: str to unmapped VA 0x48241000 aborts** |
| 8/9 | perfect context (r0-r3=0, GICD on, IRQs on), mirror markers 70/71 | **70 without 71 — SMC hangs in the perfect context** |
| 10 | SMC removed; probe long-loop diagnostic (L2 ON) | **probe ran fully (50-59, DTB/PL310 reads OK), wedged at pass ~11000/16384 — MACHINE CONFIRMED** |

## MONITOR SERVICE TABLE (authoritative RE, no guessing)
Source: mainline `arch/arm/mach-omap2/omap-secure.h` + `omap-smc.S` +
`sleep44xx.S` (TI's own resume path), anchored by the on-device-verified
0x101. `dumped4869ifs/proc/boot/trustzone-omap4` is NOT the dispatcher —
"QNX-packed" only meant a stripped section table; full disassembly shows
zero SMC instructions (it's the secure-debug/crypto resmgr: ECDH/RPMB/
SHA512 via libhuapi). The services live in the TI ROM monitor.

SMC #0, r12 = service:
- 0x100 = L2X0 DBG_CTRL write (r0=value)
- **0x101 = L2 clean+inv by PA (r0=PA, r1=size) — verified on-device**
- **0x102 = L2X0 CTRL write (r0=value) — enable/disable**
- 0x103/0x104/0x105 = auxcoreboot read/modify/addr — **solves the 0x105
  anomaly: it was never an L2 service** (likely disturbed CPU1 boot state,
  breaking the jump chain)
- 0x108 = SCU_PWR write; 0x109 = L2X0 AUXCTRL write; 0x113 = L2X0 PREFETCH
- **NO tag/data-latency service exists** — the 0x333 data-latency plan is
  impossible via SMC (mainline omap4_l2c310_write_sec handles only
  CTRL/AUX/DBG/PREFETCH).

Device facts: GP/HS check per pmOS wiki: 0x4A0022C4 = 0xae8 →
((>>8)&0x7)=2 ≠ 3 → **HS device** (consistent with secure-filtered PL310).

## SMC 0x102 HANG MATRIX (r12=0x102, r0=0) — do not retry blind
2 returns / 5 hangs across 7 attempts. No context makes it reliable:
not MMU-on/off, not GICD state, not IRQ state, not arg registers, not
position. Diagnosis: the service's internal sequence (almost certainly a
secure-side background by-way clean before the CTRL write) races foreground
traffic — PL310 r3p2 727915-class — the SAME marginal-L2 instability the
long-loop proved. A hang costs one WDT2 reboot (~2-3 min).

## OTHER FINDINGS / FIXES (runs 1-9)
- jump.sh hardening: payload launched DETACHED (`>/tmp/jump.log 2>&1 &` —
  the 120 s SSH timeout SIGHUP-killed it in run 1); live bc polling with
  transient-SSH tolerance (4-miss rule); mirror readbacks after reboot
  (mirror0 0x94000004 carries enter_stub markers 70/71).
- Kernel: ALL CIPA sites guarded on PL310 control bit0 (skip CIPA+sync when
  CTRL=0 — bare stores are DRAM-durable with the L2 off): head.S
  pbmark/pbmark3/pbmarkv macros + 2 hand-rolled blocks in __turn_mmu_on +
  main.c (118/111) + setup.c (110). Behavior-neutral when L2 on. KEEP.
- probe.S: PL310 control/dlatency readbacks into bc[8]/bc[9] (post cache-id).
- Payload: cacheable alias table entry tt[0x700] = 0xB0000000|0xC06 (high
  DRAM) for the long-loop; enter_stub takes a mapped-gicd 8th arg.
- GAS quirk: a numeric label on the SAME line as `.endm` inside a macro →
  "unexpected end of file in macro definition". Keep .endm on its own line.
- strided lesson: a physical address used as a VA (0x48241000) is an
  unmapped QNX VA → silent data abort. Always use mapdev views.

## REFERENCE MINING (user-supplied)
- pmaports linux-postmarketos-omap config (working OMAP4 reference):
  CONFIG_CACHE_L2X0=y + PL310_ERRATA_588369 + PL310_ERRATA_727915 +
  ARM_L1_CACHE_SHIFT_6 + ARM_ERRATA_798181 + SMP/SMP_ON_UP + KEXEC.
  No PlayBook port exists (wiki codename only) — **we are first**.
- droid4-kexecboot (tmlind, same SoC): kexecboot-as-bootloader in a flashed
  boot partition + utag config; mainline kexec works daily; "stock 3.0.8
  kernel does not survive kexec" — mainline does. Nothing to port; our
  QNX-runtime-kexec stays (no flashable boot partition on PlayBook).
- Endgame for L2-on: CONFIG_CACHE_L2X0=y + the two PL310 errata — mainline
  omap4 drives secure PL310 writes through the monitor SMCs we verified.

## NEXT (in order)
1. Reliable L2 disable: pre-drain the ENTIRE L2 by-PA via 0x101 (foreground,
   proven-safe, with sync polls) over DRAM before the 0x102, and/or accept
   retry-until-works (each failure = one WDT2 reboot). Verify via probe
   bc[8]=0 (control) / bc[9]=0x111.
2. Re-jump the no-L2X0 kernel with L2 OFF → early-C wedge should VANISH
   (bc past 143→136→130→110→111). Then decide the L2 strategy: L1-only may
   be acceptable short-term (the monitor has no latency service; enabling
   with 0x111 latencies risks the same instability).
3. If the SMC proves unusable even pre-drained: alternative = never disable,
   accept the machine's WB-traffic flakiness, and make the KERNEL resilient
   (or test 0x109-aux changes — e.g., disabling L2 "cache maintenance
   bypass"/parity bits — as a lower-risk monitor call).

## BLOCKED PATHS (do NOT retry)
- SMC 0x102 in any single position/context (unreliable; matrix above)
- NS writes to PL310 config regs; NS by-way 0x7FC; monitor 0x105
- MMU-off SMC (cont position)

## FILES TOUCHED
- kexec/qnx2linux.c (enter_stub: SMC experiments removed, mapped-gicd arg,
  markers; table alias tt[0x700]; pre-hold printf + tramp flush)
- kexec/stub3.S (cont SMC removed; MMU-off SMC hang documented)
- kexec/probe.S (bc[8]/bc[9] PL310 readbacks; long-loop diagnostic 80/81)
- kexec/jump.sh (detached launch, bc polling, mirror readbacks, 0x40 dump)
- kernel arch/arm/kernel/head.S, init/main.c, arch/arm/kernel/setup.c
  (CIPA guards on PL310 control)
- docs/03_DEBUGGING_SESSIONS.md (this record)
