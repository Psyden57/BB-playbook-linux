# PlayBook Linux Boot — Documentation Index

This folder contains all documentation needed to continue the BlackBerry PlayBook mainline Linux boot project solo.

## Quick Start

**Current state (2026-09-02 night, session 6)**: the **console WORKS** — the
boot log lands in the CACHEABLE DRAM rings (ring1 = 0x88000080 count / +0x100
chars, 3840-char window) and PB-PANIC prints panics into them. Decode
memdump3 output by reversing each 4-byte group. The **L2-off mode is DEAD**
(proven by the payload's `--l2test` A/B: the bypass path wedges under
sustained traffic) — the mode is `--l2on` (the L2 stays enabled; markers
flush via monitor SMC 0x101). The kernel boots cacheable to **bc=127
(paging_init: map_kernel done)** and dies deterministically at
dma_contiguous_remap — a corrupted `dma_mmu_remap[0].base`. **Root cause
pinned**: the machine corrupts DRAM transactions under QNX's secure-domain
L2 config — PL310 data latency confirmed live at 1/1/1 cycles (`--l2lat`;
the NS write = SIGBUS, secure-only) and L3/EMIF auto-idle is also
secure-only. **The fix = inside the secure monitor** (RE trustzone-omap4).
The old "171 wall" = the bypass cliff + the head.S C/B/S strip (both fixed);
the bc[2]=0x3E7 mystery = QNX-boot leftovers, not a kernel wild write.

```bash
cd /home/psyden/playbook-dev/kexec
source ../qnx-env.sh
./build.sh                    # build payload + tools
# kernel: cd /home/psyden/kernel/linux && make ... zImage && mkkernel.sh zImage
./jump.sh zImage              # deploy + jump + readback
```

## Handoff for the Next Session
- `SESSION-HANDOFF/HANDOFF_2026-09-02_night_SMC-flush-pv-wall.md` — **START HERE** (the console breakthrough, the corruption root cause, the monitor-RE task list)
- `SESSION-HANDOFF/HANDOFF_2026-09-02_late_171-wall_new-observability.md` — the pre-pivot state (now historical: the 171 wall is solved/retired)
- `SESSION-HANDOFF/SESSION-RECORD_2026-09-02_L2-confirmed_monitor-RE.md` — the L2-confirmation + monitor-RE session record
- `SESSION-HANDOFF/SESSION-RECORD_2026-09-01_ring-map-fix_early-C-wedge.md` — the M=1 fix record
- `ring3-recovered-log-2026-09-02.txt` — run 23's kernel boot log (recovered from the monitor's capture; the monitor capture is inactive in the current rings-only console)

## Document Map

| File | Purpose | Read When |
|------|---------|-----------|
| `00_PROJECT_OVERVIEW.md` | Project goals, hardware, current blocker, ops rules | First — understand the big picture |
| `01_KEXEC_ARCHITECTURE.md` | Payload flow, trampoline, page tables, bc protocol, WDT2 | When modifying qnx2linux.c |
| `02_MEMTEST_PAYLOAD.md` | Memtest design, usage, output format, interpretation | Running/analyzing memtest |
| `03_DEBUGGING_SESSIONS.md` | All session records, killed theories, SMC hang matrix, the 171 wall | Understanding what's been tried |
| `04_KEY_FILES_AND_COMMANDS.md` | File reference, build/deploy/run commands, memory map, ring3 readback | Daily work — keep open |
| `05_NEXT_STEPS.md` | The 171-wall task list, console-visibility test, L2-on endgame | Planning the next run |
| `06_HARDWARE_REFERENCE.md` | OMAP4430 memory map, WDT2 register map, PRCM map, monitor capture rings | Hardware questions |
| `07_TROUBLESHOOTING.md` | Common failures, symptoms, causes, fixes, secure-filter failure modes | When things go wrong |
| `08_KERNEL_DEBUGGING.md` | Kernel boot flow, DEBUG_LL, marker ladder, minimal kernel | Kernel-side debugging |

## OpenViking Memory (Context Recall)

Key URIs for `openviking_read` / `openviking_search`:
- `viking://resources/playbook-dev/SESSION-HANDOFF/...` — all session records
- `viking://user/default/memories/entities/project/playbook_linux_port.md` — the project state summary

## Critical Rules (Never Forget)

