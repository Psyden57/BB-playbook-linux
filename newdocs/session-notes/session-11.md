# Session 11 Notes (2026-09-11 — the rule-16 audit, the L2-state discovery,
# the exclusive-op wedge, the CP13 class, and the CMA finally passed)

## THE RUN MAP (W-69 → W-83, kernels #139 → #153, 15 runs, 15 builds)

| run | build | change | result |
|-----|-------|--------|--------|
| W-69 | #139 | (as inherited) | 145; the rule-16 audit found the "nosh" chain never existed + the f57ff062 literal = the death |
| W-70 | #140 | the ACTLR ~0x41 clear (both sites) | bc[3]=0x0 took; 145 — ACTLR exonerated |
| W-71 | #141 | the in-block phases | NO E1 phase = the wedge at the TLBIALL/the first re-walk |
| W-72 | #142 | the TLBIALL SKIPPED (the A/B) | ★ 126 — the CMA block PASSES; the death = clear_fixmap; the pgd dumps |
| W-73 | #143 | the CPU1 park | 145 + BROKE the post-reset recovery (the SAR resurrection) |
| W-74 | #144 | the poison-pair zeroing | 145 |
| W-75 | #145 | the C/B/S descriptor strip | 145; the strip live; the "poison pair" = valid descs (the W-37 misread) |
| W-76 | #146 | the SCTLR C/I=0 | 145 — the cache state exonerated |
| W-77 | #147 | the de-CIPA boot path | 145 — the dead-line-op theory falsified; the 13-min dark mystery (the W-73 breaker) |
| W-78 | #148 | --l2on | ★ 150 — PAST the CMA. **ERRATUM (session 12): the L2 was OFF (--l2on disables it since the W-46, a692300; bc[8]=0 in all runs) — the real variable = dropping the devb slay. The "L2-off-specific" framing below = superseded.** |
| W-79 | #149 | the SMC sweep (0x101) | 150 — deterministic; the culprit = the CP13 write |
| W-80 | #150 | CONFIG_SMP=n | 145 (the TLBIALL back); the CONSOLE RETURNS (1165 chars) |
| W-81 | #151 | SMP=y + the TPIDRPRW skip | 150 — the CP13 write not the (only) wedge |
| W-82 | #152 | the window phases | bc[27]=E1000006 = THE FIRST PRINTK WEDGES (the logbuf spinlock = the first ldrex/strex) |
| W-83 | #153 | SMP=n + the TLBIALL skipped + the L2 on | ★ 126 — past the CMA AND the printk; the death = clear_fixmap |

## THE UNIFIED WEDGE FAMILY (the session's headline discovery)

