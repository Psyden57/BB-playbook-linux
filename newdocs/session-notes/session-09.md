# Session 9 Notes (2026-09-04 — the pv wall, the stale pgd pair, and the
# placement-overlap guard)

## THE RUN MAP (W-25 → W-38, all --l2on/--dmaquiet, kernel #102 → #110)

| Run | Build | Change | Result |
|---|---|---|---|
| W-25 | #102 | iotable/svm bisect markers 150-158 | **150 landed, 151 not — died inside the 44-byte svm memblock_alloc**; console = clean + the "BUG: 0x00000000 at 0x20000000" line |
| W-26 | #103 | svm alloc split (159/160/161) | **stack-protector PANIC: wild write smashed fdt_get_property_namelen's frame** (arm_memblock_init's reserved-mem walk); bc[1]=133 = setup.c's parse marker (collision found!) |
| W-27 | #103 | identical re-run (determinism test) | death MOVED: passed the FDT walk, died inside map_lowmem's pte-table alloc (the unaligned DTB reserve forces the pte path) — **per-run nondeterminism PROVED** |
| W-28 | #103 | payload --dmaquiet v1: MMC2 softreset | **SIGBUS fltno=5 on the first MMCHS read** — device regs NOT NS-accessible; no jump; box then FROZE (power-hold needed) |
| W-29 | #103 | --dmaquiet v2: `slay devb-mmcsd-winchester` after the file reads | jump OK; **new death: bc[1]=121 (enable_mmu entered, 122 never) — pre-C-world**; user's ls/cat broke mid-run = devb actually dead |
| W-30 | #103 | DISPC kill readbacks bc[7]/bc[14] | **NO JUMP: the payload's own memtest/copy writes killed QNX** (froze between bc 31 and 39); screen-tap = no wake (DISPC kill works) |
| W-31 | #103 | DISPC readbacks → bc[16]/bc[17]; bc[18]=placement | died at 161 (the svm memset), pv-STALE regime returned (PB-ADJ 30000000/0); placement 0xa1c00000 |
| W-32a | #103 | hands-off baseline (zero interaction) | **byte-identical reproduction of W-31** (161/0xbfdfffd4/ring 1137) with placement 0xa1600000 — interactions AND placement exonerated |
| W-32 | #104 | self-healing invalidate in adjust_lowmem_bounds | block NEVER RAN — died at early_paging_init (the first pv consumer, upstream of the block) |
| W-33 | #105 | block moved to start_kernel (pre-setup_arch) | block ran, **failed 8/8** (pv still stale at PB-CMA) — SMC 0x101's CLEAN step poisons DRAM with the stale L2 line |
| W-34 | #106 | **W-32c: direct store of the build constants + DCCIMVAC** | **PV FIX WORKS** (tries=0, pv correct end-to-end, BUG line gone) — and the svm memset STILL kills (161→151) with correct pv |
| W-35 | #107 | memset chunk markers 164/165/166 | placement 0xa0e00000 = **the FIRST window ever overlapping the zreladdr inflation region** → decompressor chaos → head.S-tail death (bc[1]=142, ring 0, 0x3E7 back) |
| W-36 | #107 | payload placement guard [0xa0000000,0xa1000000) | guard works (0xa1600000); head.S passed; **death = the FIRST 4 volatile stores of the memset** (164-166 never fired) |
| W-37 | #108 | pmd dump + single stores | **bc[19] = 0xbfc1141e = the pmd for VA 0xdfdfffd4 is a bogus TABLE pointer into never-allocated DRAM** — the svm store's PTW walks into garbage → the silent wedge; bc[1] even held 0xd0000004 (a torn artifact) |
| W-38 | #109 | 6-pmd pattern dump, stores skipped | [0xdfc]/[0xdfd] = **IDENTICAL bogus table pointers** (0xbfc1141e, the __pmd_populate signature, deterministic across runs+placements); [0xdf8] = a CORRECT section desc (0xbf81141e); [0xdfe+]=0; [0xdf0/0xdf4]=0 (CMA-cleared) |

## CONFIRMED (facts established this session)

1. **The pv-stale regime root cause chain**: the decompressor writes the
   image cached (L2 lines = pre-fixup content); the fixup's MMU-off SO
   stores reach DRAM but NOT L2; the C world's first pv read hits the
   stale L2 line iff it survived ~10 MB of streaming (eviction luck) —
   per-run coin flip. Stale __pv_offset ⇒ every C-world __va/__pa inline
   computes garbage (they read the VARIABLE, not the patched asm stubs)
   ⇒ deterministic wedges at the first pv-consuming allocation. The
   "random wandering deaths" (svm alloc / FDT walk / pte alloc /
   MMU-enable) were all the same wall seen from different distances.
2. **The W-24 fix's flaw**: its own `__pa(va0)` consumed the stale pv it
   was repairing — in the stale regime the SMC flushes targeted
   VA-as-PA (a no-op). It only "worked" in runs that were already fresh.
3. **SMC 0x101 (clean+inv by PA) POISONS DRAM when the L2 line is
   stale**: the clean step writes the stale line back, destroying the
   fixup's correct DRAM copy (W-33: 8/8 verify failures). There is no
   invalidate-only-by-PA monitor service.
