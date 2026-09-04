# BOOTSTRAP_SESSION_9 — the session-9 handoff prompt

You are taking over the PlayBook kexec debugging effort (session 9,
2026-09-05+). Session 8 (2026-09-04) BROKE THE FIXUP WALL: the zImage
path now reaches the C world with a live console, a parsed DTB, a
correct CMA, and correct pv translation. The new front = **bc[1]=146,
inside iotable_init**.

## MANDATORY READ ORDER (before any work)

1. `newdocs/HANDOFF.md` (the protocol) → `newdocs/PROJECT_STATE.md`
2. `newdocs/ARCHITECTURE.md`, `newdocs/COMMANDS.md` (the run rules!)
3. `newdocs/session-notes/session-08.md` (THE session-8 record — the
   run map W-9→W-24, the confirmed facts, the corrections, the bequest)
4. `docs/03_DEBUGGING_SESSIONS.md` runs W-9 → W-24 (the details)
5. `docs/README.md` — the critical rules 1-16 (13-16 are NEW: python-
   verified arithmetic, grep-don't-recall, native tools, verify the
   shipped binary) + the cheatsheet + the decision tree
6. `newdocs/contradictions/` + `newdocs/KNOWN_ISSUES.md`
7. `PLAYBOOK-REFERENCE.md` §5/§8/§10 (safety) — NVRAM/RPMB stay
   off-limits forever

## WHERE THE BOOT STANDS (kernel #101, all --l2on)

- head.S: the pv fixup is INLINED (no call) at the streamed region with
  markers 143/144 + the delta in bc[6]; the .data CIPA flush (W-23) and
  the surviving-slot markers (W-22) are in place; the I-clear is
  REMOVED (W-19).
- mmu.c: PB_MMU_BC ladder intact; the W-24 dual-level pv invalidate
  (DCCIMVAC + SMC 0x101 via pb_smc_flush) sits in map_lowmem before the
  PB-ADJ print.
- dma-mapping.c: the remap markers 145/146/147 (pmd_clear / tlb-flush /
  iotable_init).
- DTS: bank = 0xa0000000 + 0x20000000 (MUST stay matched to the zImage
  PHYS_OFFSET = 0xa0000000 — AUTO_ZRELADDR's bucket).

## THE CURRENT DEATH

bc[1] = 146 (the "tlb flush done" marker inside dma_contiguous_remap);
147 ("iotable_init done") never lands. The console is CORRECT through
the PB-CMA print (pv_off=ffffffffe0000000, no BUG). **The wedge is
inside iotable_init → create_mapping** (early mapping of the CMA
region: memblock allocs → pmd/pte writes).

## THE FIRST TASK

Bisect inside iotable_init/create_mapping. Options: markers on
create_mapping's internal steps (the early_alloc path, the
alloc_init_pmd/pte path, the section-vs-page mapping split), or read
the W-4 Image-path record — W-4 PASSED this exact code with the SAME
kernel functions (no decompressor involved) — the decompressor-handoff
delta (its dirty lines, its clean/ICIALLU, the cache state it hands to
the C world) = the leading suspect class. The W-24-style dual-level
invalidate (L1 by VA + the SMC 0x101 by PA) applied to the specific
lines/regions map_kernel/iotable_init touch = the likely fix shape.

## THE OPEN QUESTIONS (session 8's unresolved)

1. **The delivery paradox mechanism** — the called fixup never executed
   via any mechanism; the inlined one is build-luck-dependent
   (#95/#96 worked untouched; #97 needed the W-24 flush). If more MMU-
   off code is needed: INLINE it, never call.
2. **Why W-17/18/19's pv reads were correct with L2 on** while
   #97+ needed the W-24 flush (eviction luck? the layout?).
3. **bc[3]=0x41, bc[13]=0x66, bc[14]=0x66, mirror0=0x27F pre-PB_MMU_BC**
   — unidentified writers in the C-world runs.
4. **bc[2]=0x3E7's writer** (probe+zImage-correlated).
5. **The 171 wall** (taskstats/kmem_cache) — the era matrix applies;
   the boot must first cross the remap/paging region.

## THE RULES (the short form — the full list = docs/README.md)

- Ask the user before every device run; request LED timings every run.
- Verify arithmetic with python (rule 13); grep source claims (rule 14);
  native tools for edits (rule 15); verify the shipped binary (rule 16).
- Never: inline-stub SMCs, NS PRCM writes, NS PL310 CTRL/AUX/latency,
  by-way 0x7FC, unbounded polls, UART3 writes from the kernel.
- NVRAM and RPMB are off-limits forever. The power-hold reset = warm
  (DRAM survives).
- After every run: append docs/03, update PROJECT_STATE, commit+push.
