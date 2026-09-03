# Known Issues & Open Problems

Current as of session 7 (2026-09-03). Ordered by blocking priority.

## 1. bc=171 wall — taskstats_init_early / kmem_cache_create

The boot's current death point. Reached twice (W-4 with probe: bc=171;
W-5 without: bc=126 — the no-DTB 16 MB fallback shifts the death point).
Session 6 analyzed this as "slab_mutex corruption"; that analysis predates
the placement/DTB discoveries and may not apply. **Untested with a working
DTB + full memory** — the zImage run comes first.

## 2. DTB delivery broken on the uncompressed-Image path (parked)

`Warning: Neither atags nor dtb found` ×2 despite a verified-correct
`params[1]` (W-4: address math closed exactly; probe validated magic;
REVERIFY passed). Somewhere between the cont's `r2 = params[1]` and the
kernel's `__vet_atags` the value is lost. The 1 GB probe loop is
exonerated (W-5 lost it too). Parked in favor of the zImage path
(DECISIONS D6); investigate only if zImage ever fails.

## 3. l2x0_of_init hazards (will fire when the boot reaches init_IRQ)

- `l2c_enable` (arch/arm/mm/cache-l2x0.c) writes `L2X0_INV_WAY` (0x7FC) —
  the by-way op that DEADLOCKS this machine from NS.
- `l2c_wait_mask` = a poll loop on the sync register.
- The DTB latency writes are safe (routed through `omap4_l2c310_write_sec`
  → default WARN+skip; the old "guard the driver against NS latency
  SIGBUS" advice is a no-op).
Fix: patch `l2c_enable` to skip the by-way op on winchester, or
`CONFIG_CACHE_L2X0=n` (keep QNX's L2 setup). Current config has L2X0 on.

## 4. The unpatched-pv-stub signature (observed once, mechanism unknown)

Session 6's "barrier stack struct corruption" (0xfe600000 → 0x1f7f0000) and
one W-4-era reading both match the **unpatched pv-stub placeholder delta**
(0x81810000 = `__PV_BITS_31_24` + `__PV_BITS_23_16`): an unpatched pv site
computes `__phys_to_virt(x) = x - 0x81810000`. The pv fixup code is
byte-identical to mainline; how a site could stay unpatched is unknown.
Not reproduced since the placement fixes. The `PB-CMA` print (kernel) will
catch it (variable correct + VA wrong → site unpatched).
Correction (session-6 context, see session-06.md): the 0x1f7f0000 BUG was
`dma_contiguous_remap`, not the barrier (the barrier-disabled run reproduced
it identically) — and the remap's base ALSO read wrong (0xa1000000 vs the
printed CMA at 0xbe800000), so either two independent anomalies or one wild
write hit both fields.

## 5. Console output stops mid-boot (secondary)

In W-4/W-5 the ring1 capture ends right after the `PB-ADJ` print while the
bc ladder continued (126/171). Something silences earlycon/printascii after
the memblock prints. Not blocking yet (breadcrumbs still work); investigate
when the boot goes deeper.

## 6. SMC 0x112 — undocumented monitor service

The QNX initial loader (eMMC secure-boot chain) contains NS-side SMC
wrappers for 0x112 (two shapes). Semantics unknown; two candidate readings:
an L2-adjacent service (the observed QNX latency value 0x111 is suspicious)
or something else entirely. RE of `bootblob_arm9000.bin` pending.

## 7. W-4/W-2 "corrupted" bc tails after reboot

After a reboot the bc page's tail (bc[6]+) holds garbage while magic +
bc[1] survive — QNX's own boot tramples DRAM. Mirror pages can be zeroed.
Always verify magic + nonce before trusting a readback.

## 8. Miscellaneous

- The payload requires root + `ThreadCtl(_NTO_TCTL_IO_PRIV)`; all device
  work assumes the rooted 2.0.0.4869 WiFi-only unit.
- The eMMC rootfs path (`root=/dev/mmcblk0p2`) is in the cmdline but the
  kernel has no rootfs yet (T4, not started).
- `qnx-env.sh` hardcodes `/home/psyden` paths (machine-specific, benign).
