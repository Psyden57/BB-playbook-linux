# Session 11 Notes (2026-09-11 — the W-69 rule-16 audit, the W-72 A/B, and
# the TLB-op wedge model)

## THE RUN MAP (W-69 → W-72 + the scuprobe, kernel #139 → #142)

- **W-69 (kernel #139)**: bc[1]=145 STILL. But the rule-16 audit
  (shipped-vmlinux) DISPROVED the session-10 "dsb nosh" chain (see
  CONFIRMED below); the W-69 death = the f57ff062 literal itself.
- **W-70 (kernel #140, the batch)**: bc[3]=0x0 (the ACTLR.FW+SMP clear
  TOOK) yet bc[1]=145 STILL — the ACTLR theory died.
- **W-71 (kernel #141, the in-block bisect)**: bc[27]=the F5 residue —
  NO E1 phase landed = the wedge is AT the TLBIALL mcr or the first
  fetch/access after it.
- **W-72 (kernel #142, the A/B: TLBIALL SKIPPED)**: ★ **bc[1]=126 — the
  boot PASSED the entire CMA block** (all four E1 phases, 146/147/126
  landed). Death moved to early_fixmap_shutdown (126→125) = the NEXT
  TLB-op site. **THE TLBIALL mcr = the deterministic machine wedge.**
  The pgd dumps landed: bc[28]=0xbfa1141e (correct section), **bc[29]=
  0xbfc1141e / bc[30]=0xbfd1141e = the QNX-era poison pair LIVE**,
  bc[31]=0.
- **scuprobe (in-QNX)**: SCU_CTRL=1 (enabled), CFG=0x511, PWRSTS=
  0x03030300; the NS write to disable = **FILTERED** (still enabled).
  The NS SCU path = closed. Box alive.

## THE CONSOLIDATED MODEL (the session's unique knowledge)

**ANY TLB maintenance op post-jump wedges the machine.** TLBIALL (the
W-72 A/B is PROOF), per-page TLBIMVA (W-43/W-67's loop), clear_fixmap
(126→125). The boot does NO TLB op before the CMA block (head.S builds
its tables MMU-off; pbmarkv = cache ops) — the CMA's TLBIALL = the
FIRST TLB op of every boot = deterministic. **Known since session 1**:
stub3.S avoids the TLBIALL ("wedged with CPU1 in reset; redundant") —
the connection to the kernel's TLB ops was never made. Prime suspect =
the SCU routing TLB-op broadcasts to CPU1's dead port.

What is now RULED OUT (all with direct evidence):
- The barrier domain/encoding: `dsb nosh` never existed on the
  hardware (GNU as rejects it; GCC's IAS silently emitted the same
  full-system mcr); the f57ff062 literal = an ISB-class encoding with
  an invalid option (UNPREDICTABLE on the A9). f57ff04f = the only
  DSB encoding that ever shipped, and it's innocent (W-72 passed with
  it in the block).
- ACTLR.SMP (bit 6) and ACTLR.FW (bit 0): all three states wedged
  (0x41 / 0x1 / 0x0). FW=0+SMP=0 is now the standing config (it costs
  nothing and matches the single-core reality).
- The DSB/ISB themselves: exonerated by position (W-71's phases).

## CONFIRMED (the toolchain fact worth keeping)

- `dsb nsh` = the valid v7 name (f57ff047); `dsb nosh` = invalid.
- The v7 barrier encodings: class field = bits[7:4] (DSB=4, DMB=5,
  ISB=6), option = bits[3:0]. f57ff04f/f57ff05f/f57ff06f = sy.
- **GCC's integrated assembler SILENTLY accepts invalid barrier names
  and emits the mcr c7,c10,4 (full-system) fallback** — rule 16 (the
  shipped-binary verification) is the only defense; the build log
  lies.

## THE PGD POISON STATUS

The pair [0xdfc/0xdfd] = 0xbfc1141e/0xbfd1141e = LIVE QNX-era TABLE
descriptors (measured in-boot, W-72's bc[29]/bc[30]). The mmu.c sweep
= CONSISTENCY-ONLY (pre != post) — the #111 plan's "re-write the
section descs" half was NEVER implemented. With the 2MB shave nothing
is allocated there, so the poison is harmless UNTIL something walks
those sections — but the pair SHOULD be zeroed/rewritten in the same
pass that fixes the TLB-op problem (the walks after any TLB
invalidation re-read the pgd).

## BEQUEST to session 12 (the CPU1-parking investigation)

The TLB-op wedge makes the CPU1 release load-bearing. The shape:
1. Payload-side: zero/neutralize the SAR wake context (0x4A326B00,
   0x150 B — NS-writable per the session-1 devpm RE) so the ROM's
   monitor-verify fails.
2. Release CPU1 (the inverse of the payload's proven RSTCTRL hold
   write — the same register family, 0x4824380C/0x4824340C).
3. CPU1 re-enters the ROM's CPU1 path (0x40028134: cpunum, the wfe
   loop, the AUX_CORE_BOOT flag) — with the SAR context invalid, the
   expected fallback = parked in the ROM's poll loop = ALIVE = the
   SCU's CPU1 port lives = the TLB broadcasts complete.
4. The kernel then boots with CPU1 parked (maxcpus=1) — restore the
   TLBIALL (the proper flush) and proceed.
RISKS: the ROM's invalid-context behavior is UNVERIFIED (may not fall
back to the poll); the RSTCTRL release write from NS is UNPROVEN (the
hold is proven, the inverse is not); a wrong release = CPU1 resumes
QNX mid-boot = the run's death (recoverable, evidence preserved).
Cheap first probe: read the SAR context + the RSTCTRL registers from
the payload (in-QNX, no jump) before any release attempt.
ALTERNATIVES if the parking fails: (a) the undocumented monitor
services (0xF0 unidentified; 0x103/0x107 = the ROM's own CPU1-path
sites); (b) the full SMP bring-up (the kernel wakes CPU1 itself).