4. **THE PV FIX (W-32c, build #106)**: directly STORE the build-time
   constants into __pv_offset (0xffffffffe0000000) /
   __pv_phys_pfn_offset (0xa0000) in start_kernel BEFORE setup_arch,
   then DCCIMVAC — the clean carries the CORRECT value through the L2C,
   overwriting the stale L2 line. Deterministic (tries=0 in every run
   since). bc[6] (the fixup's delta) confirms the same value every run.
5. **THE STALE PGD PAIR**: the pair [VA 0xdfc/0xdfd] = PA
   [0xbfc00000/0xbfd00000] reads as IDENTICAL bogus TABLE descriptors
   (0xbfc1141e → a table at 0xbfc11400 = a never-allocated QNX-era pte
   table for that DRAM region) — deterministic across runs AND kernel
   placements. map_lowmem's section write for the LAST pair of the
   linear map doesn't survive in the PTW's view (the write-back-loss
   class). [0xdf8] = a CORRECT section desc (0xbf81141e) — only the
   last pair is poisoned. NOT a random wild write.
6. **Marker-number collisions**: setup.c's pb_bc(130-136) pairs share
   numbers with mmu.c's PB_MMU_BC ladder (130-136 vs 133/134...).
   Discriminate via the MIRROR channel (PB_MMU_BC writes
   0xD4000004 = v|0x200; setup.c's pb_bc does not). W-20's "133 =
   map_lowmem done" reading is now doubtful (W-26's correction).
7. **The placement-overlap guard**: W-35's window (0xa0e00000) overlapped
   the zreladdr inflation region [0xa0008000, ~0xa0f80000) — the ONLY
   run that ever did — and it died in head.S's tail (bc[1]=142, ring 0,
   0x3E7 back). buf_placement_bad now reserves [0xa0000000,
   0xa1000000). Every clean run placed ≥ 0xa1200000.
8. **Device-register access rules extended**: MMC2/MMCHS registers
   (0x480B4000+) SIGBUS from NS (fltno=5, W-28) AND that abort class
   then FROZE the box completely (SSH dead, display dead — power-hold
   needed). NOT cheap like the run-31 PRCM SIGSEGV. DISPC registers ARE
   NS-accessible (the kill + readbacks work, 0/0 confirmed).
9. **DMA-master quiesce state**: devb slain (--dmaquiet; QNX survives —
   qnx6's write-back cache absorbs the jump.log writes; only the
   cache-flush thread needs devb, hence the W-29 "cat: cannot execute"
   = an exec needing a devb READ); DISPC killed + register-confirmed
   (bc[16]/bc[17] = 0/0); WiFi SDIO never brought up (DTS). The
   randomness SURVIVED the quiesce → the source is the stale-view class
   above, not a rogue DMA master.
10. **Swipe/tap interactions EXONERATED** (W-32a hands-off = byte-identical
    death to W-31). The touch controller and power button are not
    variables.
11. **Payload ops**: `system("slay -f devb-mmcsd-winchester")` after the
    last file read (bc 55, rc → bc[14] = 0xD1EBxxxx); rc 0x100 = exit 1
    but devb demonstrably died. do_t3's setvbuf(_IONBF) overrides
    main()'s 64KB buffer — harmless given the qnx6 cache model.
12. **The svm (struct static_vm, 44 B) allocates deterministically at PA
    0xbfdfffd4** (top-down from arm_lowmem_limit 0xbfe00000, directly
    below the CMA remap's pte table at 0xbfdff000) — every run, every
    placement. Its VA 0xdfdfffd4 sits in the poisoned top-of-map pair.

## MECHANISMS / MODELS (working)

- **The write-back-loss class**: head.S/decompressor-era content
  survives in L2 for lines the C world writes cached; the PTW (and
  sometimes the C world's own later reads, after L1 eviction) serves the
  stale content. The pv fix and the pmd pair are the same class. The
  self-healing store+DCCIMVAC pattern cures a KNOWN victim; the general
  cure = either find every victim or make the PTW's view coherent
  (the L2-state question: QNX's 1/1/1-latency PL310 config).
- **The 0x3E7 wild write** (bc[2]): back in the head.S-era deaths
  (W-29's 121, W-35's 142); probe+zImage-correlated as always; writer
  still unidentified. The bc[0x80-0x8F] dump found only sanitized
  headers + residue (no signal).
- **W-29's head.S-era 121 death** predates the placement guard; likely
  the stale-era dice. The pv fix now covers that class.

## BEQUEST to session 10

1. **W-39 (build #110) is BUILT AND UNRUN**: the 2MB allocator shave
   (arm_lowmem_limit -= 2MB after map_kernel → the svm lands at
   ~0xbfbfffd4, pair [0xdfa/0xdfb] = the previously-untested middle
   pair) + stores re-armed (167 = dumps done, 0x567 = store 1,
   151 = memset done, bc[19] = readback) + bc[25] = the VA 0xdfa pmd.
   If the middle pair is healthy → the memset completes → the boot
   advances (toward the 171 wall / KNOWN_ISSUES #3 l2x0 hazards).
   If the middle pair is ALSO stale → extend the shave or self-heal the
   pgd (the W-32c store+clean pattern applied to the pmd: write the
   expected section desc + DCCIMVAC before any access through it).
2. The W-32c direct-store pv fix = KEEP (deterministic, tries=0).
3. The placement guard = KEEP.
4. The 171 wall's old analysis (taskstats/kmem_cache) resumes once the
   boot passes the remap/paging region — read the era matrix first.
5. Extended bc slots: bc[16]/bc[17] = DISPC readbacks, bc[18] =
   placement, bc[19] = pmd/readback, bc[20..25] = the pmd pattern —
   read via `memdump3 90000040 0x30`.
6. The user's interaction protocol: hands-off baseline for A/B runs;
   ask for LED timings every recorded run; the user prefers exact
   typed timelines over approximations.
