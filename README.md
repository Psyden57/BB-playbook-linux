# PlayBook kexec — booting mainline Linux on the BlackBerry PlayBook from QNX

Research project: **booting mainline Linux on the BlackBerry PlayBook (TI
OMAP4430) by jumping from the running QNX OS into a Linux kernel — a
kexec-style transition implemented entirely from QNX userland**, on hardware
whose boot ROM locks down everything below the OS (HS / High Security unit).

Status: **the complete jump mechanism is proven working on real hardware**,
and **mainline Linux 6.15.11 boots through `start_kernel` into initcalls**.
The secure monitor is used to keep the L2 cache enabled across the jump, all
kernel console output is captured into DRAM and survives the watchdog reset
cycle, and the most recent wall (a page-table/memblock misconfiguration at
breadcrumb 127) was root-caused and fixed. The current wall is at breadcrumb
171 (`taskstats_init_early` → `kmem_cache_create`) — see
[newdocs/PROJECT_STATE.md](newdocs/PROJECT_STATE.md).

```
QNX 6.6 (Tablet OS 2.0)                          bare metal
┌────────────────────────────────┐   TTBR0     ┌──────────────────────────┐
│ qnx2linux:                     │   switch    │ trampoline → continuation│
│  kernel+DTB copy, flat table,  │ ──────────► │ (MMU off, r0=0, r1=~0,   │
│  CPU1 held, L1 flushed,        │   identity  │  r2=DTB)                 │
│  GICD off, IRAM stub armed     │   branch    │  → Linux kernel          │
└────────────────────────────────┘             └──────────────────────────┘
        watchdog WDT2 (~59s) fires ─► warm reset ──► QNX boots again
              (RAM breadcrumbs + console ring survive the warm reset)
```

## Why this is unusual

