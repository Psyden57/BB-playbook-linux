# PlayBook Linux Boot — Deep Reference Index

This folder is the **deep reference + evidence layer** of the project. The
**live** project documentation (current state, roadmap, session notes,
contradictions) lives in **[`../newdocs/`](../newdocs/) — start there**.
Where a file here disagrees with `newdocs/`, `newdocs/` wins.

## File status map

| File | Status | Role |
|------|--------|------|
| `00_PROJECT_OVERVIEW.md` | **HISTORICAL** (session-6 snapshot) | superseded by `newdocs/PROJECT_OVERVIEW.md` + `PROJECT_STATE.md`; kept for the ops-rules detail |
| `01_KEXEC_ARCHITECTURE.md` | AUTHORITATIVE (amended in place) | payload flow, trampoline, page tables, bc protocol, WDT2 — the deep version of `newdocs/ARCHITECTURE.md` |
| `02_MEMTEST_PAYLOAD.md` | AUTHORITATIVE | memtest design/usage/interpretation |
| `03_DEBUGGING_SESSIONS.md` | **APPEND-ONLY RUN LOG** — never rewrite | every run's readbacks and analysis, sessions 1-7 era. The audit trail for every claim |
| `04_KEY_FILES_AND_COMMANDS.md` | AUTHORITATIVE | file reference, build/deploy/run commands, memory map |
| `05_NEXT_STEPS.md` | **HISTORICAL** | the session-6-era task list; superseded by `newdocs/ROADMAP.md` + `PROJECT_STATE.md` (kept: the rationale behind each task) |
| `06_HARDWARE_REFERENCE.md` | AUTHORITATIVE (amended in place) | OMAP4430 memory map, WDT2 registers, PRCM map, PL310 service table |
| `07_TROUBLESHOOTING.md` | AUTHORITATIVE (amended in place) | failure symptoms → causes → fixes |
| `08_KERNEL_DEBUGGING.md` | AUTHORITATIVE (amended in place) | kernel boot flow, DEBUG_LL, marker ladder |

Amended-in-place files carry dated UPDATE/ERRATUM notes where later sessions
corrected them — the corrections are never silent (see
`newdocs/contradictions/` for the full ledger).

## Critical Rules (Never Forget)

1. **Payload SIGSEGV ≠ reboot** — a user-mode abort in the payload kills the
   process, QNX survives, SSH stays up. Only jump-context deaths reboot.
2. **PRCM (CM1/CM2) registers are secure-filtered** — a CLKSTCTRL write from
   NS = SIGSEGV. Direct auto-idle fixes are impossible.
3. **Monitor SMC #0 only works from the payload's C-flow `mon_call` shape**
   (service id in **r12**) — the enter_stub-inline shape hung 9/9; the
   trustzone-devctl passthrough (fnid in r0) is the session-1 call-shape
   artifact, not a service test.
4. **Never plain-mmap device memory** — use `mmap_device_memory` +
   `PROT_NOCACHE` + `MAP_PHYS`.
5. **jump.sh launches the payload DETACHED** (`>/tmp/jump.log &`) — a hung
   setup must not get SIGHUP-killed; ssh timeouts SIGHUP-kill payloads
   (the "silent deaths", fixed by the 120 s timeout).
6. **WDT2 window = 58.6 s** (LDR 0xFFE2B400 @ 32.768 kHz); kick = write to
   TGR (0x4A314030). The **TWL6030 PMIC watchdog (127 s) is a full
   power-off — DRAM content is LOST**; the blob holds both cores so the
   warm reset wins the race.
7. **Sync after every dd/cp** — QNX drops cache on death.
8. **The kernel must NOT write UART3** — post-idle the UART is unclocked; a
   posted store to the dead THR stalls the store buffer. The console = the
   DRAM rings only.
9. **Sustained DEVICE (SO) stores wedge the machine with the L2 on** at
   ~4-5k ops (the device-op cliff) — ring/bc sections are CACHEABLE,
   flushed in batches via SMC 0x101. Never reintroduce per-char
   device-store consoles.
