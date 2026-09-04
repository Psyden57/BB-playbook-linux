# Session 8 Notes (2026-09-04 — the fixup delivery paradox, the inline
# fixup, the bank mismatch, and the pv stale-line mechanism)

## THE RUN MAP (W-9 → W-24, all --t3/--l2on, kernel #88 → #101)

| Run | Build | Change | Result |
|---|---|---|---|
| W-9 | #88 | flushed fixup instrumentation (W-8 edit) | 130; bc[6] = probe magic — fixup never entered |
| W-10 | #88 | --t3 L2 OFF | 130; fixup never entered → L2 exonerated |
| W-10b | #88 | W-10 re-run for timings | identical; LED: blue-off = reset instant (no probe) |
| W-11 | #89 | inline-test (fixup's first stores in head.S) | **142 lands, bc[6]=0xa0000000** — stores work inline, bl never delivers |
| W-12 | #90 | computed blx (target → bc[13]) | target correct (0xa00088d4), still no delivery |
| W-13 | #91 | fixup-entry sentinel (bc[7]) | sentinel absent — the fixup never stored anything; **trample race won: fixup bytes byte-correct in DRAM** |
| W-14 | #92 | SCTLR.I cleared | no delivery → I-fetch mechanism exonerated |
| W-15 | #93 | smoke test disabled | no delivery → smoke test exonerated |
| W-16 | #94 | ALL CIPA stripped pre-fixup | no delivery → CIPA exonerated; 142/bc[13] survive with NO flush (SO stores reach DRAM) |
| W-17 | #95 | **the pv fixup INLINED** | **★ 142/143/144 → 127 → THE C WORLD, 1169-char console, DTB parsed, CMA reserved, PB-CMA all-correct** |
| W-18 | #95 | --l2on | 146; identical console → L2-independent |
| W-19 | #96 | W-14 I-clear removed | 146; identical → I-fetch exonerated |
| W-20 | #97 | remap markers 145/146/147 | **death MOVED to 133 (map_kernel)** — layout-dependent!; no decompression hole (1024 bytes verified); PB-MEM exposed the bank mismatch |
| W-21 | #98 | DTS bank → a0000000+512MB | 146 (past map_kernel/127); **pv_off=0 in the C world** |
| W-22 | #99 | fixup markers → surviving slots (bc[4]/[5]/[6]) | **143/144/0xe0000000 all land — the inline fixup EXECUTES**; pv_off=0 persists |
| W-23 | #100 | .data CIPA flush (head.S) | pv_off=0 persists; **post-mortem: DRAM holds the CORRECT pv values** |
| W-24 | #101 | C-world dual-level invalidate (DCCIMVAC + SMC 0x101) | **★ pv_off=ffffffffe0000000 in the console, BUG gone; death = 146 (iotable_init) — a NEW pv-independent wall** |

## CONFIRMED (facts established this session, all grepped/read/verified)

1. **The fixup delivery paradox is real but was never mechanistically
   solved** — the called fixup never executed a single instruction
   through bl, computed blx (verified-correct target), both L2 states,
   I=0/1, with byte-correct DRAM (W-13 post-mortem) and the fixup's
   first line shared with __vet_atags's executed tail. INLINING the
   fixup bypassed it. MECHANISM UNKNOWN — the session-9 open question.
2. **The delivery paradox is LAYOUT-DEPENDENT** — W-20's 60-byte
   dma-mapping.c shift moved the C-world death (map_kernel) — the same
   code at different addresses behaves differently.
3. **SO bc-page stores reach DRAM directly** (W-16: 142/bc[13] survived
   with NO flush) — the W-6/W-7 "dirty-discard" revision was wrong for
   the bc page, and the entire pbmark CIPA machinery is unnecessary for
   it (the W-8-era flush was added on the wrong theory).
4. **The D-side stale-line mechanism (W-22→W-24)**: the inline fixup's
   .data stores reach DRAM (post-mortem: __pv_offset =
   0xffffffffe0000000 at a100a8d4) but the C world's CACHED reads hit
   decompressor-era stale lines. I-side = fresh (the decompressor's
   ICIALLU); D-side = stale. THE FIX: the C-world dual-level invalidate
   (L1 DCCIMVAC by VA + the SMC 0x101 L2 by PA) = WORKS (W-24).
