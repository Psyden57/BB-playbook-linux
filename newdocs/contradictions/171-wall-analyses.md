# The bc=171 wall: two incompatible analyses

## Conflicting claims

**Session 6** (docs/03 late, HANDOFF_2026-09-02_late): the boot (kernel #23-#84
era, full memory, DTB present) stalls deterministically at bc=171 inside
`taskstats_init_early` → `kmem_cache_create`, analyzed as slab_mutex
corruption / cache-merge-scan issues / placement-dependent lost writeback.

**Session 7** (runs W-4/W-5): the boot reaches bc=171 (W-4) / bc=126 (W-5)
while running on the **no-DTB 16 MB memblock fallback**
("Neither atags nor dtb found", "cma: Failed to reserve 16 MiB"). These
boots are heavily degraded — memblock has ONE 16 MB region anchored at the
kernel image, no CMA, no devices from DT.

## Why they conflict

The session-6 analysis assumed a normally-configured kernel (full memory,
DTB, CMA at 0xbe800000). The session-7 171/126 deaths happen in a kernel
state so different that the same death point may have a different cause
(e.g. something in slab/bootmem hitting the 16 MB ceiling, or the missing
DTB cascading into kmem_cache_create). It is also possible both share one
upstream cause (the unpatched-pv-stub signature, see
machine-corruption-vs-code-bugs.md).

## What would resolve it

Run the zImage (DTB delivered natively, full memory) and observe:
- If the boot sails past 171: the session-6 wall was an artifact of that
  era's bugs (the pv/C/B/S strip issues since fixed); close both analyses.
- If it dies at/near 171 again with full memory: the session-6 analysis
  applies; resume it with the improved observability (ring1 console now
  works; PB-CMA print in place).

## Current status

UNRESOLVED. No run has yet reached 171 with a working DTB.

## ADDENDUM 2026-09-03 (session-6 native context, corrects a factual claim above)

The claim "no run has yet reached 171 with a working DTB" is wrong for
session 6's own runs: **run 23 (kernel #51, zImage) died at bc=171 with the
DTB present** ("OF: fdt: Machine model: BlackBerry PlayBook" in the
recovered log), memory trimmed to 0xa0000000-0xC0000000, and CMA reserved at
0xbe800000 ("cma: Reserved 16 MiB" — memblock healthy). The nuance: run 23
ran the UNCACHED (C/B/S-stripped) kernel with the L2 OFF, and its console
died mid-setup_arch (see session-06.md correction #1) while its bc ladder
continued to 171. The full era matrix:

| Kernel state | L2 | DTB/memory | Death |
|---|---|---|---|
| UNCACHED (stripped), #51 era | off | DTB + 512 MB | **171** (run 23) |
| UNCACHED (stripped), #52+ era | off | DTB + 512 MB (state inferred) | 171 (runs 24-32) |
| CACHEABLE | on | DTB + 512 MB | 127 (session-6 late, pre-fix) |
| CACHEABLE | on | 16 MB no-DTB fallback | 171 (W-4) |
| CACHEABLE | off | 16 MB no-DTB fallback | 126 (W-5) |

The untested cell = **CACHEABLE + L2-on + DTB + full memory** — exactly the
session-7 zImage run. Note the recovered-log caveat (session-06.md
correction #1): run 23's "23 KB log" is ~15 real lines; the rest is filler.
