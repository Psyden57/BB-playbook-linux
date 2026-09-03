# Session 7 Notes (2026-09-03)

Session focus: the monitor RE task from the session-6 handoff, then the
bc=127 wall, then repo preparation. A lot of this session's knowledge exists
only in the conversation — captured here with epistemic labels.

## CONFIRMED (verified against code/hardware this session)

1. **The QNX binaries do not contain the SMC dispatch.** Full static RE:
   - `trustzone-omap4` (61 KB) = the `/dev/trustzone` resource manager — a
     secure-CRYPTO resmgr (ECDH/secp521r1/SHA512/RPMB/KEK strings). ONE SMC
     site (0x7c4c, the r6=0xFF/ip=0 shape), maps PRCM (wkupCtrlVirtBase),
     but NO L2/PL310 code, no dispatch table.
   - `libsecure_dispatcher-omap4.so.1` = crypto/KDS (aes_oneshot,
     hmac_oneshot, rng_hwRNGen, kds_*); talks via MsgSendv/devctl; ZERO
     SMCs, zero coprocessor ops. Re-disassembled with capstone (the box has
     no ARM objdump; QNX ELFs read as "architecture UNKNOWN").
   - `procnto` (143k lines): ZERO SMCs, ZERO PL310 accesses (no 0x48242000
     constants). The three `mov ip, #0x104` hits are stack-buffer data.
   - `devpm-omap4` = the only NS SMC client: 3× SCU_PWR 0x108 suspend calls
     (exact mainline sleep44xx.S shape) + 2× PPA calls 0x26/0x27 (NOT in
     mainline's PPA list; shape r0=idx r1=0 r2=4 r3=pargs r6=0xFF r12=0).
   → The monitor is the TI ROM monitor; the documented 0x100-0x113 table is
   complete; **no L2 tag/data-latency SMC service exists.**

2. **PPA probe run PPA-1** (payload `--ppa`, first on-device use of the new
   `ppa_call` stub): 0x25 → 0xFF02 (rejected), 0x26 → 0, 0x27 → 0,
   0x23 L2_POR → 0xFF02 (rejected). PL310 readbacks identical before/after
   (ctrl=1, aux=1e070000, tag=0, data=0x111, prefetch=0). No LED activity
   (probe mode, no jump). **The secure-side L2 fix path is exhausted.**

3. **The bc=127 wall root cause** (all facts grepped, not recalled):
   - Runtime PHYS_OFFSET = load address (head.S, ARM_PATCH_PHYS_VIRT); DTS
     bank = 0x80000000+1GB ⇒ 576 MB of memblock below PHYS_OFFSET.
   - Bottom-up allocs (dram_sync steal, CMA) landed there (0xa1000000);
     `__phys_to_virt(0xa1000000)` < TASK_SIZE (0xBF000000) ⇒
     `BUG: not creating mapping ... in user region` at bc=127. Deterministic.
   - The observed VA 0x1f7f0000 additionally equals
     `0xa1000000 - 0x81810000` where 0x81810000 = the UNPATCHED pv-stub
     placeholder (`__PV_BITS_31_24` 0x81000000 + `__PV_BITS_23_16`
     0x810000) — i.e. an unpatched pv site existed in that run (mechanism
     unknown; phys2virt.S is byte-identical to mainline v6.15 — diffed).
   - Fix implemented (payload v4 sweep + kern_off + fdt_patch_memory +
     PB-CMA print + DTS bank) and **W-4 booted past 127 to bc=171**.

4. **QNX's free pool has no 2 MB-aligned 24 MB run** (W-3 sweep: 129/129
   honored hints reported frag<24MB; 12 generic blocks, all unaligned).
   Hence kern_off (kernel at first 2 MB-aligned offset inside the buffer).

5. **The DTB is lost on the uncompressed-Image path** — W-4 (with probe)
   and W-5 (--t3, no probe/loop) both printed "Neither atags nor dtb found"
   ×2 and used the 16 MB fallback. Address math closed EXACTLY in W-4
   (dtb_phys = 0xa19378b8 = the probe's bc[4] echo; Image 0x112F8B1 →
   padded 0x112F8B8); probe validated the magic (bc[6]=0xedfe0dd0). Loss is
   between the cont's r2 load and the kernel's `__vet_atags`. The 1 GB
   probe loop is EXONERATED (W-5). **Unresolved — parked for the zImage.**

