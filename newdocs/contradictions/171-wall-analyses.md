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