5. **The bank mismatch = the real 127 root cause on the zImage path**:
   AUTO_ZRELADDR always lands zreladdr = 0xa0008000 (the relocated
   decompressor's 128MB bucket) → PHYS_OFFSET = 0xa0000000, while the
   baked DTS bank was 0xa4000000+448MB — the kernel sat OUTSIDE its own
   memory. fdt_patch_memory patches only the SEPARATE DTB; the zImage
   uses the APPENDED one. Fix: bank = 0xa0000000+512MB (W-21).
6. **W-6's bc=107 was ambiguous** — 107 was BOTH post-fixup_smp AND
   addruart-done pre-renumbering; the bootstrap's "W-6 = post-fixup_smp"
   reading may have been wrong (W-6 could have died in the smoke test).
7. **The dtb-phys chain closes exactly** for W-8/W-10/W-11/12/13-era
   builds (bc[3] = the BUFFER base, qnx2linux.c:935; kern_off =
   round-up-2MB(phys)-phys; load = kern_phys+0x8000; dtb = load +
   round8(packed)). W-6's closure = inference (the #85 blob is lost).
8. **bc[10]/bc[11]/bc[12]/bc[13]/bc[14] writers**: bc[11] = 0xC0DE0010
   (parse_early_param entered, main.c:780); bc[12] = 0xC0DE0020
   (parse_early_param first-call completion, main.c:799) or the cont's
   r7 (dtb_phys — pre-kernel); bc[10] = the cmdline head or
   0xC0DE0001/2 (early_printk.c); bc[13] = the cmdline length (main.c)
   or bss-len (head-common) or W-22's delta store; bc[14] = 0xC0DE0030
   (the parse double-call latch).
9. **The power-hold "hard reset" = a WARM reset** (DRAM survives —
   W-4-era residue survived it). Only the TWL6030 full power-off loses
   DRAM.
10. **bc[2]=0x3E7 (999) = PROBE+zImage-correlated** — present with the
    probe in either L2 state (runs 23-32, W-6..W-9), absent with --t3
    (W-10..W-16). Writer still unidentified.
11. **The ring2 region (0x90000200+)** holds STALE Image-era console
    text (the W-4 banner — "Thu Sep 3 02:05:36 UTC 2026" = the kernel's
    BUILD timestamp, host clock); the ring2 index @0x90000080 = sane.
12. **jump.sh's timing ladder matters** — stop piping through `tail`.
    SSH-gone t: 30-45 s across the session.

## MECHANISMS / MODELS (working, not proven)

- **The D-side stale-line mechanism** (the session's best-established
  new model): the decompressor writes the image cached; its
  cache_clean_flush cleans L1→L2 (the L2 lines stay VALID with image
  content); MMU-off SO stores from the kernel go to DRAM but do not
  coherently update what the C world's cached reads hit; the C-world
  dual-level invalidate (DCCIMVAC + SMC 0x101) fixes it (W-24 ✓).
  OPEN: why W-17/18/19's pv reads were CORRECT with L2 on (eviction
  luck? the layout?); why the head.S L2-only CIPA didn't cure it.
- **The delivery paradox**: unexplained. The inline fixup = the
  workaround. If it resurfaces (it is build-luck-dependent — #95/#96
  worked, #97+ needed the W-24 flush — NOTE: the W-24 flush also
  explains part of #97+'s pv=0... NO — #97/#98's pv=0 predates the
  W-24 flush; the fixup EXECUTED in #99 (W-22) at the same address
  where #97/#98's didn't?? — actually W-22 = #99 WITH the surviving
  markers: the fixup executed in #99; whether it executed in #97/#98 is
  UNKNOWN (their markers were in bc[1], overwritten). The paradox may
  be PER-RUN nondeterministic rather than per-build.)

## CORRECTIONS to older records

- W-6's "bc=107 post-fixup_smp" → ambiguous (the duplicated 107); W-6
  may have died in the smoke test.
- W-6's "zreladdr 0xa0080000" → WRONG: zreladdr = 0xa0008000 (128MB
  bucket + TEXT_OFFSET; confirmed by the W-13 dump + r8 = 0xa0000000).
- The W-8 record's "bc[3] = buffer/kern base" → bc[3] = the BUFFER base
  (qnx2linux.c:935); kern_phys = buffer + kern_off.
- The W-9 record's "power-hold = useful for clean residue" → WRONG: the
  power-hold = a warm reset, DRAM persists.
- The W-8-era pbmark rationale ("SO stores only dirty the L2, QNX's
  re-init discards them") → WRONG for the bc page (W-16: they survive
  without any flush). The CIPA machinery = dead weight for the bc page.
- The analyst's hex-arithmetic errors (three: the "84 KB rebuild
  anomaly", the "512-byte chain mismatch", the W-21 "-0x20") → rule 13
  (python-verified arithmetic) added to docs/README.

## BEQUEST to session 9

1. **The new front = 146, inside iotable_init (147 absent)** — bisect
   create_mapping: memblock allocs / pmd-writes / the pte path. The
   Image path (W-4) PASSED this code — the decompressor-handoff delta
   remains the suspect class.
2. **The delivery paradox = OPEN** — mechanism unknown; the inline
   fixup = the workaround; if the C world needs more MMU-off code,
   INLINE it (do not call).
3. **Consider pruning the now-proven-unnecessary CIPA machinery** (the
   pbmark flushes, the W-8 phys2virt flushes) — after confirming the
   SO-store model with one control run.
4. **The 171 wall's old analysis** (taskstats/kmem_cache) resumes once
   the boot passes the remap/paging region — read the era matrix first.
5. The bc[3]=0x41, bc[13]=0x66, bc[14]=0x66, mirror0=0x27F (pre-PB-
   MMU_BC) values appeared in the C-world runs — writers unidentified.
6. The user's LED timelines: W-10b (blue-off = reset in --t3),
   W-17/W-19 (blue-off = the probe's chain in --l2on) — recorded in the
   run sections; the device clock = GMT-3, the host = UTC.
