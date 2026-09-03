# SESSION RECORD — 2026-09-01: M=1 wall broken; ring-map bug fixed; new front = early-C wedge + PL310 secure filtering

## HEADLINE
**The 122-ceiling is DEAD.** Root cause found and fixed: the PlayBook ring-map
block in `arch/arm/kernel/head.S` (`__create_page_tables`) shifted PA>>12-style
values with `lsl #SECTION_SHIFT` (=20, non-LPAE) instead of `lsl #12` — the
shift truncated every descriptor to zero, mapping the DEBUG_LL ring VAs
(0xC80/0xD00/0xD40), the PL310 section (0xFEB) AND the abort-trap vectors
(0x000/0xFFF) to PA 0x00000000 (boot ROM) / PA 0x20000000. Post-M marker
stores landed in ROM space and were silently lost; the PL310 CIPA write hit
GPMC; the sync-poll spun forever; and the abort trap pointed at ROM code, so
"no abort fired" was never true evidence. The kernel now sails past M=1 into
start_kernel C: **bc reaches 145/168/150 (was: hard ceiling at 122).**

## THE BUG (in detail, for future reference)
- Working idiom (classic head.S): section NUMBER (PA>>20) shifted by
  SECTION_SHIFT. `0x880 << 20 = 0x88000000` ✓
- Ring-map block values: `0x88000`, `0x90000`, `0x94000`, `0x48200`
  (= PA>>12!), `0x9FE00` — these only make sense shifted by 12.
- `0x88000 << 20` = bits 19+15 → bits 39+35 → truncated to **0** on 32-bit.
- Empirically confirmed in the OLD vmlinux: `orr r3, r7, r3, lsl #20` at
  c00086f4 with r3=0x88000 → tt[0xC80] = 0xC12 → VA 0xC8000000 → **PA 0**.
- Consequences (all previously misread as "M=1 hang, no abort, no panic"):
  marker 124/123/110 stores → PA 0x4 (lost); CIPA write → PA 0x20042768
  (GPMC); sync-poll read → GPMC (stuck) → infinite silent spin; trap vectors
  → ROM → wild execution instead of the 0xAB handler.
- Explains BOTH eras: the Image-era "reaches 108, never 110" AND the
  zImage-era "ceiling 122".

## THE FIX (committed in /home/psyden/kernel/linux/arch/arm/kernel/head.S)
Values kept in PA>>12 form, shift corrected to `lsl #12`:
  tt[0xC80]=0x88000C12, tt[0xD00]=0x90000C12, tt[0xD40]=0x94000C12,
  tt[0xFEB]=0x48200C12, tt[0x000]=tt[0xFFF]=0x9FE00C02 (executable).
Verified by disassembling the rebuilt vmlinux. Kernel = mainline 6.15.11,
non-LPAE, SECTION_SHIFT=20, PMD_ENTRY_ORDER=2 (PMD_ENTRY_ORDER=3 is LPAE-only).

## NEW DIAGNOSTIC STATE (this session's builds)
- **Heartbeat markers 42-50 in do_t3 setup phase** (qnx2linux.c): proven the
  payload setup never crashes.
- **Flush-free marker channel PROVEN**: plain SO stores to the bc page
  (0x90000004/8) reach DRAM with NO CIPA/SMC — bc[2]=0x1 landed flush-free.
  All fine markers are now flush-free (pb_bc v6): no SMC/CIPA/poll in the
  death path.
- **Panic notifier** registered at start_kernel head → dumps "PB-PANIC: <msg>"
  into the rings via early_print/printascii (works pre-console). Rings clean
  in every run → NO panic/BUG fires; the wedge is silent.
- **Fine ladder in start_kernel**: 118 → 150 (post set_task_stack_end_magic)
  → 151 → 142 → 144 → 145 → 143 (post cgroup_init_early) → 140 → 141 → 136
  (setup_arch) → 130-135 → 110 (post paging_init) → 111.
- **cgroup bisect markers** (160/161/0x100+i/0x120+i/164-170 in
  kernel/cgroup/cgroup.c): the wedge lands between statements of
  cgroup_init_early / right after init_and_link_css.

## THE NEW FRONT: random silent stops in early start_kernel C
- Same binary wanders: 145 / 168 / 150 across runs. NOT deterministic.
- No abort (trap flag 0x9FE000A0 = 0 every run — and the trap is NOW
  functional post-M), no panic (rings clean). The CPU just stops.
- **DISPC scanout blanked** (DISPC_CONTROL/GFX/VID attrs = 0 pre-jump):
  wedge unchanged → scanout contention ruled out. Keep the blank (harmless).
- **ACTLR at kernel entry = 0x1** (bc[3] dump) — the decompressor already
  cleared SMP (bit 6); broadcast-to-CPU1 ruled out. (A9 ACTLR SMP bit = bit 6,
  NOT bit 0; proc-v7 sets bit 6 in setup_processor.)
- **WDT2 window = 60 s EXACTLY, not 15 s** (user-timed twice: blue-off →
  red-on = 60s). The "15s" in all docs = QNX's wdtkick PERIOD. Update docs.

## PL310 FACTS (all verified on-device today)
- cache-id 0x410000C4 = L310 r3p2. Control 0x100=1 (enabled),
  aux 0x104=0x1E070000, tag-lat 0x108=0x00000000, data-lat 0x10C=0x00000111
  (1-cycle RD/WR/SETUP — very aggressive), prefetch 0xF60=0x5, power 0xF80=0.
