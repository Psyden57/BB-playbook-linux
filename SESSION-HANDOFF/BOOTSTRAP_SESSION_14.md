# BOOTSTRAP SESSION 14 (written 2026-09-12, session 13's wrap-up product)

## THE ONE-PARAGRAPH STATE

Session 13 = the W-90/91 sweep trilogy (4 runs, 4 payload builds, kernel
#155 unchanged throughout) and it NAILED the stale-view mechanism:
**W-90a's DTB smoking gun — the kernel read fdt_totalsize = 15519 (the
old DTB size) from the appended DTB whose real header = 87321, at a PA
that = exactly where W-88's kernel cached-read ITS DTB header — the L2
retains lines ACROSS the WDT2 reset, QNX's reboot, and our NOCACHE
writes; the "lottery" = which fossil lines overlap the current run's
PAs (per-run placement luck). The old bc-47 "image sweep" = 1 line per
4KB = never cleaned anything.** The cure machinery = built and proven:
l2c_ns_line_range(op, pa, size, heartbeat) = per-line 32B PL310 sweeps
with bounded sync polls + heartbeat slots; TWO OPS by region class
(0x7F0 CIPA where DRAM has no fresh pre-jump truth / QNX lives;
0x770 INV-ONLY for the jump buffer where the fossils can be DIRTY and
0x7F0's clean would poison the payload's NOCACHE-verified fresh copy;
the trampoline page = 0x7F0, clean-never-discard). W-90b = my tail
underflow (a non-32-multiple size → an infinite sweep; the heartbeats
pinned it: bc[27]=476782 chunks at the WDT2 expiry) — and proved
**1.95G device stores QNX-side = NO device-op cliff (~33M ops/s)**.
W-91 = the whole-DRAM early sweep ([0x80000000, 0xC0000000), ~35 s —
clean+inv ≈ 1 μs/op) = survived, and the boot reached **bc[1]=126 = the
W-83-era first-post-CMA-TLB-op front, 2/3 valid runs** (W-90a dest-only
= 126 too; W-90c full-sweep = the W-84 triad outlier 142+0x3E7+ring 0).
**THE 15,519 ANOMALY = THE SESSION'S OPEN THREAD: it SURVIVED the
whole-DRAM sweep** — the deployed artifact chain = python-verified
correct end-to-end (the deployed zImage = the host file EXACTLY
(W-90b's bc[2]; my "stale deploy" reading = a hand-hex slip, corrected);
the appended-DTB header = totalsize 87321 at exactly tree_zlen; the
payload's memcmp verified the DRAM copy) — yet setup.c:1209 STILL read
fdt_totalsize = 15519. NOT an L2 fossil. The old 15,519-B DTB copies
from W-84..89 are still in DRAM (the sweep = cache-only!) = the leading
candidate; the decompressor's ATAG-compat path (r8 = the stub's TTBR0 =
nonzero = "an ATAG list around"!) = the second.

## THE MANDATORY READ ORDER (before any work)

1. `newdocs/HANDOFF.md`, `DEVELOPMENT.md`, `SETUP.md` (the standing rules)
2. `newdocs/PROJECT_STATE.md` (rewritten for session 13)
3. `newdocs/session-notes/session-13.md` (the sweep trilogy + the fossil
   mechanism + the 15,519 anomaly = THE session-13 knowledge base)
4. `docs/03_DEBUGGING_SESSIONS.md` (the W-90a/b/c + W-91 records,
   append-only)
5. `docs/README.md` (rules 1-17) + `newdocs/KNOWN_ISSUES.md`
6. `PLAYBOOK-REFERENCE.md` §5/§8/§10 (the safety model)

## DEVICE FACTS (verified this session)

- **The L2 = a cross-run fossil record** (W-90a). The payload's NOCACHE
  maps create no L2 lines; the kernel's cached reads hit previous runs'
  kernel-era lines at the same PAs. The per-run placement = the lottery.
- **The device-op cliff does NOT apply QNX-side**: 1.95G device stores in
  one payload run (W-90b's runaway) = no wedge at ~33M ops/s (~30ns/op
  inv-only; ~1 μs/op clean+inv on dirty lines).
- The whole-DRAM 0x7F0 sweep = safe for QNX (clean-first = data-
  preserving; QNX kept running through ~35 s of it) and the payload's
  own stack/heap lines = cleaned, not lost.
- The W-54 decompressor dump (bc[20..23] = _edata/[r6]/r8/r4) is
  OVERWRITTEN by the kernel's W-72-era pmd dump (bc[20..25]) — the
  decompressor's values = lost by the time we read back; W-90a's
  bc[20]/bc[21] = 0 = the pmd dump's zeros, NOT the decompressor's.
- **NEW SLOT COLLISION (rule 17): the W-52 diagnostic
  (pb_bc_put(0xD0000030) = __atags_pointer, 0xD0000038 = the fixed-map
  VA) is destroyed by the parse-era C0DE markers (bc[11]=0xC0DE0010,
  bc[12]=0xC0DE0020)** — the __atags value is lost exactly when needed.
  Move it to surviving slots before the next instrumented run.
- The DTB = 87,321 B since the 17:57 rebuild (it was 15,519 forever);
  the decompressor destination = 142KB PAST the W-35 guard end
  (dest end 0xa1023a50 > 0xa1000000; .bss _end = 0xa10680c8) — latent
  placement hazard, unfixed (the sweep clamps to the placement).
- The standing rules all re-proven: rule 13 (three hand-hex slips this
  session, each caught or caused damage — 0x4FBEC1 = 5,226,177 NOT
  5,224,385), rule 16 (every build disassembly-verified pre-run), rule 14
  (mmu.c:997, setup.c:1209, head-S decompressor flow = all read, not
  recalled).

## THE FIRST TASK (W-92): EXPLAIN THE 15,519 (the instrumented run)

The kernel reads fdt_totalsize = 15519 at __atags = the current blob
tail where DRAM = the verified 87,321-B DTB and the L2 = swept. Plan:
1. Fix the bc[12]/bc[14] collision: move the W-52 __atags dump to
   surviving slots (bc[19..31] pre-kernel-era are free-ish — watch the
   sweep heartbeats bc[27..30]).
2. Pre-jump deployed-file check: print /tmp/zImage size + the appended
   DTB header (memdump the tail) — and memdump3 the DTB at dtb_phys.
3. Post-mortem: memdump3 at __atags (the r[0] base) — read the header
   DRAM-truth. If DRAM = 87321 there, the read path = the suspect (the
   fixed map / the recovery chain); if DRAM = 15519, the payload's
   staging = the suspect (readfile2/the copy path).
4. Read the decompressor's ATAG-compat path FIRST (head-S ~line 384-460:
   the folding/relocation when r8 ≠ 0 — the stub passes r8 = TTBR0!).
   If the compat path relocates the DTB, r2 ≠ blob+tree_zlen and r[0]'s
   interpretation changes.
