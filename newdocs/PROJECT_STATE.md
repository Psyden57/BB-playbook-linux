# Project State (as of session 7, 2026-09-03)

This file tracks the *current technical state* precisely. Older docs
(`docs/03`, `SESSION-HANDOFF/`) record how we got here; where they disagree
with this file, this file wins (and any unresolved disagreement is listed in
[contradictions/](contradictions/)).

## Where the boot stands

Mainline Linux 6.15.11 (omap2plus, non-LPAE, patched for the PlayBook) is
jumped from QNX. The boot currently:

1. Enters `stext` with the **MMU OFF** (it turns on at `__enable_mmu`,
   after the pv fixups and `__create_page_tables`). The zImage-path
   ladder: 101-103 land, **130 = post-__fixup_smp lands, 131 never
   does** — the current front. (The numbering is non-monotonic:
   130/131 execute BEFORE 119.)
2. (Image path — historical, parked) passed the paging_init region —
   **the bc=127 wall (`dma_contiguous_remap` "in user region" BUG) was
   root-caused and fixed on 2026-09-03** (see below). Boot reached
   **bc=171** in run W-4 — still the deepest boot, and the only one that
   entered the C world.
3. **bc=171 = the old wall, but the CURRENT front moved earlier**: the
   zImage path (the pivot that delivers DTB + full memory) dies BEFORE
   `__fixup_pv_table` can run. W-9 (2026-09-04, kernel #88, flushed
   fixup instrumentation) proved the fixup's entry stores never land:
   **death between the post-__fixup_smp marker's flush and the fixup's
   own CIPA** (~25 proven-op-class instructions + the bl). W-6/W-8's
   "dies inside the fixup" reading is superseded. See docs/03 run W-9.
   **The era-matrix sharpening**: runs 23-32 (zImage, UNCACHED, L2 OFF)
   passed the fixup 8+ times; W-6/7/8/9 (zImage, CACHEABLE, L2 ON) died
   there 4/4. **W-10 (2026-09-04, `--t3`, kernel #88, L2 OFF via 0x102):
   the fixup STILL never enters — the L2 variable is EXONERATED**; the
   remaining discriminator is the KERNEL BUILD (session-6 UNCACHED/
   C/B/S-stripped builds passed; the current cacheable build dies 5/5).
   Also closed this session: the dtb-phys
   arithmetic (params block exonerated — bc[3] is the BUFFER base,
   qnx2linux.c:935; W-6, W-8 and W-10 all close exactly), the bc[10]/bc[11]
   writers (parse_early_param + bss-bounds dumps — W-9's values are
   W-4-era residue, which SURVIVES the power-hold "hard reset" — it is a
   warm reset, DRAM persists), the ring2 wild-index theory for bc[2]=0x3E7
   (index sane), and bc[2]'s correlation: **0x3E7 needs the PROBE**
   (probe+zImage runs show it with either L2 state; --t3 shows 0).
   Next runs: (a) inline-test — the fixup's first stores copied into
   head.S right after pbmark 130; (b) resurrect the C/B/S strip on
   kernel #88.

## What was fixed in session 7 (the bc=127 wall)

Root cause (proven, all grepped not recalled):

- The kernel was loaded at an arbitrary QNX-given physical address; head.S
  derives the runtime PHYS_OFFSET from the load address
  (`ARM_PATCH_PHYS_VIRT`). The DTS declared a 1 GB bank at 0x80000000, so
  576 MB of memblock sat *below* the runtime PHYS_OFFSET — memory that can
  never be linear-mapped.
- Bottom-up memblock allocations (the `omap_secure_ram_reserve_memblock`
  steal, CMA) landed in that dead zone (e.g. 0xa1000000), and
  `dma_contiguous_remap`'s `__phys_to_virt()` produced a VA below TASK_SIZE
  (0xBF000000) → deterministic `BUG: not creating mapping ... in user
  region` at bc=127.
- Fixes now in place:
  - payload v4: 129-slot 2 MB-aligned placement sweep with per-reason
    diagnostics, then `kern_off` placement (kernel placed at the first
    2 MB-aligned offset *inside* the buffer — QNX's free pool has no
    aligned 24 MB run),
  - runtime DTB memory-node patch (`fdt_patch_memory`) so the bank always
    matches the actual placement,
  - `PB-CMA` print in `dma_contiguous_remap` (kernel) as a permanent
    discriminator,
  - DTS memory node baked to 0xa4000000+0x1c000000 as the default.
- Session 6's "silent DRAM corruption under QNX's L2 config" theory was
  **not needed** to explain the 127 wall. It may still be real (see
  [contradictions/](contradictions/)).

## The DTB delivery issue (open, blocks clean testing)

On the uncompressed-Image path, the kernel reports
`Warning: Neither atags nor dtb found` twice and falls back to a 16 MB
memblock region — yet both the payload (REVERIFY) and the probe validate
the DTB at `params[1]` before the jump. W-5 (--t3, no probe, no 1 GB loop)
still lost the DTB, so the probe loop is exonerated; the loss is somewhere
in the r2 chain (cont → kernel entry → head.S r7/r8 save/restore →
`__vet_atags`).

Session-2 backfill narrows this: the register chain was correct at probe
time at least once (bc[4]=0xa1e377f8 matched the placement math exactly),
and the vet constant is exonerated — `head-common.S` defines
`OF_DT_MAGIC 0xedfe0dd0` for LE builds (the LE read of the big-endian
magic; grep-verified 2026-09-03), so a valid DTB passes `__vet_atags`
(see session-notes/session-02.md CONFIRMED #3/#5).

**Decision: park the uncompressed-Image path.** Session 6's proven path is
the **zImage**: `CONFIG_ARM_APPENDED_DTB=y` + `CONFIG_AUTO_ZRELADDR=y`
mean the decompressor natively finds the appended DTB and sets r2, and
derives zreladdr from its own load address (512 MB granularity →
PHYS_OFFSET 512 MB-aligned → pv-fixup-safe). Session 6's recovered log
proves it worked (`OF: fdt: Machine model: BlackBerry PlayBook`,
`cma: Reserved 16 MiB at 0xbe800000`).

**W-6 (2026-09-04, first zImage test): the pivot FAILED to reach the wall —
the boot died EARLIER, inside `__fixup_pv_table` (bc=107 post-fixup_smp;
ring = smoke test only).** [SUPERSEDED 2026-09-04 by W-9: the "inside the
fixup" reading was an artifact of unflushed markers — the flushed
instrumentation proved the fixup's entry stores never landed; see
"Where the boot stands" #3 and docs/03 run W-9. The bc[4]/bc[12] chain
facts below stand; the dtb-phys math has since closed exactly (W-8/W-10).] Details: `docs/03` run W-6. Key facts: the
chain held (bc[4]/bc[12] = dtb_phys), the DTB magic validated, zImage word
0 confirmed — and **bc[2]=0x3E7 (999), the unexplained session-5/6 wild
write, returned on the zImage path exactly as in runs 23-32**. The zImage
environment differs from the Image path in one structural way: the
decompressed kernel lands OUTSIDE the payload buffer (0xa0080000,
decompressor-written, never verified) instead of the payload's
memcmp-verified copy. The Image-path binary remains the deepest boot
(bc=171); the zImage path now needs its own diagnosis before it can deliver
the DTB+full-memory test the era matrix wants.

**Next run** (W-11): inline-test — copy the fixup's first stores into
head.S directly after pbmark 130 (pins the death inside the pre-entry
window; isolates the bl/call from the stores), OR resurrect the C/B/S
strip on kernel #88 (tests the session-6 UNCACHED-build variable).

## The secure monitor — RE closed (session 7)

- No QNX binary contains the SMC dispatch (`trustzone-omap4` = the
  /dev/trustzone crypto resmgr; `libsecure_dispatcher` = crypto/KDS;
  `procnto` = 0 SMCs). The monitor is the TI ROM monitor.
- The documented service table (0x100 L2X0 DBG_CTRL, 0x102 L2X0 CTRL,
  0x101 L2 clean+inv by PA, 0x108 SCU_PWR/suspend, 0x109 L2X0 AUXCTRL,
  0x113 L2X0 PREFETCH) is **complete for L2: there is NO tag/data-latency
  service**.
- PPA probe (run PPA-1): 0x25 and 0x23 rejected (0xFF02), 0x26/0x27
  (devpm's suspend pair) accepted with no PL310 readback change.
  Secure-side L2 reconfiguration is **exhausted** as a fix path.
- New lead: the eMMC secure-boot chain (user-area sectors 5-240) contains
  the QNX initial loader with NS-side SMC wrappers for **0x112** (×2) — an
  undocumented service. Not yet semantically identified. See
  `newdocs/session-notes/session-07.md`.

## Kernel-side known hazards (ahead of the boot)

When the boot first reaches `l2x0_of_init` (init_IRQ):
- `l2c_enable` does a by-way write to `L2X0_INV_WAY` (0x7FC) — the
  on-device DEADLOCK op (NS by-way ops wedge the machine).
- `l2c_wait_mask` introduces a poll loop.
- The DTB latency writes are safe (routed through
  `omap4_l2c310_write_sec` → default WARN+skip).
Patch `l2c_enable` or run `CONFIG_CACHE_L2X0=n` (current config has it on)
when the boot gets there.

## Operational state

- Payload v4 in `kexec/` (see [../kexec/README.md](../kexec/README.md)):
  placement sweep, kern_off, DTB patching, WDT2 enable/disable lifecycle,
  `--t3/--l2on/--probe/--ppa/--l2lat/--l2test` modes.
- Kernel tree: `/home/psyden/kernel/linux` (6.15.11, config = the
  session-6 config, recoverable from any built Image via
  `scripts/extract-ikconfig`).
- Device: SSH root@169.254.0.1, key at `playbook-dev/rsa` (local-only,
  never commit). WDT2 warm-reset cycle ~59 s. LED sequence observable
  (user video-records runs).
