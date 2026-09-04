# Design Decisions

Decisions are recorded here with their rationale. "Supersedes" notes point
at older docs whose text may still reflect the older decision.

## D1: RAM breadcrumbs + DRAM console ring instead of a serial console

No usable UART pads exist on the PlayBook board (the pad hunt is documented
and closed in `LED-RE.md` / docs). Instead: strongly-ordered stores to fixed
DRAM addresses (bc page) and a DEBUG_LL hook that mirrors every earlycon
character into a DRAM ring. Both survive the WDT2 warm reset (DRAM content
persists), and are read back over SSH after QNX reboots. Supersedes the
early UART-pad ideas.

## D2: CPU1 held in PRCM warm reset across the jump

QNX runs both A9 cores; there is no hotplug API. The OMAP4 PRCM exposes
per-CPU warm-reset bits ("one CPU can set this bit to reset the other CPU;
kept in reset until the other active CPU clears this bit", TRM Table 4-30).
Holding CPU1 across the jump is simple and proven. The kernel re-enables SMP
itself.

## D3: Monitor SMC services for cache maintenance (C-flow shape only)

The PL310 is in the secure domain; NS writes to CTRL/AUX/latency abort
(SIGBUS), by-way ops deadlock. All L2 maintenance from the payload goes
through monitor SMC 0x101 (clean+inv by PA). Empirical: SMCs work from the
C-flow `mon_call()`; SMCs inline in the enter_stub never return. The SMC
immediate (#0 vs #1) is ignored by the monitor.

## D4: `--l2on` is the boot mode; the L2-off mode is retired

Session 6 proved the L2 bypass path wedges under sustained traffic, and the
"171 wall" of that era was partly the bypass cliff. The boot keeps QNX's L2
running (`--l2on`, bc[10] = PL310 CTRL readback = 1 proves it). The kernel's
L2X0 driver will need care when reached (see KNOWN_ISSUES). Supersedes the
L2-disable era of docs/03 and the "kernel stays L2-off" rule in older
handoffs.

## D5: Runtime DTB bank patching + kern_off placement (session 7)

QNX's free physical pool has no 2 MB-aligned 24 MB run (proven by the
129-slot sweep: all honored hints fragmented; 12 generic blocks, all
unaligned), but the kernel's pv fixup needs a 2 MB-aligned PHYS_OFFSET.
Solution: accept any valid buffer, place the kernel at the first 2 MB-aligned
offset inside it (`kern_off`), and patch the DTB /memory node's reg at
runtime to match (`fdt_patch_memory`). **UPDATE 2026-09-04 (W-21): the
DTS-baked bank must be 0xa0000000 + 512 MB** (zreladdr = 0xa0008000 ⇒
PHYS_OFFSET = 0xa0000000 — the old "0xa4000000 + 448 MB" default put the
kernel outside its own memory). **UPDATE 2026-09-04 (session 9): the
sweep also REJECTS any window intersecting [0xa0000000, 0xa1000000) —
the zreladdr inflation region (W-35: the decompressor relocates itself
into an overlapping window and the boot died in head.S's tail).**

## D6: zImage over uncompressed Image for kernel boots

Session 7 root-caused the bc=127 wall and fixed the placement, but the
uncompressed-Image path still loses the DTB (r2 chain; W-4/W-5). The zImage
decompressor natively delivers the appended DTB and auto-derives zreladdr
(`CONFIG_ARM_APPENDED_DTB`, `CONFIG_AUTO_ZRELADDR`). The zImage is the
proven session-6 path — use it. The Image-path r2 mystery is parked, not
solved.

## D7: No fabricated Git history

The repository's first commit represents the project as it stands after the
documentation work (session 7). Nothing is backdated or attributed
retroactively.

## D8: Proprietary material stays out of Git

Device dumps (`device-binaries/`, `dumped4869ifs/`), QNX docs
(`optimized-docs/`), the TI TRM PDF, the SSH key and build artifacts are
local-only (gitignored). The repo documents how to recreate them. Rationale:
legal (proprietary RIM/QNX/TI code), size, and reproducibility.

## D9: License — UNRESOLVED

No LICENSE file yet (the author's decision, pending). The kernel-tree
portions are derivative of GPL-2.0 code, which constrains the eventual
choice; noted in ROADMAP.

## D10: MMU-off helpers are INLINED, never called (session 8)

The called `__fixup_pv_table` never executed a single instruction on
the zImage path — through `bl`, a computed `blx` with a
post-mortem-verified target, both L2 states, I=0/1, with byte-correct
DRAM (KNOWN_ISSUES #10, runs W-9→W-16). The INLINED copy executes
(W-17) and is build-luck-dependent (W-21/22's pv=0 until the W-24
C-world invalidate). Rule going forward: any helper that must run
MMU-off in the streamed region gets INLINED into head.S, never called
via bl/blx. Mechanism of the paradox: open (session-08 notes hold the
evidence).

## D11: The pv cure = direct store in start_kernel, never SMC 0x101
### against stale lines (session 9)

The stale-pv regime (decompressor-era L2 lines + the fixup's SO stores
bypassing L2, per-run eviction luck) broke every C-world __va/__pa
inline. The W-24 fix was self-defeating (its __pa consumed the stale pv
it repaired; SMC 0x101's CLEAN step writes the stale L2 line back to
DRAM — W-33's 8/8 failure). The W-32c block (build #106, in
start_kernel before setup_arch): directly STORE the build constants
(__pv_offset = 0xffffffffe0000000, __pv_phys_pfn_offset = 0xa0000) and
DCCIMVAC — the clean carries the CORRECT value through the L2C.
Deterministic (tries=0). The same store+clean pattern is the template
for the stale pgd pair (KNOWN_ISSUES #4b).

## D12: Device-register NS access = tested before use (session 9)

MMCHS (0x480B4000+) SIGBUS'd from NS (fltno=5) AND the abort class
froze the box (W-28). DISPC = accessible (kill + readback verified).
Never assume a device register is NS-touchable: verify from a prior
run's evidence first; treat external aborts as device-wedging, not
cheap crashes.