- **No PlayBook Linux kernels exist.** Unlike the Droid 4 (same SoC, see
  tmlind's droid4-kexecboot), nobody ever ported Linux to the PlayBook.
  Everything device-specific here is built from scratch; everything
  SoC-specific comes free from mainline (omap2plus).
- **HS (High Security) device.** The boot ROM cryptographically verifies the
  boot chain; nothing below the OS is replaceable. Everything works *from
  inside the running QNX OS*, using root obtained through a software
  weakness plus a reproducible early-boot code-execution vector (below).
- **QNX has no kexec.** The whole transition — cache maintenance, flat page
  table, TTBR0 switch, MMU-off — was reimplemented from userland, modeled on
  ARM's `machine_kexec`/`relocate_kernel.S`.
- **The secondary core.** QNX runs both Cortex-A9 cores with no hotplug API.
  The OMAP4 PRCM exposes per-CPU warm-reset registers — CPU1 is held in
  reset across the jump.
- **Debugging without a serial console.** No UART pads exist on the board.
  The project uses RAM-based channels instead: a breadcrumb ladder, a DRAM
  console ring fed by a DEBUG_LL hook, and the secure monitor's UART3
  capture — all read back over SSH after each watchdog reset.

## The injection point (how root code execution works)

On Tablet OS 2.0.0.4869, the IFS boot script runs `ksh /base/scripts/startup.sh`,
which sources `/radio/scripts/env.sh` if it exists. On WiFi-only units there is
no radio partition and this OS version does not sanitize the missing `/radio`
path — so files placed under `/radio/` in the **UFS partition** (which is
modifiable, see bb10mt) execute as **root during runlevel 0**, before pps,
powerman, screen, audio or USB management even start.

## Documentation

| Start here | For |
|---|---|
| [newdocs/PROJECT_OVERVIEW.md](newdocs/PROJECT_OVERVIEW.md) | what/why/status |
| [newdocs/PROJECT_STATE.md](newdocs/PROJECT_STATE.md) | the current technical state |
| [newdocs/ARCHITECTURE.md](newdocs/ARCHITECTURE.md) | components, channels, monitor services |
| [newdocs/SETUP.md](newdocs/SETUP.md) | host/device setup |
| [newdocs/DEVELOPMENT.md](newdocs/DEVELOPMENT.md) | build/run/debug workflow |
| [newdocs/ROADMAP.md](newdocs/ROADMAP.md) | what's next |
| [newdocs/HANDOFF.md](newdocs/HANDOFF.md) | the multi-session protocol |
| [kexec/README.md](kexec/README.md) | the tools and run procedure |

Deep knowledge (read on demand):

- `docs/00-08` — the 10-file knowledge base (architecture, hardware
  reference, debugging sessions log, troubleshooting, kernel debugging)
- `KEXEC-DESIGN.md` — the jump design (memory map, stub, CPU1, test plan)
- `PLAYBOOK-REFERENCE.md` — consolidated device/OS/bootchain/NVRAM knowledge
- `LED-RE.md` — the LED driver reverse engineering
- `SESSION-HANDOFF/` + `SESSION-HANDOFF.md` — per-session research records
- `newdocs/contradictions/` — claims that conflict across sessions, with the
  evidence for each

## Repository layout

| Path | Contents |
|---|---|
| `kexec/` | the working code: payload, stubs, probes, debug tools |
| `docs/` | the 10-file deep knowledge base |
| `newdocs/` | project-level docs, session notes, contradictions |
| `SESSION-HANDOFF/` | per-session records and handoff prompts |
| root `*.md` | earlier deep-dive documents (kept for history) |

**Local-only (not in Git):** device-specific dumps and proprietary material
(`device-binaries/`, `dumped4869ifs/`, QNX docs, the TI TRM PDF, the SSH
key, build artifacts) — how to recreate them is in
[newdocs/SETUP.md](newdocs/SETUP.md).

## Safety model

- The working unit is treated as precious: **no boot-chain writes, no RPMB
  access, no nuke opcodes, ever.** Flash writes are limited to the UFS
  partition via the established bb10mt repack path.
- Jump experiments are inherently recoverable: anything that hangs is caught
  by the watchdog (WDT2, ~59 s) or the PMIC watchdog (127 s, full power-off),
  and the device returns to an untouched normal QNX boot. The injection
  payload is opt-in (default: normal boot).

## Hardware reference (the PlayBook)

| Component | Part | Linux support |
|---|---|---|
| SoC | TI OMAP4430 (ES2.2, HS), dual Cortex-A9 @1GHz | mainline (omap2plus) |
| DRAM | 1 GB Elpida, 2 banks @ 0x80000000–0xBFFFFFFF | trivial |
| PMIC | TI TWL6030 (I2C1) | mainline |
| WLAN/BT/FM | TI WL1283 WiLink 7 | mainline (wl128x) |
| Audio | Wolfson WM8994 | mainline |
| eMMC | Samsung, on **MMC2** @ 0x480B4000 | mainline (omap_hsmmc) |
| Touch | Cypress CY8CTMA3 on MCSPI3 @ 0x480BA000 | custom needed |
| Display | 1024×600 MIPI DSI panel (DSS @ 0x58000000) | custom needed |
| Debug UART | **UART3 @ 0x48020000, 115200 8N1, 48 MHz** (`console=ttyO2`) | mainline |
| Accelerometer / gyro | Bosch BMA150 / Invensense MPU-3050 | mainline |

## Credits & references

- tmlind's droid4-kexecboot and maemo-leste — the kexecboot model on the same SoC
- postmarketOS `linux-postmarketos-omap` — near-mainline OMAP4 kernel baseline
- The QNX/BB10 reverse-engineering community (bb10tools by a fellow researcher)
- TI SWPU231AP OMAP4430 TRM

## Legal / ethical note

All research is performed on personally-owned, end-of-life hardware (PlayBook
EOL ~2015, BlackBerry services for it long dead). No DRM-circumvention is
involved: the signed boot chain is left fully intact, and the work documents
defensive knowledge about a legacy platform's security design. Proprietary
binaries dumped from the device are deliberately excluded from this
repository.

## License

Not yet chosen — see [newdocs/DECISIONS.md](newdocs/DECISIONS.md) (D9).
Note: portions of this project patch the Linux kernel, which is GPL-2.0.
