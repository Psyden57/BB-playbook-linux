# BOOTSTRAP SESSION 13 (written 2026-09-12, session 12's wrap-up product)

## THE ONE-PARAGRAPH STATE

Session 12 ran W-84..W-89 (6 runs, 5 builds) and reframed the L2-on
path completely. THE --L2ON ERRATUM: the --l2on mode had DISABLED the
L2 since the W-46 rewrite (a692300) — every W-78..83 "L2-on" run ran
L2-OFF (probe.S's bc[8] = the PL310 CTRL readback = 0 in all; the
erratum 86c4e29 in docs/03). The era --l2on semantics were restored
(no SMC, bc[10]=CTRL expect 1) and the first genuine L2-on runs since
W-46 revealed: **the L2-on early C = the session-9 STALE-VIEW LOTTERY
— three boots (W-84, W-88, W-89) = three DIFFERENT death points** (the
early C [144→145] with the 0x3E7 wild write; [98 → 96]
local_flush_tlb_all in devicemaps_init after PASSING the clear_fixmap
TLB op on the way; mid-parse_early_param with the 0x3E7). The front =
a DISTRIBUTION (per-run decompressor-era L2 eviction luck), not a
point; the TLB-op skips = marginal relief; the poison = upstream. THE
CURE = the wide stale-line invalidation (the monitor 0x101 clean+inv
or the NS 0x770 inv-by-PA — proven in the session-10 audit — over the
image + pgd + .data/.bss regions) BEFORE the C world reads; the
W-32c/111 sweeps cover the pv and the pgd only. **THE OBSERVABILITY =
UPGRADED AND PROVEN ON-DEVICE (both channels):** (a) the ring BATCH
flush (every 64 chars, guarded on the PL310 enabled, NS 0x7F0 CIPA ×36
+ the bounded 0x730) = the console log survives in the L2-on state
(W-88 = 1165 chars = the full boot log); (b) the payload's LED phase
markers — blue = the payload, MAGENTA = "the kernel owns the machine"
(set BEFORE the GICD-off; frozen magenta in video = "died at the
jump"). The kernel runs = #155 = 6.15.11 SMP=n, the CMA TLBIALL
skipped, the devicemaps_init local_flush_tlb_all skipped, the sweeps
via SMC, run with **--l2on**.

## THE MANDATORY READ ORDER (before any work)

1. `newdocs/HANDOFF.md`, `DEVELOPMENT.md`, `SETUP.md` (the standing
   rules)
2. `newdocs/PROJECT_STATE.md` (rewritten for session 12)
3. `newdocs/session-notes/session-12.md` (the run map W-84..W-89 + the
   new rules — THE session-12 knowledge base)
4. `docs/03_DEBUGGING_SESSIONS.md` (the W-84..W-89 records +
   the session-12 erratum, append-only)
5. `docs/README.md` (rules 1-17) + `newdocs/KNOWN_ISSUES.md`
6. `newdocs/audit-approach-2026-09-11.md` (the NS 0x770 inv-by-PA
   proof — the W-90 cure's tool)
7. `PLAYBOOK-REFERENCE.md` §5/§8/§10 (the safety model)

## DEVICE FACTS (verified this session)

- **NEW RULE: nothing QNX-side after the GICD-off (bc 41)** — the
  interrupt-driven drivers block forever on the masked completion IRQ.
  The W-86/87 deaths = the i2c devctl (the LED magenta) frozen inside
  the GICD-off window: the I2C bytes went out (the LED = magenta on
  video) but the devctl never returned; the payload froze; the WDT2
  expired exactly 59 s later. 2/2 deterministic across placements.
- **NEW: the direct NS access to I2C4 (0x48350000) = SIGBUS (fltno=5,
  the MMCHS secure-filter class; the box SURVIVED unlike the W-28
  freeze)** — the kernel-side LED colors = hardware-impossible; the
  LED = the QNX devctl path only (led_color_qnx: R=0x02 G=0x04 B=0x08
  on GENERAL@0x10 via /dev/i2c3).
- **The clean-machine (battery-pull) baseline = the readback freshness
  discriminator**: the bc page + the rings = the uniform 0xAA filler;
  any real marker = unmistakable. W-89's bc[19..31] = pure filler =
  the proof the kernel never ran.
- **A dark-hang run = evidence-negative by construction**: no WDT2
  reset = no fresh bc page; the recovery power-hold tramples the DRAM
  completely (W-85 = worse than the W-73 partial trample). The LED
  timeline + the ring = the only evidence channels for such runs.
- The LED scheme: blue = the payload start, MAGENTA = the payload is
  past the file reads (~15 s before the jump; set at bc 42, safely
  before the GICD-off). **USER CORRECTION (the W-89 debrief): the
  bc-32-era magenta (the ~1-3 s window) was NEVER seen in W-88/89 —
  only blue; the write now sits at bc 42 for ~15 s of visibility.**
  Frozen magenta in video = "died at/before the jump in video".
- The memdump3 on-device binary = wiped by any reboot (the battery
  pull AND the hard reset) — re-scp from kexec/ before any readback.
- The kernel-side make MUST run in a shell without qnx-env.sh (the
  QNX make 3.82 hijacks the build: "GNU Make >= 4.0 required").

## THE FIRST TASK (W-90): the stale-line cure

The L2-on lottery = the decompressor-era L2 lines (the image region,
the pgd region, the .data/.bss the C world reads) served to cached
reads/PTW walks with per-run luck. The W-32c (the pv) and the #111
(the pgd) sweeps = the two-point patches. THE CURE = the WIDE
invalidation BEFORE the C world starts reading:

1. Inventory what the C world reads early: the kernel image
   (.text/.data/.bss = the decompressor's destination 0xa0008000+),
   swapper_pg_dir (PA 0xa0004000), the bc page (already fresh), the
   DTB (the appended + the payload's copy), the cmdline.
2. The invalidation mechanism: the monitor SMC 0x101 (clean+inv by PA
   — proven, but the CLEAN step writes the stale lines back = the
   W-33 poison risk!) vs the NS 0x770 inv-by-PA (INVALIDATE ONLY, no
   clean-back — proven NS-writable by the l2canary ladder, the
   audit-approach Path B). **0x770 inv-only = the right op**: the
   stale lines get discarded, the DRAM truth (the decompressor's
   stores = SO = already in DRAM) gets served.
3. Placement: in head.S's tail / the early C (the W-32c block region)
   — after the MMU is on (the loop = C code, the PL310 VA 0xFEB42000
   = mapped) and BEFORE the reads (the pv store block already runs
   there). The range: [0xa0000000, arm_lowmem_limit) = the whole
   lowmem?? Too big (~450 MB = 14M lines). NARROW IT: the image +
   the pgd + the .data/.bss = ~10 MB = ~300k lines = still big for a
   pre-jump loop (~5-10 s of device ops — the cliff = 4-5k
   STORES/reads; the CIPA ops = writes to one register = count them
   as device ops = 300k = WAY over). ALTERNATIVE: the payload-side
   pre-jump sweep (QNX-side, no cliff — the device-op cliff = the
   L2-ON kernel-era device STORES; the payload's pre-jump loop =
   userland, unbounded?) — the payload already sweeps the image
   region (bc 47, the #111 image sweep via 0x7F0)! EXTEND IT: the
   payload sweeps the ENTIRE buffer (the kernel + the DTB) + the
   decompressor destination [0xa0008000, _edata) + the pgd PA —
   pre-jump, QNX-side, with the L2 ON (the l2canary-proven op).
4. THE A/B: W-90 = the payload's extended sweep + run --l2on. If the
   lottery = gone (3/3 deterministic past the early C) = THE CURE; if
   not = the stale source = elsewhere (the QNX-era lines in the
   untouched DRAM regions?).
5. ONE VARIABLE: no other changes; the skips stay as they are.

## THE BEQUEST ORDER (after the stale-line cure falls)

1. The LED heartbeat: a free-running "alive" blink = needs a timer
   post-start_kernel — OR the cheap version: the kernel's pb_bc_put
   sites toggle... (the I2C = closed; EN-low = the color-killer — the
   LED = payload-side only, done). REVISIT only if a new LED channel
   appears.
2. The 171 wall (the taskstats/kmem_cache era) — after the cure, the
   L2-on boots should reach it; the session-5-era markers (181-185)
   = ready.
3. The SMP bring-up (the bequest): the CPU1 release done RIGHT (the
   SAR neutralization 0x4A326B00 + the kernel-side re-hold in head.S
   + the park blob) = the real TLB-op cure AND the second core. THE
   PARK IS STILL FORBIDDEN without the SAR neutralization.
4. The rootfs era: the eMMC, the initramfs, the console = a real
   driver (the earlycon → ttyO2).
5. The display (the DISPC RE) — the user's RE targets.
6. The RE queue: the bootrom's CH* parsers, the ip=0xF0 service, the
   registry blob (id 0x18), the hidden 4 KB dispatch region.

## THE STANDING RULES (the short list — full: docs/README.md)

- ASK THE USER before every device run; hands-off; unfiltered jump.sh
  output (never `| tail`); record video; the LED timings on request
  (the magenta = subtle — the user may see it as red+blue).
- python for hex decode; grep, don't recall; the native edit tools.
- Shipped-binary verification (rule 16) before every run — this
  session proved it again (the flush/pb_led verification caught the
  build order; the qnx-env make 3.82 hijack nearly shipped a stale
  zImage).
- NVRAM and RPMB (hd6) untouchable. The device clock = don't trust.
- git commit + push every state change; the bootstrap = the wrap-up
  product.
- NEW: nothing QNX-side after the GICD-off (the W-86/87 rule).
- NEW: the kernel make = a clean shell (no qnx-env).

## ADDENDUM (the session-12 debrief)

- **OpenViking = DROPPED (2026-09-12)** — do not look for or write
  viking:// references; the repo = the only context store (newdocs/ +
  docs/ + SESSION-HANDOFF/ per newdocs/HANDOFF.md). The remaining
  OpenViking mentions in the docs (the session-05 note, the docs/05
  update) = historical/dated.
- **The LED magenta timing correction**: the bc-32-era magenta (~1-3 s
  before the jump) was NEVER seen by the user in W-88/89 (only blue);
  the write = MOVED to bc 42 (mid-payload, ~15 s of visibility) — the
  payload carries the fix; expect blue → magenta → off on every run.
- **The session-end convention (the user's rule, now written down):
  wrap a session ONLY when the context is at 40-60%** — do not wrap
  early because a "clean narrative point" appears; there is always more
  diagnostic value in continuing while the context lasts. W-90 (the
  stale-line cure) = fully within a session's remaining budget when the
  wrap urge hits — KEEP WORKING unless the user says stop.
