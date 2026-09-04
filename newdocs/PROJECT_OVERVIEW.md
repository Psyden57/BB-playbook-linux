# Project Overview

## What this is

Research and engineering project: **booting mainline Linux on the BlackBerry
PlayBook tablet (TI OMAP4430, "winchester") from inside the running QNX 6.6
OS** — a kexec-style transition implemented entirely from QNX userland, on
hardware whose boot ROM cryptographically locks everything below the OS
(HS / High Security unit).

There is no PlayBook Linux port to build on. Unlike the Droid 4 (same SoC,
which has tmlind's droid4-kexecboot), nobody has ever run Linux on this
device. Everything device-specific here was reverse-engineered from scratch;
everything SoC-level comes from mainline Linux (omap2plus defconfig, with
PandaBoard / Droid 4 / Galaxy Espresso as OMAP4 references).

## Why it exists

- The PlayBook is a capable OMAP4430 tablet (dual Cortex-A9, 1 GB DRAM,
  64 GB eMMC) that runs a locked-down QNX 6.6 (Tablet OS 2.0). This project
  answers: *can mainline Linux be brought to it without touching the secure
  boot chain?*
- The answer so far: **yes, the mechanism works on real hardware.** The
  project is now in the kernel-bring-up phase (walls being knocked down one
  by one; see [PROJECT_STATE.md](PROJECT_STATE.md)).

## How it works (one paragraph)

A QNX userland payload (`kexec/qnx2linux`) obtains System mode via
`ThreadCtl(_NTO_TCTL_IO_PRIV)`, allocates 24 MB of contiguous physical DRAM,
copies the Linux kernel + DTB into it, builds a flat 1 MB-section page table
and an IRAM stub, holds CPU1 in warm reset, flushes L1, stops the GIC
interrupt controller, then switches TTBR0 to the flat table and branches
(MMU off, `r0=0, r1=~0, r2=DTB`) into the kernel's `stext`. If the kernel
dies, the WDT2 watchdog warm-resets the board after ~59 s, QNX reboots, and
RAM-resident debug artifacts (breadcrumbs + console ring) are read back over
SSH. See [ARCHITECTURE.md](ARCHITECTURE.md) for the full picture.

## Major milestones

| Milestone | State |
|-----------|-------|
| T2 — full jump mechanism proven (test blob, UART, breadcrumbs, WDT2 cycle) | **DONE** (Aug 2026) |
| T3 — mainline kernel boot | **IN PROGRESS** |
| T3.a — boot reaches early C (`start_kernel`) | DONE |
| T3.b — L2 cache state control via secure monitor | DONE (both on/off modes) |
| T3.c — console visibility (DRAM ring capture of all kernel output) | DONE |
| T3.d — bc=127 wall (dma_contiguous_remap) | **SOLVED** (2026-09-03, session 7) |
| T3.e — the session-9 walls (the stale pv regime → CURED; the stale pgd pair → 2MB shave untested) | **CURRENT WALL** (W-39 pending) |
| bc=171 wall (taskstats_init_early) | passed on the Image path (W-4); the zImage-path test resumes after the pgd wall |
| Rootfs (eMMC) mount | Not started |

## What makes this hard (and interesting)

- **HS device**: the boot ROM verifies the boot chain; nothing below the OS
  is replaceable. All work happens *from inside the running OS*.
- **QNX has no kexec**: cache maintenance, flat page table, TTBR0 switch and
  MMU-off were reimplemented from userland, modeled on ARM's
  `machine_kexec` / `relocate_kernel.S`.
- **The secondary core**: QNX runs both A9 cores with no hotplug API. The
  OMAP4 PRCM exposes per-CPU warm-reset registers — CPU1 is simply held in
  reset across the jump.
- **No serial console**: no usable UART pads exist on the board. The project
  built RAM-based debug channels instead (breadcrumb ladder + a DRAM console
  ring + the secure monitor's own UART3 capture), all read back over SSH
  after each watchdog reset cycle.
- **The secure monitor (TrustZone)**: PL310 L2 control and PRCM are
  secure-only from NS. A reverse-engineered SMC service table (0x100–0x113,
  plus PPA services) governs what the kernel may and may not touch.

## Repository layout

```
kexec/                the payload, stubs, probes and tools (the daily work dir)
docs/                 the 10-file deep knowledge base (00-08)
SESSION-HANDOFF/      per-session records and handoffs
newdocs/              this documentation set (project-level, session notes)
*.md (root)           earlier deep-dive documents (kexec design, hardware RE...)
```

Note: `device-binaries/`, `dumped4869ifs/`, `optimized-docs/` and the TI TRM
PDF are **local-only** (proprietary QNX/BlackBerry/TI material dumped from
the device or downloaded; deliberately excluded from Git — see
[SETUP.md](SETUP.md) for how to recreate them).
