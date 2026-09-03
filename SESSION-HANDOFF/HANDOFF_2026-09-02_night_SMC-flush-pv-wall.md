# SESSION-HANDOFF — 2026-09-02 night (session 6): console SOLVED, corruption root-caused to secure-domain config

Read docs/03_DEBUGGING_SESSIONS.md section "2026-09-02 Evening" first — it has
the full run map. This file = the state, the mental model, and the task list.

## THE BREAKTHROUGH (vs the previous handoff)

1. **THE CONSOLE WORKS.** ring1 (PA 0x88000080 count, +0x100 chars) holds the
   full boot log, readable after the WDT2 reset. memdump3 prints words
   BIG-ENDIAN-formatted: reverse each 4-byte group when decoding. Ring
   windows are 3840 chars (base+0x80..base+0xF80). PB-PANIC notifier works.
   bc[1] = the last flushed marker; bc[2] = v|0x100 pair.
2. **L2-off is DEAD as a mode.** The payload --l2test A/B proved it: device
   stores with L2 ON = clean, the same stores with L2 OFF = machine wedge.
   The old "171 wall" (and the session's wild writes/corruption) = this.
3. **L2-on boots work until ~paging_init** (bc=127, map_kernel done) with
   1100-1200 chars of clean console. The wall = a corrupted
   dma_mmu_remap[0].base (0xbe800000 -> 0xa1000000) -> BUG mapping for
   0xa1000000 at 0x1f7f0000 in user region -> (if the barrier mapping had
   been live) omap4_mb abort -> panic. DETERMINISTIC per placement.
4. **The corruption root = secure-domain config we cannot touch from NS:**
   - PL310 data latency = 0x111 (1/1/1 cycles!) confirmed live by --l2lat
     (0x108=0, 0x10C=0x111 read fine; the NS write = SIGBUS — secure-filtered).
   - L3/EMIF auto-idle (PRCM) = secure-filtered too.
   - No monitor service exists for the latencies in the RE'd table
     (0x100/0x101/0x102/0x103/0x104/0x105/0x108/0x109/0x113).
   Both = the "silent corruption" the first sessions chased. The machine
   loses/corrupts DRAM transactions at a low rate under QNX's marginal L2
   config; every "wild write" (bc[2]=0x3E7-era, |0x80 ring bytes, corrupted
   pv-patch sites, corrupted dma_mmu_remap) = this, placement-dependent.

## THE FIX = INSIDE THE SECURE MONITOR (Task 1 of the old handoff, now urgent)

RE the trustzone-omap4 QNX-PACKED module — ALREADY DUMPED AND DISASSEMBLED:
playbook-dev/device-binaries/trustzone-omap4 (61 KB ELF) +
trustzone-omap4.dis (12.5k lines; also libsecure_dispatcher-omap4.so.1 —
its .dis is 0 bytes, re-disassemble it). See docs/04 §Workspace Layout.
Look for:
- a L2X0 tag/data-latency service (0x104 was mapped to "auxcoreboot" — that
  mapping is SUSPECT; verify against the SMC dispatch table);
- the PPA clock-domain service (disable L3/EMIF auto-idle);
- anything that programs 0x104/0x10C (tag/data latency registers).
If a latency service exists: payload sets 3-cycle latencies pre-jump
(mon_call shape, C-flow only) and the L2-on mode becomes clean.

## ALSO NEW THIS SESSION

- Kernel flags: CONFIG_CMDLINE_FORCE=y (the cmdline was placement-flaky);
  CONFIG_CACHE_L2X0=y + a pl310 DTS node @ 0x48242000 (tag/data-latency
  <3 3 3>). WARNING: the l2x0 driver writes the latency regs via NS writel
  = SIGBUS on this machine — the boot dies at l2x0_of_init (init_IRQ) once
  it gets past the current wall. Patch cache-l2x0.c to skip the latency
  writes (route via write_sec default: WARN+skip) OR keep L2X0 off until
  the monitor latency service is found.
- head.S: the C/B/S strip is REMOVED (kernel RAM = cacheable — the UNCACHED
  kernel was the 120-wall and the 171-wall). Diagnostic sections
  0xFEB/0xFEC/0xFED/vectors = DEVICE (r6); the ring/bc sections 0xC80/0xD00/
  0xD40 = CACHEABLE (r7). ACTLR.SMP is left at 1 (the clear caused wedges).
- omap4bc.S: senduart and the LSR drain are REMOVED (UART3 dead post-idle —
  a posted store to the dead THR stalls the store buffer; a dead-target read
  stalls forever). Rings-only console. The monitor's UART3 capture is
  therefore INACTIVE; do not trust ring3 counts.
- All flushes = pb_smc_flush = monitor SMC 0x101 (C-flow shape, r12=0x101,
  r0=PA, r1=size), synchronous, NO sync polls. pb_bc_put = store + guarded
  SMC flush. pbmarkv (head.S) = DCCMVAC+CIPA+BOUNDED poll (no SMC from asm).
  Boot-wide audit: ZERO unbounded polls remain (the setup_arch LED-off and
  LED-on blocks were the last; both now pb_bc_put).
- adjust_lowmem_bounds: vmalloc_limit computed vs PHYS_OFFSET leaves
  lowmem_limit=0 on sane runs too (0x30000000-era) — arm_memblock_init now
  forces current_limit = memblock_end_of_DRAM() and prints PB-MEM lines.
  The barrier (omap_barriers_init) is DISABLED (#if 0) — its stack struct
  was corrupted (0xfe600000 -> 0x1f7f0000) and the unmapped dram_sync write
  aborted. dram_sync_paddr steal still runs (0xa1000000).
- The payload: --l2lat (latency read/attempt/SIGBUS probe — the proof above),
  --l2test (the A/B cliff proof), --l2on (skip the 0x102 disable; bc[10] =
  the PL310 CTRL readback = 1 proves the L2 state). --l2test phase C wedges
  the box on purpose (WDT2 recovers).
- Display confound: "display quiesced" does NOT blank the screen (DISPC
  scanout continues = DRAM contention through the whole boot). Confirm the
  screen state at quiesce on every run; the user can tap to keep it awake.
- memdump3 output = word-reversed; ring decode: swap each 4-byte group.

## CURRENT WALL (bc=127, 100% reproducible x2)

dma_contiguous_remap maps dma_mmu_remap[0] whose base is corrupted
(0xbe800000 -> 0xa1000000) -> create_mapping BUG (skipped) -> the CMA area
left unmapped -> later aborts. The corruption = the secure-domain issue
above. Do NOT chase the mapping code; chase the monitor RE.

## NEXT SESSION TASKS (in order)

1. RE trustzone-omap4 (the monitor) for a latency/PPA service. If found:
   payload pre-jump: fix latencies (0x333) and/or disable auto-idle, then
   the --l2on mode should boot CLEAN past paging_init.
2. Keep CONFIG_L2X0=y but guard cache-l2x0.c against the NS latency SIGBUS
   (skip the latency writes; the write_sec default path already warns).
3. Then: the normal boot continues into initcalls with a WORKING console.
   Expect walls at the eMMC/ADMA, the WiFi SDIO, the PRCM-dependent drivers.
4. Update docs/03 + this handoff after EVERY run (session 6 neglected this
   for the middle stretch — the docs caught up only at the end).

## OPERATIONAL RULES (unchanged + new)

- Ask the user before every device run. The user can hard-reboot (power
  hold) to wipe DRAM. The user can video-record runs and extract LED
  timings — ASK for timings on every run.
- Check the bc[15] nonce on every readback. Decode memdump3 word-reversed.
- The ring1 log = the primary readback; the bc ladder = the marker channel;
  PB-PANIC = the panic text (rings).
- Never: enter_stub-inline SMCs, NS PRCM writes, NS PL310 CTRL/AUX/latency
  writes, unbounded polls, UART3 writes from the kernel.
