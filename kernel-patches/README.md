# Kernel patches — mainline 6.15.11 → PlayBook "winchester"

This directory contains the complete diff between **mainline Linux
6.15.11** and the kernel tree the project actually builds and boots
(`arch/arm` omap2plus, non-LPAE). Apply with:

```bash
git checkout v6.15.11            # or unpack pristine 6.15.11
git apply --check 0001-playbook-winchester-6.15.11.patch   # dry run
git apply 0001-playbook-winchester-6.15.11.patch
cp winchester.config .config     # the exact config the project boots
make ARCH=arm CROSS_COMPILE=<arm-linux-> -j$(nproc) zImage dtbs
# then pack: kexec/mkkernel.sh zImage   (appends the DTB)
```

`winchester.config` is the shipped config (note `CONFIG_IKCONFIG=y`: the
config is also baked into every built kernel and recoverable via
`scripts/extract-ikconfig`).

## What the patch contains, file by file

### Load-bearing (removing these breaks the boot)

| File | Purpose |
|------|---------|
| `arch/arm/kernel/head.S` (+247) | The PlayBook prelude: breadcrumb ladder (pbmark/pbmark3/pbmarkv), DEBUG_LL smoke test, **r1/r2 save/restore around the UART diagnostics**, early ring/PL310/UART section maps, the ring-map `lsl #12` fix (the M=1 unlock), TTBR0 walk-attribute strip (`bic r4, r4, #0x6A` — standing, necessity untested, see KNOWN_ISSUES) |
| `arch/arm/include/debug/omap4bc.S` (new, +116) | DEBUG_LL: UART3 @0xFED20000 **+ DRAM console ring capture** (VA 0xD4000080 when MMU-on / raw PA when off; MMU state captured once at busyuart entry) |
| `arch/arm/mach-omap2/omap4-common.c` | `omap4_l2c310_write_sec` — routes L2X0 CTRL/AUX/PREFETCH writes through the secure monitor SMCs (0x102/0x109/0x113); latency writes hit the default WARN+skip |
| `arch/arm/mm/dma-mapping.c` | `PB-CMA` diagnostic print in `dma_contiguous_remap` (base/size/va/`__pv_offset` — the discriminator for the pv-stub defect) |
| `arch/arm/mm/init.c` | PB-MEM/PB-ADJ prints + the memblock limit fix (`adjust_lowmem_bounds` computed 0 on this layout; forced to `memblock_end_of_DRAM`) |
| `arch/arm/mm/mmu.c` | `adjust_lowmem_bounds` instrumentation (vmalloc_limit/lowmem_limit prints) |
| `arch/arm/Kconfig.debug` (+12) | `DEBUG_PLAYBOOK_BC` choice → `DEBUG_LL_INCLUDE="debug/omap4bc.S"` |
| `arch/arm/boot/dts/ti/omap/Makefile` + `omap4-winchester.dts` (new) | The device tree: memory bank, pl310 node, reserved-memory bc/ring pages, model string |
| `arch/arm/kernel/early_printk.c` | early console plumbing compatible with omap4bc |

### Diagnostic instrumentation (as-shipped; safe to strip, but they are in every log)

| File | Purpose |
|------|---------|
| `init/main.c` (+183) | bc probes through `parse_early_param` (cmdline head/len), `setup_arch`, and the initcall levels — the session-5/6-era bisect scaffolding |
| `arch/arm/kernel/setup.c` (+49) | PB-ADJ/PB-MEM-era setup probes |
| `arch/arm/kernel/head-common.S` | bc probes in `__mmap_switched` (bss bounds) |
| `kernel/cgroup/cgroup.c`, `kernel/taskstats.c`, `mm/slab_common.c` | The 171-wall bisect markers (fine-grained breadcrumbs inside cgroup_init_subsys / taskstats_init_early / kmem_cache_create) |
| `arch/arm/mm/mmu.c`, `arch/arm/mm/init.c` | (also carry the instrumentation noted above) |

No functional deletions: the only removed lines are a `mov r3, r3` NOP in
head.S, the two `omap4-sdp*.dtb` list entries in the dts Makefile, and
guard lines in `parse_early_param`/initcall loops that were rewritten to
carry probes.

## Caveats

- The patch reflects the tree **as of session 7 (2026-09-03)** — the exact
  bits behind runs W-4/W-5 and kernel `#84`. It will evolve with the boot.
- `head.S`/`omap4bc.S` are heavily comment-marked `PlayBook` — grep that
  for the full change inventory.
- The bc/ring section VAs (0xC88/0xD00/0xD40) are load-bearing *and*
  deliberately identical to the linear-map VAs those pages later get, so
  `paging_init`'s re-map is seamless.
- Regenerating the diff: the project keeps a pristine 6.15.11 tree; the
  patch is `diff -ruN` of the two trees restricted to tracked source files
  (build artifacts excluded).
