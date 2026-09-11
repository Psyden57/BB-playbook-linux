# Session 10 (2026-09-11) — session notes

Backfilled at session 11's start at the session-11 agent's request (Q1);
the runs themselves were recorded as git commit messages. This file holds
the knowledge uniquely available to session 10. docs/03's run map =
the complement (the W-40..W-68 backfill).

## The session's five durable discoveries

1. **NS PL310 0x768 = a no-op.** The project-wide "CLEAN_INV_LINE_PA"
   flush address is not a mainline l2x0 register at all (0x740 = DUMMY;
   the real by-PA ops = 0x770 inv / 0x7B0 clean / 0x7F0 clean+inv).
   Every "NS flush works" claim = re-examine: the verified effects came
   via SMC 0x101 (the monitor does it right, secure-side).
2. **NS 0x770 invalidate-by-PA, NO clean: PROVEN working** (the l2canary
   ladder: the dirty L2 line discarded, the DRAM canary served — the
   DRAM keeps the truth, no poisoning). 0x7F0 CIPA also works from NS.
   This is the real stale-line cure the roadmap called for.
3. **The unified barrier diagnosis**: every full-system/inner-shareable
   barrier on the boot path (dsb sy, dsb ish, the IS TLB variants, and
   the DSB before/inside the flushed code) waits for the HELD CPU1 =
   THE explanation for months of per-run flakiness. `dsb nosh` = the
   CPU-local barrier = the fix. Also wedging: set_current's
   mcr c13,c0,3 (the TPIDRURO write — bypassed with the plain str to
   __current, KEPT) and the DCCMVAC of a DIRTY CACHEABLE line with the
   L2 bypassed (hangs — the same op on the uncached bcs = harmless).
4. **The .init.data/.data cached stores are LOST with the L2 off** (the
   L1-dirty content dies at the WDT2 reset; survival = per-slot
   eviction luck). This is why r2 reached the kernel correctly
   (bc[24] = the DTB) yet __atags_pointer read 0 at setup_arch, and why
   the marker losses were per-slot. The cure = DON'T do cache ops (the
   DCCMVAC = the hang) — recover the value from a DRAM-truth source
   instead: setup.c reads bc[20] (the decompressor's W-54 MMU-off
   SO-store dump = the appended DTB's phys) when __atags_pointer == 0.
   VERIFIED: the ring then shows the full healthy boot log through the
   CMA (the W-66 breakthrough run).
5. **The fresh-boot QNX pool has NO 24 MB contiguous run** (twice;
   all 129 hinted slots non-contiguous; the generic fallback's grabs
   land in the guarded inflation region). The payload now ladders
   12 → 8 → 6 MB with 2 s settles.

## The harness truths

- The L2-off now runs inside the proven --dmaquiet shape (the W-46
  fix). The old --t3 shape = the old direct-jump = no FDT pointer =
  the "invalid dtb" error path.
- pb_bc_put now ALWAYS DCCMVACs the line (the W-50: with the L2 off the
  plain store = L1-dirty = lost at the reset; the marker losses were
  per-slot L1-eviction luck).
- The kernel = SINGLE-CORE for the whole boot: ACTLR.SMP cleared in
  start_kernel AND re-cleared after setup_processor (the W-48: the
  setup re-arms it!). maxcpus=1.
- The sweep (the pgd per-line CIPA) self-verifies: bc[26] =
  0xBEEF0000|mism<<8|tmo — 0xBEEF0000 = completed with zero
  mismatches/timeouts on the runs where it fired.
- The W-40b lesson: iotable_init inside the sweep = its own poison (its
  svm alloc lands under the PRE-shave limit = the poison region). The
  PL310 desc = written directly into pgd[0xFEB] instead; the
  pgd[0xFEB] line must be pre-cleaned before the first PL310 access
  (the W-40e lesson: the sweep's own first TLB refill walks it).
- The forensics map (bc[16..27]) + the read command = in
  SESSION-HANDOFF/BOOTSTRAP_SESSION_11.md.

## The session-11 agent's four questions, answered

1. The run records = only in git; the backfill = done (docs/03 now has
   the W-40..W-68 map; this file = the notes).
2. In #139: the bc[27] phase ladder (1/2/3) still exists in
   tlb-v7.S's v7wbi_flush_kern_tlb_range, but that function is NO
   LONGER CALLED by the CMA path (the flush = the local TLBIALL+nosh
   inline in dma-mapping.c). The CMA path's "flush passed" signal =
   the pb_bc_put(146) marker. The F4/F5/F6 spans = still live in
   setup.c/devtree.c/head-common.S. Nothing removed — only bypassed.
3. The pbmark/pbmarkv CIPA ops = intentionally left in #139 (harmless
   on the uncached bcs, and the WDT2 kicks inside them are
   load-bearing). For #140+: FIRST convert their `dsb sy` barriers to
   `dsb nosh` (the barrier sweep = the priority!), only then consider
   stripping the flush ops.
4. If #139 passes 146/147: do NOT expect bc[1]=161/167 to be the wall —
   the L2-off + the sweep + the nosh barriers changed the game: the
   poison lines can't be served, the flush = deterministic. The
   expected next wall = a NEW site (possibly the remaining dsb-sy
   sites in the marker macros, or the first genuinely new code path).
   The run decides; the bc[1] + bc[27] ladder will name it.
