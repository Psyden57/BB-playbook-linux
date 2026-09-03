# Architecture

## The transition, end to end

```
 QNX 6.6 (Tablet OS 2.0) userland                 bare metal
┌──────────────────────────────────────┐          ┌────────────────────────────┐
│ qnx2linux (root, System mode)        │          │ IRAM stub (0x40304000)     │
│ 1. allocate 24MB contiguous phys     │          │  flat page table (1MB secs)│
│ 2. copy kernel+DTB into it           │          │  continuation code         │
│ 3. build flat table (tt[] in IRAM)   │  blx     │  probe.bin (optional)      │
│ 4. copy trampoline (position-indep)  │ ───────► │                            │
│ 5. arm WDT2, hold CPU1 in warm reset │          │ trampoline (runs from      │
│ 6. clean+inval L1, GICD CTLR = 0     │          │  payload text VA)          │
│ 7. enter_stub: TTBR0=flat, switch    │          │   └─► continuation (IRAM,  │
│ 8. optional monitor SMC (L2 on/off)  │          │       VA==PA): MMU off,    │
│ 9. last cached act: nothing          │          │       r0=0 r1=~0 r2=DTB    │
└──────────────────────────────────────┘          │           └─► stext        │
        WDT2 (~59s) fires ─► warm reset ─► QNX    └────────────────────────────┘
        breadcrumbs + console ring survive                 Linux 6.15.11
        (read back over SSH after reboot)
```

## Components

### kexec/qnx2linux.c — the payload (QNX userland, ~1450 lines)

The main program. Modes (PAYLOAD_MODE env or argv[1], selected in jump.sh):

| Mode | Purpose |
|------|---------|
| `--probe` (default) | T2-era test: jump to probe.bin in IRAM |
| `--t3` | jump directly into the kernel (no probe) |
| `--l2on` | keep the PL310 L2 enabled through the jump (current default for boots) |
| `--l2lat` | PL310 latency probe (proved NS latency writes SIGBUS) |
| `--l2test` | retired: L2-off A/B cliff test (phase C wedges on purpose) |
| `--ppa` | secure PPA service probe (0x25/0x26/0x27/0x23 with PL310 readbacks) |

