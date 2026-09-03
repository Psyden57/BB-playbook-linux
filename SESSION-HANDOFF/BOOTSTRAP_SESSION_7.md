You are continuing a QNX→Linux kexec project on a BlackBerry PlayBook (TI
OMAP4430 "winchester", HS device, QNX 6.6, root via SSH root@169.254.0.1,
key at playbook-dev/rsa). This is session 7. Sessions 1-6 are documented;
read IN THIS ORDER before touching anything:

1. viking://resources/playbook-dev/SESSION-HANDOFF/HANDOFF_2026-09-02_night_SMC-flush-pv-wall.md
   (session 6's handoff — the console breakthrough, the corruption root
   cause, and the task list)
2. playbook-dev/docs/05_NEXT_STEPS.md (the current task list — rewritten)
3. playbook-dev/docs/03_DEBUGGING_SESSIONS.md, sections "2026-09-02 Evening"
   and "Late-night addendum" (the session 6 run map), then the NIGHT UPDATE
   sections at the top of 01_KEXEC_ARCHITECTURE.md, 06_HARDWARE_REFERENCE.md,
   07_TROUBLESHOOTING.md and 08_KERNEL_DEBUGGING.md — they SUPERSEDE older
   text in those files wherever they conflict.

STATE SUMMARY (session 6 outcome):

- **The console WORKS.** The boot log lands in the CACHEABLE DRAM rings
  (ring1 = 0x88000080 count / +0x80+idx chars, 3840-char window) and
  survives the WDT2 reset via batched monitor-SMC 0x101 flushes. PB-PANIC
  prints panic text into the rings. memdump3 prints words BIG-ENDIAN:
  reverse each 4-byte group when decoding. The monitor's UART3 capture
  (ring3) is INACTIVE — the kernel never writes UART3 (it dies post-idle;
  a posted store to the dead THR stalls the store buffer).
- **The L2-off mode is RETIRED** (--l2test A/B: the bypass path wedges under
  sustained traffic; the old "171 wall" = that cliff + the head.S C/B/S
  strip that left the kernel UNCACHED — both fixed). The mode = --l2on
  (PAYLOAD_MODE=--l2on ./jump.sh zImage; the payload skips the 0x102
  disable; bc[10] = the PL310 CTRL readback = 1 proves the L2 state).
- **The current wall**: --l2on boots reach bc=127 (paging_init:
  map_kernel done) with ~1100 chars of clean console, then die
  deterministically at dma_contiguous_remap — `dma_mmu_remap[0].base`
  corrupted (0xbe800000 → 0xa1000000) → `BUG: not creating mapping for
  0xa1000000 at 0x1f7f0000 in user region` → the CMA unmapped → abort.
  Reproduced in both L2 modes.
- **The root cause (pinned)**: the machine corrupts/loses DRAM transactions
  under QNX's secure-domain L2 config. PL310 data latency = 0x111 (1/1/1
  cycles) confirmed live (--l2lat: 0x10C reads 0x111; the NS write =
  SIGBUS — secure-filtered). L3/EMIF auto-idle = also secure-only. NO
  monitor service for the latencies exists in the RE'd API
  (0x100/0x101/0x102/0x103/0x104/0x105/0x108/0x109/0x113). Every wild
  write/|0x80 byte/corrupted pv-patch site this session = this, and it is
  placement-dependent.
- **Kernel flags**: CONFIG_CMDLINE_FORCE=y (cmdline was placement-flaky);
  CONFIG_CACHE_L2X0=y + a pl310 DTS node @ 0x48242000 (latencies <3 3 3>).
  WARNING: the driver's NS latency write = SIGBUS — guard cache-l2x0.c
  (skip the latency writes) before the boot reaches l2x0_of_init.
- **Kernel-side state**: kernel RAM cacheable (the C/B/S strip removed);
  0xFEB/0xFEC/0xFED/vectors DEVICE (r6); ACTLR.SMP left at 1; all flushes =
  pb_smc_flush (SMC 0x101, C-flow, synchronous); ZERO unbounded polls
  boot-wide; the omap4 dram barrier DISABLED (a corrupted stack struct
  aborted); the memblock limit forced (adjust computed 0); PB-ADJ/PB-MEM
  diagnostics in the rings; PB-PANIC notifier works.
- **Payload flags**: --l2on (keep the L2), --l2lat (the latency probe: the
  proof the NS write SIGBUSes), --l2test (the A/B cliff proof — phase C
  WEDGES the box on purpose; WDT2 recovers). jump.sh: PAYLOAD_MODE env
  selects the mode (default --probe).

FIRST TASK (from the handoff): **RE the trustzone-omap4 monitor module** —
the fix for everything is inside the secure monitor:
1. The module is ALREADY DUMPED AND DISASSEMBLED:
   `playbook-dev/device-binaries/trustzone-omap4` (61 KB ELF) +
   `trustzone-omap4.dis` (12.5k lines). No live-system dump needed. Also on
   disk: `libsecure_dispatcher-omap4.so.1` (+ a 0-byte .dis — re-disassemble
   it), `procnto.dis`, and the other QNX binaries. See the Workspace Layout
   table in docs/04_KEY_FILES_AND_COMMANDS.md.
2. Locate the SMC dispatch table in the .dis (search for the known service
   numbers as immediates: 0x100/0x101/0x102/0x109/0x113 — the CMP/BEQ
   chains — and their handlers).
3. Hunt for: a L2X0 tag/data-latency service (the old "0x104 = auxcoreboot"
   mapping is SUSPECT — verify it against the dispatch chain), and the PPA
   clock-domain service (disable L3/EMIF auto-idle).
4. If a latency service exists: payload pre-jump, mon_call it with 3-cycle
   values (tag 0x108 / data 0x10C = 0x333-style), verify by readback, then
   a --l2on jump should boot CLEAN past paging_init.

RULES (unchanged + new):
- Ask the user before every device run. The user can hard-reboot (power
  hold) to wipe DRAM, and video-records runs — ASK for LED timings on every
  run (blue-on duration, blue-off → red gap).
- Check the bc[15] nonce on every readback. Decode memdump3 word-reversed.
- The ring1 log = the primary readback; the bc ladder = the marker channel.
- Never: enter_stub-inline SMCs, NS PRCM writes, NS PL310 CTRL/AUX/latency
  writes, unbounded polls, UART3 writes from the kernel.
- Do not re-test dead ends (the L2-off mode, the UART pad hunt, the slab-
  mutex theories — all documented as solved/dead in the docs).
- The user prefers the Edit tool over python heredocs for file edits.
- Update docs/03 + the handoff after every run.
