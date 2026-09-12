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
13. **Verify arithmetic with python, not in your head** — STRENGTHENED
    2026-09-12 (session 13, after the "15,519 anomaly" cost three runs):
    EVERY hex value that feeds a conclusion goes through python (or bc) —
    no mental arithmetic, ever, not even "trivial" ones. This explicitly
    includes: (a) every value read from a print — note the print's format
    specifier (%x/%llx = HEX, %u/%d = DECIMAL) BEFORE interpreting the
    value; a value's radix is part of the arithmetic and a decimal misread
    of a hex print has the same standing as a wrong conversion; (b) every
    bc/ring decode — via a decode script, never eyeballed; (c) every
    size/offset/alignment/range comparison (a `size & 31` check, an A-B
    diff, a "does X match Y" claim) — python-computed; (d) every
    build-artifact comparison (a size delta, a header field read back
    from a binary). The analyst's record: three consecutive hand slips in
    session 7 (one caused a false 84 KB "rebuild anomaly"), and in
    session 13 a hex print read as decimal produced a phantom "old DTB"
    that drove three full runs (W-90a's fossil narrative, W-91's
    whole-DRAM sweep, W-92's capture) before W-93's capture dissolved it.
14. **Verify source claims with grep/read, never memory** — recalled
    code snippets have been fabricated before (omap-secure.c reserve,
    head-common.S fixup calls — neither exists). Every claim about what
    a file contains gets a real grep/read first.
15. **Use the native read/edit/write tools for file work** — no
    python3/sed one-liner edits (python3 as a calculator for rule 13 is
    fine); prefer Edit over shell rewrites.
16. **Verify the shipped binary, not the build log** — after every
    kernel/payload build, objdump the actual artifact (symbol present,
    new instructions in place, size delta sane) before running; a
    rebuild has silently produced a stale packed image before
    (session 3, run 8).
17. **Discriminate pb_bc pairs via the mirror channel** (session 9) —
    setup.c's pb_bc(130-136) pair-writers COLLIDE with mmu.c's
    PB_MMU_BC ladder numbers: PB_MMU_BC also writes the mirror
    (0xD4000004 = v|0x200), setup.c's pb_bc does not. A mirror value of
    0x46 (70, payload-era) with bc[1]=133 means setup.c's marker, not
    map_lowmem's. W-20's "133 = map_lowmem done" reading was probably
    the setup.c marker. Also: shared bc slots get overwritten by later
    phases (bc[10] by parse-era markers) — put retry/forensic values in
    surviving slots (bc[19]-bc[25], read via
    `memdump3 90000040 0x30`).

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
| 0xa0008000 | zImage decompress target = zreladdr (AUTO_ZRELADDR bucket 0xa0000000 + TEXT_OFFSET; PHYS_OFFSET = 0xa0000000 — the DTS bank must match, see the W-21 record). **The placement sweep reserves [0xa0000000, 0xa1000000) for the inflation + relocated decompressor (session 9, W-35).** |
| 0x90000040-0x64 | Extended bc slots (session 9): bc[16]/bc[17] = DISPC kill readbacks (expect 0/0), bc[18] = payload placement, bc[19] = pmd/readback, bc[20]-bc[25] = the pmd pattern (VAs 0xdfc/0xdfe/0xdf8/0xdf4/0xdf0/0xdfa) — read via `memdump3 90000040 0x30` |

## Decision Tree (After a Jump Run)

```
bc[15] nonce fresh? ── no ──► stale readback; investigate before concluding
bc[1] after jump? (the zImage ladder, W-9→W-38 era; 2026-09-04)
├─ 142/143/144 (head.S fixup region) ──► head.S-tail death, pre-C world —
│                                      W-35: a placement overlapping the
│                                      zreladdr inflation region; check the
│                                      guard (bc[18] ≥ 0xa1200000)
├─ 121/122 (MMU-on pair)             ──► enable_mmu entered, turn_mmu_on
│                                      never — W-29-era; pre-C-world death
├─ 150-166 (iotable/svm region)      ──► the svm memset wall: bc[19] =
│                                      pmd for the svm VA — a section desc
│                                      = healthy mapping; a TABLE pointer
│                                      (0xbfc1141e) = the stale pgd pair
│                                      (KNOWN_ISSUES #4b, build #110 shaves
│                                      the allocator limit by 2MB)
├─ 133/135/134 (setup.c pairs)       ──► setup_arch/early_paging_init era —
│                                      discriminate from mmu.c's 133/134
│                                      via the mirror channel (rule 17)
├─ pv_off=0 in the console           ──► should be IMPOSSIBLE now (the
│                                      W-32c direct-store block, marker 163,
│                                      tries in bc[19]) — if seen, the store
│                                      fix failed: investigate immediately
├─ climbs past 171 → initcall breadcrumbs (bc[6]/bc[7])
│                                    ──► progress! bc[7] = the hanging driver's
│                                      fn pointer (resolve via System.map)
├─ l2x0_of_init wedge (init_IRQ)     ──► the by-way 0x7FC deadlock
│                                      (newdocs/KNOWN_ISSUES #3)
├─ 50 (payload era)                  ──► payload crash — SSH back in, read bc,
│                                      fix, re-run (no reboot cost)
├─ payload markers < 50              ──► setup-phase problem (eMMC/sync stalls
│                                      seen — retry once)
├─ bc[1] = 0xAB                      ──► real post-M abort fired — read the ring
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
