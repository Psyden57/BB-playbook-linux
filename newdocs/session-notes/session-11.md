# Session 11 Notes (2026-09-11 — the W-69 run + the rule-16 audit that
# disproved the "nosh" chain)

## THE RUN MAP (W-69 → W-70, kernel #139 → #140)

- **W-69 (kernel #139, --dmaquiet)**: bc[1]=145 STILL, no 146. BUT the
  evidence chain advanced massively: bc[27]=0xF5000002 (the setup.c F5
  span "post fdt call" = the FDT-recovery chain works end-to-end), the
  L2 OFF confirmed (bc[8]=0, was 1 in W-39), the sweep tally clean
  (bc[26]=0xBEEF0000), pv tries=0. Death region = the CMA remap's
  TLBIALL block between marker 145 and pb_bc_put(146).
- **W-70 (kernel #140)**: built, UNRUN (the session's first task after
  W-69). See below.

## CONFIRMED (the rule-16 audit — the session's unique knowledge)

1. **`dsb nosh` is not a valid GAS barrier name** ("invalid barrier
   type -- `dsb nosh'"); the valid v7 name = `nsh` (option 0x7 = the
   encoding f57ff047). EMPIRICALLY: arm-linux-as errors; `dsb nsh`
   assembles to f57ff047.
2. **GCC's integrated assembler SILENTLY accepted `dsb nosh` and
   emitted the SAME full-system mcr c7,c10,4 encoding** — verified in
   the shipped vmlinux (tlb-v7.S's W-66 sites = ee070f9a = mcr c7,c10,4
   = DSB SY). **The W-66..W-68 "nosh" conversions never changed a
   single barrier on the hardware** — they were no-ops, and every
   conclusion drawn from "the nosh barriers didn't fix X" is unfounded.
3. **The barrier-class field = bits[7:4] of the f57ff0XX encoding**:
   DSB=4 (f57ff04f = sy), DMB=5 (f57ff05f = sy), ISB=6 (f57ff06f = sy)
   — all verified via as+objdump. **The W-68 literal f57ff062 = the
   ISB-class with option 0x2 = UNPREDICTABLE on v7-A** — NOT a valid
   DSB. The W-69 death is most plausibly AT that literal (c0f07e40):
   F5-span landed pre-CMA, 145 = the last bc write.
4. **The mechanism: the A9 ACTLR bit 0 = FW = "cache and TLB
   maintenance broadcast"** (CMSIS-verified; bit 6 = SMP = "coherent
   requests"). QNX leaves ACTLR=0x41 (both); EVERY prior clear touched
   only bit 6 (re-read in both sites by the session-10 agent: start_
   kernel's W-45 clear and setup.c's W-48 re-clear were both ~(1<<6)).
   **FW=1 persisted through every run — every TLB op broadcast to the
   HELD CPU1 via the SCU regardless of encoding.** W-44's "the non-ISH
   c8,c7 TLB op STILL wedges" = the correct signal that the encoding
   never mattered.
5. The 2026-09-02 bss-clear wedge (bc[1]=120) = the SMP=0 + L2-ON
   cached-write class (not the FW broadcast — the bss clear is plain
   cached writes, no maintenance ops). **The L2 is off now** (mon_call
   0x102 inside --dmaquiet, bc[8]=0), so FW=0+SMP=0 is safe for it.
6. The payload's L2-off inside the proven --dmaquiet shape WORKS
   (W-69: bc[8]=0) — the Q2 answer's "the L2-off payload flow" is live.

## BUILD #140 (W-70) — the batch (per the session-10 agent's sign-off)

- All the "nosh" conversions REVERTED to plain dsb sy (restore-to-
  known-good — the encodings never changed, so this is NOT a variable).
- The f57ff062 literals (dma-mapping.c CMA flush, the mmu.c sweep ×3)
  restored to f57ff04f (real DSB SY).
- **BOTH ACTLR sites clear ~0x41 (SMP+FW)**: start_kernel's W-45 block
  (bc[3] = the post-clear readback = 0x0 = THE DISCRIMINATOR) and
  setup.c's W-48 re-clear.
- Packed 5,568,577 B (zImage 5,481,256 + DTB 87,321); shipped vmlinux
  verified (bic r0,r0,#65 at both sites, ZERO f57ff062, the CMA block
  = TLBIALL+f57ff04f+isb).
- kernel-patches regenerated. NOTE: tlb-v7.S was session-10-modified
  but MISSING from the snapshot list — added now (a snapshot gap found
  by the W-70 diff regeneration).

## W-70 EXPECTATIONS

- bc[3] = 0x0 (the FW+SMP clear took) = the discriminator.
- If the FW story holds: the boot PASSES the CMA TLBIALL block
  (146/147 land) — the first run past 145 ever with a deterministic
  mechanism. Then the old walls (161/167, the svm memset) — the
  agent's Q4: "don't expect the old walls — the L2-off + sweep +
  verified barriers changed the game; expect a new site; the run
  decides."
- If the boot STILL dies at 145/146: the FW story is wrong — bisect
  the TLBIALL vs the DSB with bc[27] phase markers in the CMA block.

## ERRATUM CARRIERS (per the docs rules — never silent)

- docs/03 W-66/W-68/W-69 entries + PROJECT_STATE.md item 4: the "nosh"
  mechanism text is SUPERSEDED by the rule-16 audit (the encoding
  never mattered; the real class = ACTLR.FW).
- BOOTSTRAP_SESSION_11.md keeps its historical text (the erratum lives
  here + in docs/03 W-69).