10. **Zero unbounded polls boot-wide** — every PL310 sync poll has a
    countdown; new code must use `pb_smc_flush`.
11. **memdump3 output words are BIG-ENDIAN-formatted** — reverse each
    4-byte group when decoding ring text.
12. **NVRAM and RPMB are untouchable** (brick hazards — README safety model,
    PLAYBOOK-REFERENCE §5/§8).

## Key Addresses Cheatsheet

| Address | Purpose |
|---------|---------|
| 0x90000000 | BC primary (bc[1]=step @+4, nonce bc[15] @+0x3C) |
| 0x9FE00000 | BC mirror + abort trap + DEBUG_LL ring |
| 0x88000080 / 0x88000100 | Ring1 count/index + chars — **the kernel console log (PRIMARY console evidence, 3840-char window)** |
| 0x94000080 / 0x94000100 | Ring3 = the monitor's UART3 capture (INACTIVE — the kernel no longer writes UART3) + early_printk mirror target |
| 0x40304000 | IRAM flat L1 table |
| 0x40308040 | Continuation (identity) |
| 0x40309000 / 0x40309800 | probe.bin / params block (kernel entry, DTB phys) |
| 0x4A314030 | WDT2 TGR (kick = any write; LDR=0x2C, CRR=0x28, SPR=0x48) |
| 0x4A008700+ | CM2 CORE CLKSTCTRLs (SECURE-FILTERED — no NS writes!) |
| 0x80008000 | zImage decompress target (zreladdr) |

## Decision Tree (After a Jump Run)

```
bc[15] nonce fresh? ── no ──► stale readback; investigate before concluding
bc[1] after jump?
├─ 126/171 (dma_contiguous / taskstats era) ──► see the era matrix in
│        newdocs/contradictions/171-wall-analyses.md; decode the ring1 tail
├─ climbs past 171 → initcall breadcrumbs (bc[6]/bc[7])
│                                   ──► progress! bc[7] = the hanging driver's
│                                      fn pointer (resolve via System.map)
├─ l2x0_of_init wedge (init_IRQ)    ──► the by-way 0x7FC deadlock
│                                      (newdocs/KNOWN_ISSUES #3)
├─ 50 (payload era)                 ──► payload crash — SSH back in, read bc,
│                                      fix, re-run (no reboot cost)
├─ payload markers < 50             ──► setup-phase problem (eMMC/sync stalls
│                                      seen — retry once)
├─ bc[1] = 0xAB                     ──► real post-M abort fired — read the ring
└─ mirror0 = 70 (without 71)        ──► NORMAL (70 = "jump started"; the 71
                                       writer is gone — do NOT retry on this)
```

## Deploy Checklist

- [ ] Device charged (a full cycle costs minutes; WDT2 window ~59 s)
- [ ] SSH up (`ssh root@169.254.0.1 echo ok`)
- [ ] Display stack quiesced (`slay splash backlight_win screen`)
- [ ] jump.sh launches payload detached
- [ ] After the run: bc[15] nonce fresh? → ring1 tail → bc[6]/bc[7] → bc[1]
- [ ] Note LED timings (user video-records runs)

## Updating Docs

After each session/run:
- `03_DEBUGGING_SESSIONS.md` — append the run record (append-only)
- `newdocs/PROJECT_STATE.md` + `newdocs/session-notes/session-NN.md` — the
  live state and the session's unique knowledge
- `newdocs/contradictions/` — anything that conflicts with prior claims
- `04_KEY_FILES_AND_COMMANDS.md` — new tools/commands

---

**Status note (2026-09-03, session 7 reconciliation)**: this folder is the
deep reference; the live state moved to `newdocs/`. The session-6-era
"corruption root cause pinned" and "fix = RE trustzone-omap4" claims in
older snapshots are superseded — see `newdocs/PROJECT_STATE.md` and
`newdocs/contradictions/machine-corruption-vs-code-bugs.md`.