- **NS access to PL310 CONFIG registers is filtered**:
  * CONTROL (0x100) NS write → **bus hang** (payload froze, 60s WDT2).
  * DATA LATENCY (0x10C) NS write → **synchronous SIGBUS** (fltno=5, abort).
  * NS reads of everything work fine (memdump3).
  * Maintenance regs NS-writable: by-PA CIPA 0x768 ✓ (used everywhere),
    sync 0x730 ✓, by-way 0x7FC = **BACKGROUND op — DEADLOCKS the machine**
    (fired twice with live/other-core traffic; PL310 r3p2 erratum 727915
    class). NEVER use by-way from NS here. Only by-PA foreground ops.
- **Monitor service probe: 0x105 returned 0 but did NOT disable the L2
  (control stayed 1) and BROKE the jump chain** — enter_stub never executed
  (bc=70 never landed in any mirror), probe never ran, LED sequence never
  happened (user: blue ON 2m30s → off → reboot). 0x105 has different
  semantics on this monitor. REVERTED. The kernel was also rebuilt with
  CONFIG_CACHE_L2X0=n (stays — kernel must never touch PL310 config from NS).

## CURRENT BEST THEORY (early-C wedge)
The kernel's early C is the first sustained cacheable write-back traffic
through the L2 in QNX's configuration (data latency 0x111 = 1 cycle, very
aggressive) → random eviction/interconnect stalls → CPU stalls silently on a
fetch/access → sits until the 60s WDT2. Fits: wandering death points, no
abort (transaction hangs, bus doesn't error), no panic, DISPC-independent.
QNX's own historic flakiness (RNDIS timeouts, sync() hangs, "device
instability") is plausibly the same machine behavior under QNX loads.

## BLOCKED PATHS (do NOT retry blind)
- NS write to PL310 control/aux/latency (hang/SIGBUS).
- NS by-way clean+inv 0x7FC (background deadlock).
- Monitor SMC 0x105 (not disable; breaks the jump chain).
- Uncompressed Image (architecturally broken — PHYS_OFFSET derivation).

## NEXT SESSION — FIRST TASKS
1. **RE the monitor's real L2 service IDs.** The QNX-side module
   `dumped4869ifs/proc/boot/trustzone-omap4` (61 KB) is QNX-PACKED (only a
   QNX_info section visible; objdump finds no .text). Unpack it by dumping
   the module's memory from the LIVE device: find it in `pidin arg`, then
   dump its region via /proc/<pid>/as (memdump3-style tool or dd of the as
   file), then objdump the unpacked image and find the SMC service dispatch
   (r12 constants) — looking for L2 set-aux / set-latency / enable / disable
   services in the 0x100 range. KNOWN-GOOD reference service: 0x101 =
   clean+inv by PA (r0=PA, r1=size, returns status in r0).
2. With real service IDs: payload pre-jump (before CPU1 hold) asks the
   monitor to set sane PL310 latencies (e.g. data 0x333) and/or disable L2.
   Verify via NS control/latency readbacks recorded in bc[2]/bc[4].
3. Re-jump the no-L2X0 kernel (already built) with a correctly reconfigured
   or disabled L2. If the early-C wedge disappears → L2-state theory
   confirmed; then re-enable L2 properly (monitor set-aux + enable) or via
   a kernel-side monitor-call patch at l2c_enable time.
4. Alternative bisect if the monitor RE stalls: probe.S long-loop test
   (bare-metal, MMU on, cacheable write loop with flush-free markers) to
   isolate machine-vs-kernel.

## TOOLCHAIN / MECHANICS NOTES (today)
- Kernel GAS: main.c/setup.c are assembled for a pre-v7 baseline — raw
  `dsb`/`dmb`/`smc` mnemonics FAIL ("selected processor does not support").
  Use the kernel's dsb(sy) macro (CP15 mcr form) and .word encodings for SMC:
  dsb sy = 0xF57FF04F, smc #0 = 0xE320F000, dmb sy = 0xF57FF05F.
- jump.sh runs need `cd /home/psyden/playbook-dev/kexec` (rsa path is ../rsa).
- The buildroot objdump chokes on QNX-packed ELFs; use the QNX SDP objdump
  for IFS binaries (and even it sees only QNX_info in packed modules).
- memdump3 prints raw u32 words (not byte dumps) — byte-reverse per word for
  strings.
- DISPC blank (0x48050000: +0x440 CONTROL, +0x4A0 GFX_ATTR, +0x4C0/+0x500
  VID attrs = 0) works from the payload, harmless — keep it.
- The kernel's panic notifier + early_print → rings: USE IT — any BUG/panic
  in early boot is now visible post-reboot in ring1.

## FILES TOUCHED TODAY
- kernel: arch/arm/kernel/head.S (ring-map fix + comments), init/main.c
  (markers, panic notifier, pb_bc_mark), arch/arm/kernel/setup.c (markers
  136/130-135 + pb_bc macro), .config (CONFIG_CACHE_L2X0=n).
- payload: kexec/qnx2linux.c (heartbeats 42-50, DISPC blank bc=34, SMC
  experiment reverted, PL310 comments), kexec/memtest.c + build.sh (memtest).
- docs: /home/psyden/playbook-dev/docs/* (solo-continuation documentation).
