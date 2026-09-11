# BOOTSTRAP SESSION 11 (written 2026-09-11, session 10's wrap-up product)

## THE ONE-PARAGRAPH STATE

The front has moved from "jumping into the kernel" to "the kernel's own
early path". All prior walls are behind us and the boot now reliably
reaches bc[1]=145 (the CMA remap's pmd_clear-done marker in
dma_contiguous_remap) — and dies at the very next op, the TLBIALL,
because its trailing `dsb` is the full-system (SY) domain, which waits
for the HELD CPU1. The unified diagnosis of session 10: **every
barrier in the full-system/inner-shareable domain (dsb sy/ish, and
likely some cp13/broadcast ops) wedges against the held CPU1 — flaky,
not deterministic.** The fix in flight: `dsb nosh` (the CPU-local
barrier) on the boot-critical paths; tlb-v7.S is patched, the CMA
flush is done locally (TLBIALL + dsb nosh + isb), and ONE more `dsb
sy` family remains in the marker macros (pbmark/pbmarkv/pbmark3 and
main.c's pb_bc_put — see the bequest). The kernel to run first =
**#139** (kexec/kernel/zImage, packed 5,485,048 B zImage + DTB).

## THE SESSION-10 ARC (W-39 → W-68, ~20 recorded runs, 19 kernel builds)

1. W-39 (#110, --dmaquiet): the front moved to the svm memset through
   the "middle pair" — pgd-healthy yet store-wedged; the stale-view
   class re-characterized as a per-line L2 discrepancy.
2. The approach audit (Psyden's three questions) →
   `newdocs/audit-approach-2026-09-11.md`. HEADLINE FINDINGS:
   - **NS PL310 0x768 (the project-wide "CLEAN_INV_LINE_PA" flush) is
     NOT a real op register — a no-op at best.** The real map:
     INV_LINE_PA=0x770, CLEAN_LINE_PA=0x7B0, CLEAN_INV_LINE_PA=0x7F0.
   - **NS 0x770 invalidate-by-PA, NO clean: PROVEN on-device**
     (kexec/l2canary.c: the dirty L2 line discarded, the DRAM canary
     0xC0FFEE11 served). 0x7F0 CIPA also works from NS.
   - **TRM §27.5 Table 27-61: R12=0x112 = write PL310 tag+data RAM
     latencies** — session-7's "no latency service" RE claim was
     WRONG; on-device 0x112 ABORTS NS callers (SIGBUS fltno=5), so the
     latency path is closed with direct evidence.
   - **The boot ROM dumped + RE'd** (0x40028000, 48 KB + the lower
     block 0x40020000-0x40026F24 with a 4 KB ROM_HIDE hole = the
     dispatch-region candidate; its own SMC sites = ip=0x103/0x107/0xF0;
     zero direct PL310 access). Fresh device dumps verified the old
     corpus byte-identical (`bootdumps-2026-09-11/README.md`).
3. The payload/boot redesign (the W-4x chain):
   - **The L2-off now runs INSIDE the proven --dmaquiet shape**
     (the payload's mon_call(0x102) — the W-46 fix; the old --t3
     shape = the old direct-jump = the "invalid dtb" error).
   - **The buffer = 12 MB on the no-probe path** (the fresh-boot pool
     has NO 24 MB contiguous run; the placement ladder 12→8→6 MB with
     2 s settles = the W-61 fix).
   - **The image-region CIPA sweep at bc 47 works on-device.**
4. **The kernel's own delivery chain — where the session's decode went:**
   - r2 = the DTB ARRIVES at stext correctly (bc[24] = 0xa1942b88,
     bc[25] = 0xffffffff). The decompressor's appended-DTB search
     WORKS (bc[20] = _edata, bc[21] = the magic 0xedfe0dd0 — the W-54
     markers, MMU-off SO stores = DRAM truth).
   - **__atags_pointer was being LOST**: the .init.data store sits
     L1-dirty and dies at the WDT2 reset (the L2 off = nothing catches
     it; survival = per-slot eviction luck). DCCMVAC-on-a-dirty-
     cacheable-line with the L2 bypassed HANGS (the W-58/W-64 lesson:
     the same op on the uncached bcs = harmless).
   - **The cure = NO cache ops: setup.c recovers the FDT pointer from
     bc[20] (the decompressor's DRAM-truth dump) when __atags_pointer
     == 0 — VERIFIED WORKING (W-66: bc[12] = the DTB, the ring = the
     FULL healthy boot log through the CMA!).**
   - **set_current (the mcr c13,c0,3 TPIDRURO write) = another wedge
     site — bypassed with the plain str to __current (KEPT).**
   - **The pb_bc_put fix (the W-50): with the L2 off the old "plain
     store = DRAM-truth" assumption was wrong — the store = L1-dirty
     and dies at the reset; the markers' losses = per-slot L1-eviction
     luck. pb_bc_put now ALWAYS DCCMVACs the line.**
5. **THE UNIFIED BARRIER DIAGNOSIS** (the W-43..W-68 ladder): every
   full-system/inner-shareable barrier on the boot path (dsb sy, dsb
   ish, the IS TLB variants) = a wait for the HELD CPU1 = the flaky
   wedges. Fixes applied so far: `dsb nosh` in tlb-v7.S (the entry +
   post-loop), the non-ISH c8,c7 TLB encodings, the CMA flush local
   (TLBIALL + dsb nosh + isb at c0f07e3c-44). **REMAINING: the
   pbmark/pbmarkv/pbmark3 macros' `dsb`/`dsb sy` sites (head.S/
   head-common.S) and main.c's pb_bc_put `dsb(sy)` — they pass by
   CPU1-ack luck and must be converted to nosh.**

## THE MANDATORY READ ORDER (before any work)

1. `newdocs/HANDOFF.md`, `DEVELOPMENT.md`, `SETUP.md` (the standing rules)
2. `newdocs/PROJECT_STATE.md` (the front + the session-10 update)
3. `newdocs/ARCHITECTURE.md` + `COMMANDS.md` (the flow + the ops)
4. `docs/README.md` (rules 1-17; rule 17 = the pb_bc mirror-channel
   discrimination) + `docs/03_DEBUGGING_SESSIONS.md` (the run log)
5. `newdocs/audit-approach-2026-09-11.md` (the session-10 audit)
6. `bootdumps-2026-09-11/README.md` + `BOOTROM-RE.md` (the dumps + RE)
7. `newdocs/KNOWN_ISSUES.md` + the contradictions ledger
8. `PLAYBOOK-REFERENCE.md` §5/§8/§10 (the safety model)

## DEVICE FACTS (verified this session)

- QNX 6.6 device at 169.254.0.1 via SSH, key `playbook-dev/rsa`,
  options: `-o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa
  -o PubkeyAcceptedKeyTypes=+ssh-rsa` (hmac-sha1 was dropped).
- Boot 2-3 min; /tmp wiped per reboot (jump.sh redeploys everything).
- WDT2 window: the late 15 s kick pre-jump; the WDT2 fires ~59 s from
  the last kick. Payload SIGSEGVs do NOT reboot; the freeze = the MMCHS
  region only (0x40034000-class reads = harmless SIGBUS, box alive).
- The device clock = unreliable (the RTC was wiped; the ls shows Nov 16).
- The user does battery-pulls between runs (the DRAM = clean; the
  readbacks = provably fresh). LED timings on request.
- The readbacks: `memdump3 90000000 0x40` (bc[0..15]) + the session-10
  forensics = `memdump3 90000040 0x30` (bc[16..27]) — see the map below.

## THE SESSION-10 FORENSICS MAP (the bc slots)

- bc[0]=magic bc[1]=bc[2]=pb second channel; bc[3]=the ACTLR
  post-clear (expect 0x1); bc[4]/bc[5]=the head.S inline markers
  (143/144); bc[9]=0x111 (the PL310 dlat readback); bc[10..12]=the
  vmalloc/svm slots; bc[13]=0xa0008xxx-class = the svm_pa residue
  (non-issue); bc[15]=nonce.
- **bc[16]=DISPC kill readback1, bc[17]=readback2, bc[18]=the buffer
  phys, bc[19]=the pv-block tries|tmo<<8 (0 = verified), bc[20..23]=
  the decompressor's appended-DTB check (r6/_edata, the magic
  0xedfe0dd0, the incoming r8, r4), bc[24]=r2 at stext (the DTB!),
  bc[25]=r1 at stext, bc[26]=the r2-arg at __mmap_switched,
  bc[26]=0xBEEF0000|mism<<8|tmo = the sweep's self-verify tally
  (0xBEEF0000 = completed, zero mismatches), bc[27]=the LAST phase:
  the flush's entry (1) / post-loop (2) / post-isb (3) — or the
  setup.c span (F4000001-3 / F5000001-2 / F6000001-3).**

## THE FIRST TASK (W-69): run #139 as-is

`PAYLOAD_MODE=--dmaquiet ./jump.sh zImage`, hands-off, unfiltered
jump.sh output (never `| tail`), record video if convenient. Read
bc[16..27] (`memdump3 90000040 0x30`) and decode with a python script.

Expected: the boot = PAST the CMA flush (the #139 = the local
TLBIALL+nosh) → the death lands in the CMA iotable_init's svm memset
(the old 167 wall) OR proceeds further. Then: convert the remaining
`dsb sy`/ISH sites (the marker macros + pb_bc_put) to `dsb nosh` —
ONE run per conversion, the bc[27] ladder names each.

## THE BEQUEST ORDER (after the boot stabilizes)

1. The barrier sweep: the pbmark/pbmarkv/pbmark3 + pb_bc_put =
   `dsb nosh` (the deterministic boots!).
2. Once the boot = through initcalls: retire the per-victim ladders
   (the W-32c block's CIPA, the 2MB shave) IF the boot passes cleanly
   (keep them if the boot = marginal).
3. The rootfs era: the DMA coherence (the outer-cache callbacks with
   the 0x7F0/0x770 line-ops), the SMP bring-up (the second-core = the
   same broadcast/hold wedge class — the next big investigation),
   the console (earlycon → the real driver).
4. The RE queue: the bootrom's CH* parsers, the ip=0xF0 service, the
   registry blob (id 0x18), the 0x112-latency = aborts-NS (closed).

## THE STANDING RULES (the short list — full: docs/README.md)

- ASK THE USER before every device run; hands-off runs; unfiltered
  jump.sh output; record video if convenient, ask for LED timings
  when they matter; the device clock = don't trust.
- python for hex decode, NEVER mental arithmetic. grep, don't recall.
- Shipped-binary verification (rule 16) before any run: objdump the
  changed code in the built blob.
- NVRAM and RPMB (hd6) untouchable (the brick hazards).
- git commit + push every state change; the bootstrap = the wrap-up
  product (added to OpenViking too).