Key subsystems inside:
- **Placement search** (2026-09-03 v4): 129-slot 2 MB-aligned sweep of the
  upper DRAM bank with per-reason failure counters, then a generic loop;
  kernel placed at `kern_off` (first 2 MB-aligned offset inside the buffer,
  because QNX's free pool has no aligned 24 MB run).
- **fdt_patch_memory()**: minimal FDT walker that rewrites the /memory
  node's `reg` to match the actual placement (bank = [kern_phys, 0xc0000000)).
- **params block** (IRAM 0x40309800): `[0]` kernel entry, `[1]` DTB phys
  (read by both the probe and the stub continuation).
- **jump buffer verification**: memcpy + memcmp through a NOCACHE mapping
  before the jump; REVERIFY pass right before entering the no-I/O zone.
- **WDT2 lifecycle**: proper enable (SPR 0xBBBB/0x4444) + kick (TGR
  complement) before the jump; disable (SPR 0xAAAA/0x5555) on abort paths.

### kexec/stub3.S — trampoline + continuation

- `tramp_pos_start` (position-independent, runs from the payload's text):
  TTBCR=0, DACR=all, TTBR0=flat table, breadcrumbs 63/64, `bx` to the
  continuation in IRAM. **Why TTBCR=0 is load-bearing** (session-1
  discovery, rationale not previously written down): QNX splits the VA
  space — high VAs walk TTBR1. After switching TTBR0 to the flat table,
  any identity walk whose VA is above QNX's N-boundary would still use the
  QNX kernel tables and abort; TTBCR=0 forces every VA onto the flat table
  (which identity-maps DRAM 0x80000000-0xBFFFFFFF and IRAM). Session-1's
  first trampoline omitted this and died between breadcrumbs 41 and 21.
- `cont_start` (copied to IRAM 0x40308040, VA==PA): breadcrumb 21, records
  the DTB chain value to 0x90000030, SVC mode + scratch stack (0x89000000),
  MMU off, `r0=0, r1=~0, r2=params[1]`, `bx r9` → target.
- NOTE: no TLBIALL here (wedged with CPU1 in reset; redundant).
- Related session-1 rules that constrain any rewrite of this stage (see
  newdocs/session-notes/session-01.md for the incidents behind each):
  the jump buffer must be mapped **PROT_EXEC** (QNX enforces XN on
  data/anon pages — a heap-resident stub SIGSEGVs at its own address);
  anything executed after MMU-off must reach DDR via a **NOCACHE write**
  (cached-written bytes sit in L2 and SO fetches bypass stale lines);
  **no kernel calls and no console I/O** once interrupts are disabled
  (procnto unreachable → deadlock; console dead → printf blocks forever);
  a section-alias table entry `tt[VA>>20] = PA_section` is only exact when
  `(VA & 0xFFFFF) == (PA & 0xFFFFF)` — prefer reaching code by its
  physical address (TLBIMVA + bx).

### kexec/probe.S — bare-metal diagnostic (optional)

Runs at IRAM 0x40309000 before chaining into the kernel: validates the DTB
(magic + totalsize), toggles the FAN5702 LED (GPIO1_13), checks PL310
accessibility, records readbacks into the bc page (bc[4]=r2, bc[6]=DTB
magic, bc[8]=PL310 CTRL, bc[9]=data latency), runs a 1 GB store loop (L2
sanity), then chains into the kernel with the ARM boot convention.

### IRAM layout (0x40304000+)

| Address | Contents |
|---------|----------|
| 0x40304000 | flat page table (`tt[]`, 16 KB) |
| 0x40308040 | continuation (from stub3.S) |
| 0x40309000 | probe.bin (optional) |
| 0x40309800 | params block (kernel entry, DTB phys, kern_phys, DTB size) |

### The debug/observability channels

1. **Breadcrumb ladder** (bc page, DRAM 0x90000000): word 1 = current step;
   strongly-ordered stores (MMU-off PA writes reach DRAM in both L2 states);
   survives the WDT2 warm reset; read back with `memdump3` over SSH.
   Mirrors at 0x88000000/0x94000000/0x9FE00000. bc[15] = per-run nonce.
2. **Console ring** (ring1, DRAM 0x88000100, count at 0x88000080): the
   kernel's DEBUG_LL (`omap4bc.S`) mirrors every earlycon character into
   the ring AND to UART3; flushed via monitor SMC 0x101 batches; survives
   the reset. Decode `memdump3` output word-reversed (it prints big-endian).
3. **PB-PANIC notifier**: kernel panics print into the rings.
4. **LED** (FAN5702, GPIO1_13 EN): blue during payload/kernel start, off
   during kernel run, red at reboot — the user video-records runs for
   timing analysis.
5. **ring3** (0x94000100): the secure monitor's own UART3 capture —
   INACTIVE while the kernel dies post-idle (a posted store to the dead THR
   stalls the store buffer). Historical.

### The kernel side

- Tree: `/home/psyden/kernel/linux` (mainline 6.15.11, non-LPAE omap2plus).
- Patches: `arch/arm/kernel/head.S` (PlayBook prelude: breadcrumb ladder,
  DEBUG_LL smoke test, r1/r2 save/restore, ring-map fixes incl. the M=1
  fix), `arch/arm/include/debug/omap4bc.S` (DEBUG_LL → UART3 + DRAM rings),
  `arch/arm/mach-omap2/omap4-common.c` (L2 write_sec routing), DTS
  `omap4-winchester.dts` (memory bank, pl310 node, reserved-memory nodes),
  `arch/arm/mm/dma-mapping.c` (PB-CMA print).
- Build: `mkkernel.sh` packs Image/zImage + DTB into `kexec/kernel/`.
- Config: session-6 config; recoverable from any built Image via
  `scripts/extract-ikconfig` (CONFIG_IKCONFIG=y). Forced cmdline:
  `console=ttyO2,115200n8 earlyprintk keep_bootcon ignore_loglevel maxcpus=1 root=/dev/mmcblk0p2 rootwait`.

### The secure monitor (TrustZone) surface

SMC #0 (or #1 — the immediate is ignored) with the service in r12; C-flow
`mon_call()` shape works, inline-in-stub SMCs hang. Verified services:

| Service | Function |
|---------|----------|
| 0x100 | L2X0 DBG_CTRL write |
| 0x101 | L2 clean+invalidate by PA (the payload's flush mechanism) |
| 0x102 | L2X0 CTRL write (0/1) |
| 0x108 | SCU_PWR / omap4 suspend (devpm's entry) |
| 0x109 | L2X0 AUXCTRL write |
| 0x113 | L2X0 PREFETCH write |
| 0x100-0x113 | table complete — **no tag/data-latency service exists** |
| PPA 0x26/0x27 | devpm suspend pair (accepted, semantics unknown) |
| PPA 0x25/0x23 | rejected on this monitor (0xFF02) |

NS access rules (hard-won): PL310 CTRL/AUX/latency writes = SIGBUS/abort;
by-way ops (0x7FC) = deadlock; by-PA (0x768) + sync (0x730) = safe; PRCM
writes = secure-filtered (silent SIGSEGV, no reboot).

### Tools (kexec/)

`memdump3` (device-side RAM dump, prints words big-endian — reverse each
4-byte group), `memtest`, `scatter` (RAM survival map across the reset),
`smctest` (interactive SMC caller), `secure-probe`, `sevtap`, `xntest`,
`i2c-hammer`, `uart-hunt`, `waitdev`, `memw32`, `retry-jump.sh`.

## Interaction with the rest of the machine

- **CPU1**: held in warm reset via `RSTCTRL_CPU1` (PRCM 0x4824380C/0x4824340C)
  before the jump; the kernel brings it up itself (SMP).
  - Why CPU1 cannot be redirected instead: on warm reset CPU1 runs a
    **SAR RAM trampoline** (0x4A326B00-CD0, persistent across boots) that
    monitor-verifies its saved context (SMC services 0x26/0x27) before
    resuming — releasing CPU1 with stale state re-enters QNX seamlessly;
    the context cannot be forged. The real AUX_CORE_BOOT registers are
    WUGEN 0x48281800/04 (cold-boot only) — 0x4A002E08 is a wrong address.
  - devpm's offline flow NS-writes the 0x150-byte wake trampoline into SAR
    0x4A326B00 from a plain mmap — the "monitor-protected" blob is ordinary
    memory (session-1 RE, unexploited).
- **Two watchdogs, two failure classes** (the recovery design exploits both):
  - **WDT2** (0x4A314000, ~58.6 s): warm reset — DRAM survives, so bc/ring
    readbacks work. The payload arms and kicks it; a hung kernel = reset =
    back to QNX.
  - **TWL6030 PMIC watchdog** (I2C1, slave 0x48, reg 0x2C, 127 s): full
    **power-off — DRAM content is LOST** (observed live once: the device
    "simply shut down"). This is why the blob holds BOTH cores at the end:
    nobody services WDT2 → warm reset happens well before the 127 s PMIC
    deadline, preserving the run's evidence.
- **QNX's CP15 state at jump time** (payload-measured): ACTLR = 0x41
  (SMP/nAMP + cache/TLB-op broadcast), diagnostic c15,c0,1 = 0x810 (errata
  742230/751472 workarounds already applied by QNX). The kernel's proc-v7
  setup ORs onto this.
- **The device-op cliff** (sizing rule for any device-access loop): with
  the L2 ON (QNX's config), sustained strongly-ordered stores wedge the
  machine at roughly **4-5k operations** (smoke test ~480 ops fine;
  --l2test phase C wedged inside a ~4k-op loop; the old console died at
  ~5k ring ops). Cached stores are exempt (the bss clear = megabytes, fine).
  Distinct from the one-shot by-way deadlock (KNOWN_ISSUES #3).
- **eMMC / WiFi / display**: untouched; QNX's drivers are simply abandoned
  mid-flight. The kernel needs its own drivers (mainline has them all for
  OMAP4). The display scanout remains live DRAM traffic unless quiesced —
  note the display state as a run variable.
