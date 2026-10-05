# Session 18 Notes (2026-10-05 — W-101 r3: a 126-class draw + the decode correction)

## THE SESSION'S SHAPE (in progress; the wrap will extend)

Boot from BOOTSTRAP_SESSION_18 (read order done; link verified: door keeper
alive since 13:36, SSH up; artifact #162 exact, 5,223,425 / 47c175cb; repo
clean at d0c61cd). ONE user-gated device run so far (W-101 r3) + the full
battery + two extra full-page reads (ring2/ring3). NO artifact change (pure
draw — "one variable = none"). The session's biggest offline product: the
DECODE CORRECTION (CR→LF dissolved) + the r2/r1 death windows restated from
source order + the ring3.count model.

## W-101 r3 (the run — full record: ~/agent-runs/w101-run3-record.md)

- 126-class draw; death ∈ early_fixmap_shutdown (the [126→125] window) =
  the session-11 front / the 126-family wedge PROPER. Non-flipped
  (placement 0xa2400000; bc[6]=e0000000).
- The W-101 pass ran 3/3 (bc[16]=272; arm-zeroing watched live).
- W-94 snapshot ran live (fresh bc[25]=0xa0000000 / bc[28]=0x1fe00000;
  sentinels intact).
- ring2 agrees (1160/1160 both); ring3.count = 23041 + ring1.count EXACTLY
  (base parked — dual-writer page).
- LED 5/20/40/107 — deltas 15/20/67 (the 9th off→red instance: the first
  67 after 68×8; flagged, no conclusion).

## THE DECODE CORRECTION (the session's biggest finding)

- Ring text starts at +0x101 (ring_put pre-increments the idx); the
  session-17 decoder (raw[0x78:]) was off by one — prepended a residue byte
  and dropped the true last char ⇒ the phantom "CR→LF gap" deaths. Fixed:
  raw[0x79:]. All records end \r\n-complete (r1/r2/r3 + W-99 r1).
- Restated death windows: r2 = the svm-store class INSIDE the
  dma_contiguous_remap's iotable_init call (after 167, at p[0]=0;
  bc[13]=svm_pa=0xbfbfffd4; bc[10]=c0de0002 — md loop not run); r1 = after
  the complete PB-ADJ#1 record, before PB-MEM (window [133→134] stands).
- Future decode sanity: first fresh char = "[" (banner), last = "\n", and
  cross-check the last line against the count.

## SOURCE-ORDER FACTS NAILED (for reuse)

- The bc[1] ladder is NOT globally monotonic and numbers collide across
  files (setup.c 130-136 vs mmu.c 9x/12x/15x). Discriminators: PB_MMU_BC
  writes bc[1] + bc[2]=v|0x100 + mirror=v|0x200; plain pb_bc_put writes
  ONLY bc[1]; setup.c's pb_bc writes bc[1] + bc[2]=v|0x100 but NO mirror
  (rule 17).
- mmu.c paging_init tail order: map_kernel → PB_MMU_BC(127) → shave →
  dma_contiguous_remap() → PB_MMU_BC(126) → early_fixmap_shutdown() →
  PB_MMU_BC(125) → devicemaps_init() → PB_MMU_BC(129) → ... → bootmem_init
  → PB_MMU_BC(128). So 126 = "remap done"; death at [126→125] = inside
  early_fixmap_shutdown.
- iotable_init's ladder (mmu.c:1042-1116): 150 → 159/160/161 → [W-94-era
  dumps] 167 → (p[0]=0) 167|0x400 → 151 → bc[19] readback → md loop
  [bc[10]=md->virtual → create_mapping → 152 → add_static_vm → 153].
  CALLED BY dma_contiguous_remap (dma-mapping.c:393) — hence the svm-store
  class is reachable per draw inside the remap.
- PB-CMA print = dma-mapping.c:294 (its "va=" = map.virtual — bc[10]'s
  last writer on draws that get there). The flipped draw's FDT-trim
  warning adds exactly one printk line: r2 ring = r3 ring + 71 chars
  ("[    0.000000] OF: fdt: Ignoring memory range 0xa0000000 - 0xa8000000\r\n"
  = 15+54+2).
- The W-94 snapshot slots: bc[24]=cnt, bc[25]=m[0].base, bc[28]=m[0].size,
  bc[29]/[30]=sentinels (intact ⇒ cnt==1), bc[31]=cnt#2 (mmu.c:1654-1670).
- bc[26]=0xbeef0000 (mmu.c:2029 sweep tally / payload value — same-value
  class), bc[27]=0xE1000003 (dma-mapping.c:390 post-ISB marker).

## ENVIRONMENT / TOOLING NOTES

- The Hermes terminal-output layer can MASK hex-looking runs in ASCII
  renderings (e.g. "a8000000+180****0000" where the ring truly holds
  "a8000000+18000000"). When a value looks masked: re-extract it via
  python and print the HEX bytes — the hex passes through clean.
- poll-jump.sh's pass-1 residue read is the freshness baseline for every
  slot 0..31 — compare CHANGED values against it.

## W-101 r4 (the run — full record: ~/agent-runs/w101-run4-record.md)

- THE TRIAD DRAW: bc[1]=142 + bc[2]=0x3E7 + ring 0/0; death INSIDE the inline
  fixup region (after 144, before pbmark 131; bc[16]=0 — the W-101 block left
  no completion marker; walk-1-vs-W-101 sub-window not resolvable without new
  markers).
- Fresh + source-matched: bc[3]=a3300000 (payload phys write), bc[13]=a000859c
  (head.S W-12 target), bc[20..23] = the decompressor W-54 dump (first
  pre-C-death capture), bc[24]/[25] = head.S W-56 (r2/r1 at stext).
- ring3 = 0x5A01/0 — the base alone (4/4 model; the base's writer still
  unpinned — settle with one armed-window read).
- LED partial (magenta→off 20 s; off→red 68 s — the 10th instance, band
  restored).

## OPEN / NEXT

1. W-102 (user directive; name not defined in the repo yet — clarify/propose):
   leading candidate = fixup-region fine markers (resolve the r4-class
   sub-window + make the W-101 block's progress visible).
2. More W-101 draws (r5+) as the spectrum builds (N=4: early / svm-store /
   126-family / fixup-region).
3. ring3 base-0x5A01 origin (one armed-window read settles it).
4. TASK-005 follow-ups unchanged (early_write breadcrumb; F1 console-lock).
5. Strategic fronts unchanged (the CPU1 release; the 185-path).
