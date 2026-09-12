# Session 13 Notes (2026-09-12 — the L2 = a cross-run FOSSIL RECORD

# (the stale-view mechanism nailed), the W-90 sweep trilogy, and the

# whole-DRAM W-91 design)

## THE RUN MAP (W-90a/b/c, all --l2on, kernel #155 unchanged)

| run | build | change | result |
|-----|-------|--------|--------|
| W-90a | payload 987a7f8 | the dest-band per-line 0x7F0 sweep in the jump tail | ★★ bc[1]=126 = the DEEPEST L2-on run ever; parse passed; ring 1251 chars; the DTB-totalsize FOSSIL SMOKING GUN |
| W-90b | payload 536cffa | + the buffer 0x770 inv-only sweep + the trampoline page | ★ INVALID: my tail underflow (size not 32-multiple) = an infinite 1.95G-op sweep; the heartbeats pinned it (bc[27]=476782 chunks at the WDT2 expiry) |
| W-90c | payload 1472b26 | size &= ~31 | the sweeps 3/3 clean, the chain ran — the W-84 triad death (142+0x3E7+ring 0) = the lottery NOT cured |

## CONFIRMED (this session's durable facts)

1. **THE L2 = A CROSS-RUN FOSSIL RECORD.** The L2 retains lines across the
   WDT2 warm reset, QNX's reboot, and the payload's NOCACHE writes (which
   create NO L2 lines). W-90a's kernel cached-read fdt_totalsize = 15519
   (= W-88's DTB size, whose header sat at the EXACT same PA: blob +
   tree_zlen) from an appended DTB whose real header = 87321
   (python-verified). The stale-view lottery = WHICH fossil lines overlap
   the current run's PAs = per-run placement luck.
2. **The old bc-47 "image sweep" = ONE line op per 4KB page = 0.8%
   coverage.** Per-LINE (32B) sweeps = the real thing.