6. **The zImage path is natively correct**: `CONFIG_ARM_APPENDED_DTB=y` +
   `CONFIG_AUTO_ZRELADDR=y` (grepped in .config). Session 6's recovered log
   (kernel #51, zImage) shows "Machine model: BlackBerry PlayBook" +
   "Ignoring memory range 0x80000000 - 0xa0000000" — the fdt trim to
   PHYS_OFFSET is native; the 1 GB DTS bank was never the problem on the
   zImage path. (The W-series bank patch is belt-and-braces, harmless.)

7. **The kernel .config is recoverable from any built Image**
   (`scripts/extract-ikconfig`, CONFIG_IKCONFIG=y) — used this session to
   restore a config mangled by a bare `make` syncconfig (which dropped
   DEBUG_LL/DEBUG_PLAYBOOK_BC/ARCH_OMAP2PLUS). Rule: never bare-`make` the
   tree without CROSS_COMPILE.

8. **WDT2 lifecycle implemented** (from the kernel's omap_wdt.c, grepped):
   enable = SPR(0x48) 0xBBBB then 0x4444 (WPS(0x34) bit 0x10 poll);
   disable = 0xAAAA then 0x5555. Kick = TGR(0x30) complement (QNX wdtkick
   convention). Aborts disarm; kicks re-enable first (a TGR write on a
   disabled WDT is a no-op).

9. **eMMC secure-boot chain dumped and carved** (user-area sectors 5-240;
   `dd if=/dev/hd0` on-device): mini-FAT MBR (32-byte records: size @+0,
   offset @+4, name @+0x14) → KEYS (0x200, TI certs), PRIMAPP (0xc00,
   3 KB Thumb PPA app, ~112 instructions then cert data), MLO (0x4000,
   signed-header second stage), and an 84 KB ARM image (eMMC 0x9000-0x1e000,
   vector table at +0) = the QNX initial loader with NS-side SMC wrappers
   `push; movw ip, #SVC; smc #1; pop` for **0x102, 0x109, 0x100 and 0x112
   (twice)** + the r6=0xFF PPA shape at +0x94. **0x112 is undocumented** —
   semantics unknown, callers not found by ARM/Thumb BL scan (likely called
   via literals from another stage). Carved blobs live in
   `device-binaries/bootblob_*.bin` (local-only).

10. **DRAM = two 512 MB banks** (user-provided cfp flashinfo): 0x80000000-
    9FFFFFFF and 0xA0000000-BFFFFFFF (Elpida, 64 MB ranks). The payload's
    `p >= 0xa0000000` constraint keeps the kernel in one bank. Also:
    Bootrom 5.27.0.20, IRAM base 0x40304000, OS 1.0.7.2670 DEV, Bootrom
    copy at MCT blocks 5-12.

11. **cache-l2x0.c audit**: the "guard the driver against NS latency
    SIGBUS" warning is a NO-OP (latency writes route through
    `omap4_l2c310_write_sec` → default WARN+skip). The REAL hazards when
    the boot reaches `l2x0_of_init`: `l2c_enable`'s by-way write to
    L2X0_INV_WAY (0x7FC = the on-device deadlock op) + `l2c_wait_mask`
    poll. (Amended in docs/01/05/07/08.)

## REASONABLE INFERENCES (not proven)

- The 171 wall (taskstats/kmem_cache_create) is probably *not* the old
  "slab_mutex corruption" — both 171-era deaths so far ran on a 16 MB
  no-DTB fallback. A zImage run with full memory is the fair test.
- The `wdt2 kicked (wtgr=0x63bd)` W-2 surprise reboot = the payload's own
  WDT2 arm + abort path (W-2 predates the disarm-on-abort). Consistent with
  timings; not reproduced since.
- The console silencing after "PB-ADJ" (ring stops, bc continues) is likely
  a printascii/earlycon reconfig issue post-memblock, not a hang.

## UNKNOWNS / UNRESOLVED

- WHERE exactly the r2 value dies on the Image path (cont → head.S → vet).
- The pv-fixup unpatched-site mechanism (once is not a pattern).
- What SMC 0x112 does; who calls the loader's SMC stubs (no BL callers
  found in-blob).
- Why the ring3 monitor UART capture never activates (documented dead end
  from session 6 — posted-store stall theory).
- Whether QNX's L2 configuration (data latency 0x111 = 1/1/1 cycles,
  PL310 reset default) actually contributes any instability once the boot
  is fixed — the corruption theory is unproven either way.

## SESSION-SPECIFIC OPERATIONAL FACTS

- W-1: pinned placement refused (0xa4000000 busy) — correct abort behavior.
- W-2: even after clean reboot all fallbacks failed → built the sweep.
- W-3: sweep counters (frag=129, unaligned=12) → kern_off design.
- W-4: first successful jump of the fixed kernel; LED sequence observed
  (blue on during copy, off during kernel, red at reset; exact timings
  pending from the user's video). bc=171.
- W-5: --t3; bc=126; ring1 count 0x44a; kern_phys 0xa0400000 (varies run
  to run — kern_off working as designed).
- PPA-1: payload-only probe; no reboot by design; QNX alive throughout.

## CONTRADICTIONS with older docs

See `newdocs/contradictions/` (3 files). Most important: the machine-
corruption theory (session 6) vs the code-bug explanations (session 7) —
not fully resolved; the DTB loss keeps a lost-write hypothesis alive.
