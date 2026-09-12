# Session 12 Notes (2026-09-12 — the --l2on erratum confirmed, W-84 run,
# the L2-on state regresses the boot to the early-C region)

## THE HEADLINE

1. **The --l2on erratum (found in the W-84 archaeology, verified by the
   session-11 agent, erratum 86c4e29)**: the --l2on mode disabled the L2
   since the W-46 rewrite (a692300) — every W-78..83 "L2-on" run ran
   L2-OFF. The session-11 "L2-state-dependent wedge family" framing =
   superseded. The variable that moved the front in W-78 = dropping the
   devb slay.
2. **W-84 = the first GENUINE L2-on run since W-46** (the payload's era
   --l2on branch restored: no SMC, bc[6]=0x2102, bc[10]=CTRL expect 1;
   kernel #153 unchanged; rule-16 verified in the shipped binary).
   RESULT: **the boot REGRESSED to a death between markers 144 and 145**
   (the early-C region) — neither predicted outcome (past the CMA /
   wedged at a TLB op).

## W-84 READBACK MAP (the fresh-vs-stale discipline that cracked it)

- **Fresh (bc[0..5] = bc_arm-sanitized at arm-up)**: bc[1]=142 (the last
  bc[1]-slot write; the next writer = 145, never reached), bc[2]=0x3E7
  (the W-6/W-35 zImage-correlated wild write — back), bc[3]=0xa2e00000
  (the placement), bc[4]=143, bc[5]=144 (the head.S fixup-region
  surviving-slot markers — fresh because of the sanitizer).
- **Fresh (payload/stub/probe slots)**: bc[10]=1 (the pre-jump PL310
  CTRL readback — THE L2 ON), bc[8]=1 (probe.S's post-jump CTRL
  readback — THE L2 ON), bc[9]=0x111, bc[12]=0xa3302cd8 (__atags_pointer
  = the payload's standalone DTB), bc[14]=0xff8ed7b8 (the fixed-map VA
  of the bc[20]-recovered appended DTB: 0xff800000|0xed7b8;
  0xa32ed7b8&0xFFFFF=0xed7b8 ✓), bc[16]/bc[17]=0/0 (the DISPC kill),
  bc[18]=0xa2e00000 (the placement, clean vs the guard), bc[20..25]
  (the decompressor dump: _edata=placement+0x4ed7b8, the magic, r8, r4,
  r2@stext=the appended DTB, machine=0xffffffff).
- **W-83 residue (proven, not assumed)**: bc[26]=0xBEEF0000 (the sweep
  tally), bc[27]=0xE1000003 (the post-ISB phase), bc[28..31]=0x041e
  shapes — the SMP=n desc shape (the S bit/bit16 is OR'd into
  MT_MEMORY_RWX.prot_sect ONLY from the SMP TTB flag, mmu.c:619; SMP=n
  kernels write 0x0041e, SMP=y kernels 0x1141e). W-83 (the SMP=n kernel
  that reached 126, past the dump point) wrote exactly these.
- **bc[6]=0xe0000000 = the stub echo** (era-documented; the payload's
  0x2102 got overwritten by the stub — the era comment predicted this).
- **RING1 = 0 = STRUCTURAL, not a death point**: the W-77 de-CIPA made
  the ring writes plain L1-dirty stores; with the L2 ON they never reach
  DRAM → lost at the WDT2 reset. The console = UNAVAILABLE in the L2-on
  state until the ring flush is restored (pb_bc_put survives because it
  always DCCMVACs → the L2 retains across the warm reset).

## THE W-84 MODEL UPDATE

- The death span = [144 (head.S fixup done) → 145 (the CMA pmd_clear
  done)]: __mmap_switched → start_kernel → setup_arch, with the FDT
  chain demonstrably complete. The earlycon never registered (ring=0 is
  consistent — it registers at parse_early_param, after
  setup_machine_fdt).
- The W-35 triad (bc[1]=142 + 0x3E7 + ring 0) reproduced with a CLEAN
  placement (0xa2e00000, well outside the guard) → the W-35 mechanism
  (the placement overlap) does NOT apply; the L2 state = the delta vs
  W-83 (which reached 126 with the L2 off).
- **The two L2 states fail at DIFFERENT points with the SAME kernel**:
  L2-off → the first TLB op (clear_fixmap, 126→125); L2-on → an early-C
  op before the CMA pmd_clear (144→145). The L2 is neither a cure nor
  dead as a variable. The wedge family stays SCU-routed-global-ops with
  CPU1 held; WHICH op fires first = L2-state-dependent.

## THE MODE MATRIX (after the W-84 payload fix — IMPORTANT)

| mode | L2 | devb slay | notes |
|------|----|-----------|-------|
| --l2on | ON | no | W-84's mode; the era semantics restored |
| --dmaquiet | **ON** | yes | NO LONGER disables the L2 (the W-84 fix removed the SMC from the shared branch) |
| --t3 | OFF | no | the only L2-off mode now (the else branch) |

## NEXT (W-85)

The slay A/B under the L2-on: `PAYLOAD_MODE=--dmaquiet ./jump.sh zImage`
(the slay = the one variable vs W-84). Ideally after a battery pull
(DRAM+L2 wiped = a clean machine — the user volunteered; also removes
all residue noise from the readback). If W-85 passes 145+ → the slay
correlation strengthens; if it dies ~144/145 → deterministic early-C
L2-on breakage.

Standing candidate after the slay A/B: restore the ring flush for the
L2-on state (the console evidence) — the ring needs its batch CIPA/SMC
flush back, or a DCCMVAC-per-batch in the ring putchar.

## RULES RE-PROVEN THIS SESSION

- Rule 16 (verify the shipped binary) — the payload fix was verified by
  disassembly before the run (the SMC gone from the g_l2on path).
- Rule 13 (python for hex) — the 0x041e-vs-0x141e shape decode, the
  fixed-map VA check (0xff8ed7b8 ↔ 0xa32ed7b8), the placement-overlap
  check all went through python.
- Rule 14 (grep, don't recall) — the S-bit/SMP claim, the marker order
  in dma-mapping.c, and the era branch shape all verified in the tree.