3. **THE DEVICE-OP CLIFF DOES NOT APPLY QNX-SIDE**: W-90b's runaway sweep
   = 1.95G device stores at ~33M ops/s (~30ns/op) = the full 59 s WDT2
   window without wedging the machine. A whole-DRAM sweep (1GB = 33.5M
   lines) = ~1-2 s = FEASIBLE (W-91's foundation).
4. **Two region classes = two ops**: regions with NO fresh DRAM truth
   pre-jump (the decompressor destination, the pgd, QNX's live lowmem) →
   0x7F0 CIPA (clean = data-preserving; the clean-back writes junk over
   junk); regions where DRAM = the payload's NOCACHE-verified fresh copy
   (the jump buffer) → **0x770 INV-ONLY** (the fossils there can be DIRTY
   and 0x7F0's clean would push them OVER the fresh copy = W-33 for real).
   The trampoline page = the exception: the payload's CACHED memcpy = dirty
   lines that MUST be 0x7F0-cleaned to DRAM, never discarded (enter_stub
   re-reads them cached).
5. **Sweep heartbeats = load-bearing**: bc[19]/bc[26] = the dest
   chunk/timeouts, bc[27]/bc[28] = the buffer's; bc_write(48) = all
   survived. W-90b was diagnosable ONLY through them. Bounded sync polls
   per 4096 lines (rule 10).
6. **The decompressor destination = 142KB PAST the W-35 guard end**
   (Image 0x101ba50 → dest end 0xa1023a50 > 0xa1000000; .bss _end =
   0xa10680c8). Latent hazard: a buffer placed at 0xa1000000-0xa1068fff
   would sit inside the decompressor's output range. Not fixed (one
   variable); the sweep clamps to the placement.
7. The W-84 triad (bc[1]=142 + 0x3E7 + ring 0) = REPRODUCIBLE WITH THE
   FULL SWEEP — the early-C poison = NOT (only) the dest/buffer fossils.
   Leading candidates: the memblock-ALLOCATED pages' fossils (the early
   C's first cached reads of allocations anywhere in the bank) and/or the
   L1 I-cache across runs.
8. Slot forensics discipline: the stub echo (bc[6]=0xe0000000) and the
   probe values (bc[7]/bc[8]) = deterministic = residue-INDISTINGUISHABLE;
   only values that DIFFER from the previous run's (bc[10]=1 vs W-90a's
   0xde800000) = provably fresh.

## THE W-91 DESIGN (the next cure, not yet run)

One EARLY whole-DRAM 0x7F0 CIPA sweep in the payload (before the file
reads, bc ~36-42): [0x80000000, 0xC0000000) = 1GB = 33.5M lines ≈ 1-2 s.
QNX-safe (clean = data-preserving — QNX keeps running through it). Evicts
EVERY fossil machine-wide BEFORE the payload's copies and the jump. The
bc-51 dest/buffer sweeps = belt-and-braces. The heartbeats + the bounded
polls = mandatory. THEN: the boot should either go deterministic
(3/3 past the early C → the 171 wall) or the poison = NOT fossils at all
(the L1 I-cache candidate = the next A/B: the sweep + a deliberate L1
flush strategy).

## THE W-91 RUN (2026-09-12, the day's final run) — 126 AGAIN, AND THE
## 15,519 ANOMALY SURVIVED THE WHOLE-DRAM SWEEP

The sweep = ran (~35 s — clean+inv ≈ 1 μs/op = 33× slower than the W-90b
inv-only runaway; the late bc-51 WDT2 kick covered it; the jump worked).
The boot = **bc[1]=126 = the SAME front as W-90a**, parse passed (bc[11]
+ bc[12]), ring = 1165 chars (the full log). bc[13]=0xdfbf7000 = the
SAME pte pointer as W-90a = deterministic. The front = 126 in 2/3 valid
runs (c = the W-84 triad outlier).

**THE HEADLINE TWIST: PB-RES r[0]=a2aee9a8+15519 — the kernel STILL read
fdt_totalsize = 15,519 with a fully-swept L2 and a python-verified
correct deployed artifact chain** (W-90b's bc[2] = 0x4FBEC1 = 5,226,177 =
the host zImage exactly — my "stale deploy" reading = a hand-hex slip,
corrected by python; the zImage's appended-DTB header = totalsize 87321
at exactly tree_zlen; the payload's memcmp = verified). The L2-fossil
explanation = RULED OUT for this value. The old 15,519-B DTB copies from
W-84..89 = still in DRAM (the sweep = cache-only!) = the leading
candidate for what the kernel's FDT recovery/entry chain actually
latches onto, but r[0] = the CURRENT blob tail = unresolved. Session 14:
(1) move the W-52 __atags dump out of bc[12]/bc[14] (the parse-era C0DE
markers destroy them — rule 17 named), (2) the deployed-file pre-check,
(3) memdump3 at __atags post-mortem, (4) the decompressor's ATAG-compat
path (r8 = the stub's TTBR0 = nonzero = "an ATAG list around"!) = read
the compat code before the next run.

## RULES RE-PROVEN / NEW

- Rule 13 (python for hex): W-90b's underflow = a hand-skipped alignment
  check (0x71E9B1 & 0x1F = 0x11). EVERY size passed to a stepping loop =
  python-verified alignment.
- Rule 16 (the shipped binary): all three payload builds verified by
  disassembly before the run (the inlined sweep, then the three l2c_
  ns_line_range calls with ops 0x7F0/0x770/0x7F0, then the bic #31).
- Rule 14 (grep, don't recall): mmu.c:997's "in user region", setup.c:1209's
  DTB reserve, the PB-MEM/PB-RES print placement (arm_memblock_init TOP =
  pre-steal reserves) — all read from the tree, several saved from wrong
  recollections.
- NEW: infinite-loop sweeps = bounded ONLY by the WDT2; the heartbeat
  slots = the only post-mortem. Every new sweep gets one.