1. **Payload SIGSEGV ≠ reboot** (run 31) — a user-mode abort in the payload kills the process, QNX survives, SSH stays up. Only jump-context deaths reboot. Iterate fast.
2. **PRCM (CM1/CM2) registers are secure-filtered** — a CLKSTCTRL write from NS = SIGSEGV (run 31). Direct auto-idle fixes are impossible; the monitor's PPA services are the only path.
3. **Monitor SMC #0 only works from the payload's C-flow `mon_call` shape** — the enter_stub-inline shape hung 9/9 (full matrix in 03). The kernel's C-code SMCs (pb_smc_flush, service 0x101) are C-flow and proven.
4. **Never plain-mmap device memory** — use `mmap_device_memory` + `PROT_NOCACHE` + `MAP_PHYS`
5. **jump.sh launches the payload DETACHED** (>/tmp/jump.log &) — a hung setup must not get SIGHUP-killed
6. **WDT2 window = 58.6 s** (LDR 0xFFE2B400 @ 32.768 kHz); kick = any write to TGR (0x4A314030). QNX's wdtkick daemon (15 s period) is the fallback kicker — after the jump nobody kicks but us.
7. **Sync after every dd/cp** — QNX drops cache on death
8. **UART pad hunt: DEAD** — and the kernel must NOT write UART3 at all: post-idle the UART is unclocked, a posted store to the dead THR stalls the store buffer and a dead-target read stalls forever. The console = the DRAM rings only.
9. **Sustained DEVICE (SO) stores wedge the machine with the L2 on** — the ring/bc sections are CACHEABLE (r7) and their data is flushed in batches via SMC 0x101. Never reintroduce per-char device-store consoles.
10. **Zero unbounded polls boot-wide** (audited 2026-09-02 night) — every PL310 sync poll has a countdown; new code must use pb_smc_flush instead.
11. **memdump3 output words are BIG-ENDIAN-formatted** — reverse each 4-byte group when decoding ring text.

## Key Addresses Cheatsheet

| Address | Purpose |
|---------|---------|
| 0x90000000 | BC primary (bc[1]=step @+4, nonce bc[15] @+0x3C) |
| 0x9FE00000 | BC mirror + abort trap + DEBUG_LL ring |
| 0x88000080 / 0x88000100 | Ring1 count/index + chars |
| 0x88000080 / 0x88000100 | **Ring1 = the kernel console log (PRIMARY console evidence, 3840-char window)** |
| 0x94000080 / 0x94000100 | Ring3 = the monitor's UART3 capture (INACTIVE — the kernel no longer writes UART3) + early_printk mirror target |
| 0x40304000 | IRAM flat L1 table |
| 0x40308000 | Continuation (identity) |
| 0x4A314030 | WDT2 TGR (kick = any write; LDR=0x2C, CRR=0x28, SPR=0x48) |
| 0x4A008700+ | CM2 CORE CLKSTCTRLs (SECURE-FILTERED — no NS writes!) |
| 0x80008000 | zImage decompress target (zreladdr) |

## Decision Tree (After a Jump Run)

```
bc[15] nonce fresh? ── no ──► stale readback; investigate before concluding
bc[1] after jump?
├─ 127 (paging_init: map_kernel)   ──► the CURRENT wall (dma_contiguous_remap
│                                      corruption) — decode the ring1 log tail
├─ 133 (parse_early_param done)    ──► console registered; deeper = better;
│                                      132/133 = the old console-write wedge
├─ climbs past 127 → 134 → 110/111 → initcall breadcrumbs (bc[6]/bc[7])
│                                   ──► progress! bc[7] = the hanging driver's
│                                      fn pointer (resolve via System.map)
├─ 50 (payload era)                ──► payload crash — SSH back in, read bc,
│                                      fix, re-run (no reboot cost)
├─ payload markers < 50            ──► setup-phase problem (eMMC/sync stalls
│                                      seen — retry once)
├─ bc[1] = 0xAB                    ──► real post-M abort fired — read the ring
└─ mirror0 = 70 (without 71)       ──► NORMAL (70 = "jump started"; the 71
                                       writer is gone — do NOT retry on this)
```

## Building New Things

```bash
# Always in kexec/ with env sourced:
cd /home/psyden/playbook-dev/kexec
source ../qnx-env.sh

# Build all:
./build.sh

# Build single:
arm-unknown-nto-qnx6.6.0eabi-gcc -O1 -marm -o mytool mytool.c
```

## Deploy Checklist

- [ ] SSH up (`ssh root@169.254.0.1 echo ok`)
- [ ] Display stack quiesced (`slay splash backlight_win screen`)
- [ ] jump.sh launches payload detached
- [ ] After the run: bc[15] nonce fresh? → ring3 tail → bc[6]/bc[7] → bc[1]

## Updating Docs

After each session/run, update:
- `03_DEBUGGING_SESSIONS.md` — new session record
- `05_NEXT_STEPS.md` — new decision point
- `04_KEY_FILES_AND_COMMANDS.md` — new tools/commands
- `SESSION-HANDOFF/` — the session record + handoff

---

**Last updated**: 2026-09-02 (night, session 6) — console SOLVED (cacheable
rings, SMC 0x101 flushes, PB-PANIC notifier); L2-off mode retired (--l2test
A/B proof); kernel boots cacheable to bc=127 (paging_init) and dies at
dma_contiguous_remap via a corrupted dma_mmu_remap entry; corruption root
pinned to the secure-domain L2 config (PL310 data latency 0x111 confirmed
live, NS write = SIGBUS; L3/EMIF auto-idle also secure-only)
**Primary blocker**: the secure-domain L2 config — the fix = RE the
trustzone-omap4 monitor for a latency/PPA service
**Next action**: read HANDOFF_2026-09-02_night_SMC-flush-pv-wall.md