**ERRATUM (session 12, 2026-09-12): the L2 was OFF in ALL 15 session-11
runs — the --l2on mode has disabled the L2 since the W-46 rewrite
(a692300; the on-device witness = probe.S's bc[8] = the PL310 CTRL
readback = 0 everywhere; the W-39's pre-W-46 bc[8]=1 = the L2 on). The
"L2-on/L2-off" attribution below is WRONG: the variable that moved the
front = dropping the devb slay. The correct matrix: the TLB ops wedge
under (the slay + SMP=y) 6/6 and under (SMP=n, no slay) 2/2, but pass
under (no-slay + SMP=y) 2/2 — no single variable explains it; the runs
23-32 (the L2 off, no slay, the old kernel) completed the TLB ops to 171,
so the L2 state is exonerated. See docs/03's erratum.**

**The machine wedges on SCU-routed global ops with CPU1 held:**
1. **The TLB maintenance ops** — deterministic with the L2 OFF (the
   W-72..77 era), flaky with the L2 ON (the W-78/79 passed the TLBIALL,
   the W-80/83 wedged at the TLB ops).
2. **The ldrex/strex exclusives (the spinlocks)** — deterministic with the
   L2 ON + SMP=y (the first printk = the logbuf lock = the kill site, 3/3);
   harmless with !SMP's plain spinlocks (the W-80 passed the printk).

## RULED OUT (with direct on-device evidence — never re-test)

- The barrier domain/encoding: `dsb nosh` = an invalid GAS name; GCC's
  integrated assembler SILENTLY accepted it and emitted the same
  full-system mcr — the session-10 "nosh" chain never existed on the
  hardware. The W-68 f57ff062 literal = an ISB-class encoding with an
  invalid option (UNPREDICTABLE on the A9) = the W-69 death site.
  Valid encodings (verified): DSB=f57ff04n (sy=F), DMB=f57ff05n,
  ISB=f57ff06n; the class field = bits[7:4].
- ACTLR.SMP (bit 6) and ACTLR.FW (bit 0): all three states wedged
  (0x41/0x1/0x0). The ~0x41 clear is KEPT (harmless, matches maxcpus=1).
- CPU1 parked vs held: the park failed to cure the TLB ops AND broke the
  post-WDT2-reset recovery (the PRCM hold bit persists across resets;
  the released CPU1 resurrects QNX via the SAR path = the 11-min dark
  device). **THE PARK IS FORBIDDEN** until the SAR neutralization
  (0x4A326B00, NS-writable) + the kernel-side re-hold exist.
- The pgd content: the W-74 zeroing = no cure. AND the "poison pair"
  [0xdfc/0xdfd] = actually VALID section descriptors in every era — the
  W-37 "QNX-era table pointer" reading was an attribute-bit misread
  (0x141e = the cached shape, 0x1412 = the uncached shape). The W-75
  strip is live and proven via the bc[28..31] dumps.
- The descriptor cacheability (the W-75 strip) and the SCTLR cache state
  (the W-76 C/I=0): both exonerated.
- The CP13 TLS writes: the W-63 class is real (the W-78/79 = 150 with the
  write, the W-80 = passed without it) but the W-81 skip = still 150 = the
  print-side wedge dominated. The set_my_cpu_offset skip is KEPT.
- The NS SCU CTRL write: FILTERED (the scuprobe — reads fine, the write
  survives silently, the enable bit unchanged). The box survived.

## THE STATE OF PLAY AT THE WRAP

- The run mode = **--l2on** (the L2 stays on; --dmaquiet's L2-off = the
  deterministic TLB-op wedge).
- The kernel = **CONFIG_SMP=n** (the UP spinlocks; maxcpus=1 was already
  the de-facto shape; the SMP bring-up = the bequest).
- The marker front = **the first TLB op after the CMA (clear_fixmap in
  early_fixmap_shutdown)**, bc[1]=126→125.
- The console = alive (the earlycon ring; the era's proven channel).
- The payload flow = the LAST untested variable for the TLB-op wedge: the
  runs 23-32 era (--t3, the old minimal flow) = the only era where TLB
  ops ever completed. THE NEXT SESSION'S OPENING MOVE = the git
  archaeology: the era's qnx2linux.c + the current kernel, then bisect
  the payload deltas (the DISPC kill, the devb slay, the CIPA sweeps, the
  memtest, the placement mechanics).
- If the payload bisect fails: the real cure = the CPU1 release done
  RIGHT (the SAR neutralization + the kernel-side re-hold + the park
  blob), or the full SMP bring-up. The LED-progress encoding (the
  user's idea) = a cheap add-on: the kernel already drives GPIO1_13
  (the SETDATAOUT write at marker 111) — a per-phase/per-initcall
  heartbeat = the video-visible death point for the unrecoverable runs.

## TOOLCHAIN FACTS (keep)

- `dsb nsh` = the valid v7 name (f57ff047); `dsb nosh` = invalid.
- GCC's integrated assembler silently accepts invalid barrier names and
  emits the mcr fallback — rule 16 (the shipped-binary verification) is
  the only defense.
- The A9 ACTLR: bit 6 = SMP ("coherent requests"), bit 0 = FW ("cache and
  TLB maintenance broadcast") — the CMSIS-verified map.
- memdump3 90000040 0x40 = the extended slots incl. bc[28..31].
