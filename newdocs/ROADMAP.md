# Roadmap

Ordered. The first item is the next device run.

## 0. Where session 8 left the boot (2026-09-04)

The zImage path is IN THE C WORLD: the pv fixup is inlined into head.S
(the delivery paradox bypassed — never CALL MMU-off helpers), the DTS
bank = 0xa0000000+512MB (matches the zImage PHYS_OFFSET — AUTO_ZRELADDR
always lands 0xa0008000), and the C world's pv reads are cured by the
W-24 dual-level invalidate (L1 DCCIMVAC by VA + the monitor SMC 0x101
by PA) in map_lowmem. Death: bc[1]=146 — inside iotable_init, 147
absent. Console correct through the PB-CMA print.

## 1. Bisect iotable_init/create_mapping (next session, first run)

Markers inside create_mapping's path (the early_alloc/ memblock allocs,
the alloc_init_pmd/pte split, the section-vs-page branch). Suspect
class: the decompressor handoff (the W-4 Image path PASSED this exact
code without a decompressor). The W-24-style dual-level invalidate
applied to whatever lines/regions the mapping path touches = the likely
fix shape.

## 2. The ladder beyond the remap: 126 → 125 → 129 → 128 → bootmem_init
   → paging_init returns → the 171 region

The 171 wall (taskstats_init_early → kmem_cache_create) gets its first
zImage-path test — with a real DTB, the full 512 MB bank, and a live
console. Read newdocs/contradictions/171-wall-analyses.md (the era
matrix) before analyzing.

## 3. Deal with l2x0_of_init before the boot reaches init_IRQ

Patch `l2c_enable` to skip the by-way op (0x7FC) on winchester, or set
`CONFIG_CACHE_L2X0=n` (keep QNX's L2 setup). See KNOWN_ISSUES #3.

## 4. RE SMC 0x112 (the undocumented loader service)

Full disassembly of `bootblob_arm9000.bin` (local-only; from the eMMC
secure-boot chain), find 0x112's callers and arguments. If it is an L2
tag/data-latency write service, the QNX 1/1/1 latency config becomes
fixable pre-jump.

## 5. Rootfs (T4)

## 6. Open questions carried from session 8

- The MMU-off delivery paradox (the called fixup never executed; the
  inlined one is build-luck-dependent) — mechanism unknown; session-08
  notes hold the full evidence. If more MMU-off code is needed: INLINE
  it, never call.
- The head.S/phys2virt.S comments still narrate the wrong
  dirty-discard theory (W-16 falsified it for the bc page) — a cleanup
  pass when convenient.
- bc[2]=0x3E7's writer (probe+zImage-correlated); bc[3]=0x41,
  bc[13]/bc[14]=0x66, mirror0=0x27F pre-PB_MMU_BC (C-world runs).