5. ONE VARIABLE: no sweep changes, no kernel changes.

## THE ORDER AFTER (if the 15,519 falls)

1. The determinism check: 3× W-92-class runs — the front should sit at
   126 (the post-CMA TLB op) = the bequest's territory.
2. The 171 wall markers (181-185) = ready.
3. The SMP bring-up (the CPU1 release, the SAR neutralization) = the
   real TLB-op cure. THE PARK IS STILL FORBIDDEN without the SAR
   neutralization.
4. The rootfs era, then the display (the DISPC RE), then the RE queue.

## THE STANDING RULES (the short list — full: docs/README.md)

- ASK THE USER before every device run; hands-off; unfiltered jump.sh
  output; record video; the LED timings on request.
- python for hex (three slips this session — it is load-bearing);
  grep, don't recall; the native edit tools.
- Rule 16 (the shipped binary) before every run; the sweep heartbeats +
  bounded polls on every new sweep; bc[15] nonce on every readback;
  distrust bc[6]+ unless provably fresh (deterministic values = residue-
  indistinguishable — only values that DIFFER from the previous run
  count as fresh).
- NVRAM and RPMB untouchable. The device clock = don't trust.
- Nothing QNX-side after the GICD-off; the kernel make = a clean shell;
  re-scp memdump3 after any reboot; /tmp = wiped on reboot (the deployed
  files die with it — the deploy = per-run).
- git commit + push every state change; the bootstrap = the wrap-up
  product; wrap ONLY at 40-60% context (keep working until then).
- jump.sh may outlive its 300 s wrapper on slow runs (the W-91 sweep =
  +35 s) — the readback = manual if the wrapper dies.
