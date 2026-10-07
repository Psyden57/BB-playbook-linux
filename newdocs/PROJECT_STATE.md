# Project State (as of session 14, 2026-10-04)

This file tracks the *current technical state* precisely. Older docs
(`docs/03`, `SESSION-HANDOFF/`) record how we got here; where they disagree
with this file, this file wins (and any unresolved disagreement is listed in
[contradictions/](contradictions/)). Per-session summaries live in
`newdocs/session-notes/session-NN.md`; the historical recaps are kept below
with dated headers.

## Where the boot stands

**SESSION-21 (2026-10-07, wrapped):** the (M) study → F2 design → preflight → **F2 r1 + the SMC-clobber find → fix #167.** TASK-014 (the (M) mechanism/lever study; ~/agent-runs/TASK-014-*) = ACCEPTED, verdict MOSTLY (session-19 cross-check folded): release = RSTCTRL←0; verify = SMC 0x103 + RSTCTRL readback (NS-readable, returns the last write); NO *proven*-safe released cell ⇒ the re-hold is REQUIRED; the [A08] reset-consumer is NOT in the dumped ROM (hidden 4KB); the kexec lead is weak. The F2 design (`W-104-F2-design.md`; relay review SKIPPED — aged relay, user call; the TASK-015 preflight = the gate; GO-WITH-FIXES folded) + build #166 → **F2 r1 (fired 2026-10-07 ≈02:25Z): the F2 block ran; the payload SIGSEGV'd ONE INSTRUCTION after mon_call(0x103) returned r0=2 — the monitor DESTROYS r4 (left = the service id 0x103) and mon_call's asm never declared it (the marker poll deref'd the clobbered register; ref=0x103). QNX survived. The release HAD executed: 6+ min with A08=blob and NO marker ⇒ the payload-time release does NOT wake CPU1 onto [A08] (a hard datum; F3 input). Recovery (02:33:19Z): A08 → 4a326b00 + the pen re-asserted (RSTCTRL 0→1; both readback-verified); smctest r0=2. The crash-era WDT2 window fired ~8 min later INTO the restored safe config = a CLEAN warm cycle (user: RED + the USB chain; keeper re-handshake 02:36:34) ⇒ safe-cell clean cycles N=2.** FIX #167 shipped: mon_call = push/pop {r4-r11} around the SMC (both tools; disasm-verified). qnx2linux 30,584 B sha 827b1c0f5061; smctest 9,401 B sha 5d217e0b798d; commit 1317e32. Record: ~/agent-runs/w104-run2-record.md. NEXT: F2 r2 (user-gated) + the WDT-window calibration thread.

**SESSION-20 (2026-10-06, wrapped):** the W-104 arc end-to-end. THE REDESIGN
(~/agent-runs/W-104-design.md): the reset-safe neutralization = the W-103
instrument + a PRE-KERNEL RESTORE — the continuation (stub3.S cont_start,
IRAM 0x40308000) restores CPU1_WAKEUP_NS_PA_ADDR = 0x4A326B00 iff params[4]
(0x40309810) equals it (armed ONLY by --sarrep; normal --l2on runs skip —
reset-safe). Relay-reviewed (session-19 GO + folds) + TASK-013 preflight
(ACCEPTED; GO-WITH-FIXES) + BUILD #165 (29,108 B sha 08fc7bbf; the link
carries a ~6-byte build-varying stamp — rule-16 via size + disasm + byte
patterns). **★ F1 (flown 23:09:07Z, PAYLOAD_MODE=--sarrep): CLEAN RECOVERY —
the W-103 r1 stall did NOT reproduce with the restore in place (the
single-variable delta; restore-before-reset VALIDATED at N=1).** The run: a
FLIPPED-bucket draw (aac00000; 3rd flipped ever) + the fixup instrument
PASSED again (bc[16]=272 + bc[17]=2; 2nd healthy) + a 126-family wedge death
(3rd instance; trio 126/0x17e/0x27e; inside early_fixmap_shutdown) with a
NEW CMA-reserve-failure sub-shape (empty dma_mmu_remap; console 1181 chars
ending at PB-ADJ#2 — no PB-CMA record; r2's = 1317). Timeline: reboot-wait
163 s; total 4 m 07 s; off→red 68 s (the band's 13th); host USB chain
normal; no button. Post-run: A08 = 0x4a326b00 (restore-default no-op); **the
park blob SURVIVED at 40309A00** (e320f002/eafffffd — contra r1's
post-button "cleared" reading; the WDT cycle does not clear IRAM); PRM_RSTST
= 0x4A307B04 NS-READABLE (first read; PRE=POST=0 → inconclusive — the boot
consumes the sticky bits; USB/LED stays primary); A0C stable (8a852251);
ring3 model 7/7. The USB calibration (user): normal cycles produce host USB
events (bootloader/RNDIS/devmode) — r1's "no reset completed" is calibrated.
Record: ~/agent-runs/w104-run1-record.md. The relay cross-check LANDED
(session-19: record verified; the chain sharpened; the blob = inert; C3
parked as a diagnostic; the ladder starts with the (M) mechanism study;
HARD GATE: no A08-touching flight without the re-hold/verify discipline).

**SESSION-19 (2026-10-05/06, wrapped):** TASK-008 CLOSED the 0x3E7
thread — the writer = kexec/probe.S:157 (the probe's long-loop pass
counter; terminal 999; a DESIGNED benign breadcrumb; visible iff the
death precedes the first kernel bc pair-write; the old
"zImage-correlated" framing = a death-depth artifact; report + root
review in ~/agent-runs/TASK-008-*). W-102 r2 FLOWN: THE FIXUP REGION
PASSED — the instrument's first healthy reading (bc[17]=2 + bc[16]=272 =
walk-1 + the W-101 block COMPLETED); a FLIPPED-bucket draw (placement
0xaab00000; bc[6]=0xe8000000; D13 accept+document) died in the [126→125]
window = early_fixmap_shutdown = the 126-family wedge PROPER (2nd
instance: W-101 r3 → W-102 r2). The triad streak broke at 2; bc[2]=0x17e
(the C-era pair overwrite — TASK-008 semantics verified live). W-94
snapshot intact under the flipped base; ring1/2 = 1317 chars; ring3 model
6/6 exact; off→red = 68 s (12th band instance; deltas 5/16/19/68).
**W-103 r1 (the SAR neutralization flight, session-19's route-(a) stage 0)
= a MECHANISM RESULT: the payload-side repoint + IRAM park blob (built,
rule-16'd, ledger #164) RAN and JUMPED (no abort — the block's verifies
passed), but the post-jump WDT cycle STALLED with the wake path
repointed: the device stayed powered (host USB: no reset event; no RED)
for 8 m 20 s (reboot-wait 21:17:41 → 21:26:01Z) until the user's
power-button hold. Recovery clean — A08 = 0x4a326b00 (armed; restore = a
no-op), the QNX trampoline back, IRAM cleared (the blob gone), A0C
boot-mutable. FINDING: THE RESET/BOOT FLOW CONSUMES CPU1's WAKE PATH; a
parked-IRAM landing is not reset-valid. Route-(a) stage 1's core
requirement is now: a RESET-VALID wake target / kernel-side control
(stage 2's shape), plus restore-before-reset discipline. DRAM evidence
lost to the fresh boot; full record: ~/agent-runs/w103-run1-record.md.**

**SESSION-18 (2026-10-05, W-101 r3 + the decode correction): r3 = a
NON-flipped deep draw (placement 0xa2400000; bc[6]=e0000000, fresh) — death
in the [126→125] window = early_fixmap_shutdown (bc[1]=126 + bc[2]=0x17e +
mirror=0x27e triple; bc[10]=0xde800000 = the dma_contiguous_remap's
iotable_init md->virtual; bc[13]=0xdfbf7000 = the "126-signature") = the
session-11 front / the 126-family wedge PROPER. THE PASS RAN 3/3
(bc[16]=272; arm-zero watched live). The W-94 snapshot ran
(bc[25]=0xa0000000, bc[28]=0x1fe00000 fresh; sentinels intact). ★ THE
DECODE CORRECTION: the ring text starts at +0x101 (ring_put pre-increments
the index) — the session-17 decoder was off by one and the CR→LF
"micro-signature" WAS THAT ARTIFACT: all records (r1/r2/r3 + W-99 r1's BUG
line) end \r\n-COMPLETE; r2's death restated = the svm-store class INSIDE
dma_contiguous_remap (bc[13]=svm_pa=0xbfbfffd4; last mirror=127; md loop not
yet run); r1's window [133→134] stands (refined: after the complete
PB-ADJ#1 record). ring3.count = 23041 + ring1.count EXACTLY (3/3) — the
"accumulates across runs" reading was a +440-delta coincidence. Spectrum
(W-101, 4 draws): r1 = early [133→134] / r2 = svm-store / r3 = 126-family /
r4 = fixup-region (THE TRIAD: bc[1]=142 + bc[2]=0x3E7 + ring 0; bc[16]=0 — the
W-101 block left no completion marker; the 0x3E7 wild write recurred, 3rd
lifetime) — no new classes yet. off→red: 68×8, 67, 68 (r4's recording was
partial — the 68 band restored). **W-102 (the fixup-region fine markers,
bc[17]: 1 = walk-1 done / 2 = inv-loop entry) = BUILT AND FLYING (ledger
#163; banner #162; pack 5,223,233 sha ee303813). W-102 r1 = the 142-window's
FIRST SPLIT: bc[17]=0 ⇒ the fixup-region deaths (r4, r1) are WALK-1 deaths —
the W-101 block is NOT implicated (r4 resolves by equivalence). The triad
(142+0x3E7+ring0) struck 2 consecutive draws; 0x3E7 = 4th lifetime instance.
NEXT: W-102 r2+ draws.** The bucket-flip decision = ACCEPT + DOCUMENT
(DECISIONS D13 — no guard). Session wrapped 2026-10-05 —
BOOTSTRAP_SESSION_19 = the wrap product.**

**SESSION-17 (2026-10-05, the W-101 build + first flights): THE FIXUP-SIDE
PV-STUB INV IS IN AND FLYING (ledger #162; UTS banner "#161"; pack =
5,223,425 B sha256 47c175cb). W-101 = a per-site PL310 inv-only (0x770)
SECOND WALK inside the head.S inline fixup (MMU-off direct PA,
CTRL-guarded, bounded 0x730 syncs every 64 + final; bc[16] = count /
0xFFFFFFFF) — the W-100 start_kernel pass is REMOVED (session-16
directive: replace). Build gotcha fixed mid-build: head.o assembles at
armv6k — no movw/movt there; constants via ldr= pools (post-stext .ltorg,
behind the `b __enable_mmu`). Rule-16 on the shipped artifact: disasm +
pool placement + byte-deltas + DTB identity all verified. THE TWO DRAWS:
**r1 = the pass ran (bc[16] 0→272, live-watched) but the draw died EARLY
at the PB-ADJ#1 record's CR→LF gap (bc[1]=133 — a shallow class); r2 =
THE FIRST BUCKET-FLIP DRAW (placement 0xaaf00000 → kernel base
0xa8000000; bc[6]=0xe8000000; the FDT trims the bank; the
stubs-vs-variable split-brain: W-32c force-writes e0000000 over the true
e8000000) — it went DEEP (map_lowmem + the W-94 memblock snapshot
captured live for the first time) and died at the PB-CMA record's CR→LF
gap (bc[1]=167). THE CR→LF MICRO-SIGNATURE: 3 datapoints now (W-99 r1,
W-101 r1, W-101 r2) — deaths in the 1-char window after a record's final
'\r'; OPEN thread.** The off→red 68 s: 7th/8th instances (68×8). TASK-005
(console silence = F1 "stops being invoked", MEDIUM-HIGH) + TASK-006
(W-101 preflight, GO) accepted. NEXT (session 18): W-101 draws r3+ (the
class-vs-dice question; watch the CR→LF tails); the bucket-flip decision
(payload guard ≥0xa8000000 candidate); the CR→LF signature audit; the
strategic fronts (the 126-family wedge; the CPU1 release) unchanged.**

**SESSION-16 (2026-10-05, W-99 r1 — kernel #160: the omap4bc batch-flush
cadence 64→16 + window 42 lines; front = 126 warm, ring 1246): ★ THE
PB-CMA PRINT CAUGHT AN UNPATCHED PV SITE LIVE — va = 0xbe800000 −
0x81810000 = 0x3cff0000 EXACTLY (the placeholder delta; __pv_offset =
the correct ffffffff e0000000), the exact discriminator the print was
built for; the wrong va then fed create_mapping (PA 0xbe800000 @ VA
0x3cff0000 < TASK_SIZE) → the in-ring "BUG: not creating mapping" line
→ **death MID-PRINTK (the line's '\n' never written; count stops at the
'\r')**. THE RING PERSISTENCE = empirically wider than both flush
windows (text to 1246 = 31 chars past the last batch flush) ⇒ the
console is NOT the bottleneck — the boot dies before printing more; the
cadence change remains the in-flight-printk insurance (untested-for-
effect this draw). The FULL RING TEXT IS ON RECORD for the first time
(the W-99 jump.sh fix: readback 0x20 → 0xf80). off→red = 69 s (the
second ~+10 s-over-WDT2 instance; the bootrom-phase delay reproduces).
Artifacts: kernel #160 pack = 5,224,865 B sha256 b77d1197… (`kexec/
kernel/zImage`; #159 backup in ~/agent-runs/w95-artifacts, #160 in
w99-artifacts). Full record: docs/03 W-99 r1. **TASK-004 audit FOLLOWED
(same session): the unpatched-pv mechanism = a STALE L2 line serving
the .text stub's instruction fetch (HIGH/MEDIUM; 136 sites enumerated,
zero cache ops in the fixup, variables-only W-32c coverage, and the
cross-draw proof: W-96 r2 va=0xde800000 vs W-99 r1 va=0x3cff0000 on
the same function). W-100 CANDIDATES: the inv-only (0x770) discriminator
loop / the per-site inv in the fixup itself.** **W-100 r1-r4 (kernel
#161, the inv-only pass) = FLOWN ×4 on the SAME artifact: the pass runs
(272/272, bc[16]=0x110, 4/4) and is benign. THE DRAW SPECTRUM: r1/r4 =
126-remap-window death (modal, va patched, no BUG); r2 = CMA-reserve-
FAILED → deep (post-setup_arch, bc[1]=111); r3 = THE WEDGE PASSED →
INITCALL ERA (bc[2]=0xBEEF, core level, hero = `rcu_init_tasks_generic`
— the #2 hero after atomic_pool_init; bc[1]=184). The 126-wall = the
CMA-remap's TLB-op family = PER-DRAW DICE, not deterministic. The
stale-site discriminator: 0 hits in 4 draws — the W-101 decision (the
fixup-side per-site inv = the robust cure) heads the session-17 queue.
off→red = 68 s ×6 (the bootloader-phase delta).** NEXT (session 17):
read BOOTSTRAP_SESSION_17; the W-101 decision; the 185-path; the CPU1
release stays the strategic cure front.**

**SESSION-14 (2026-10-04/05, the post-migration session): W-96 (the
determinism series) → W-97/W-98 (the LIVE-poller era: kexec/poll-jump.sh,
host-side 3 s telemetry during the payload window). THE HEADLINE
(W-98 r1): THE BOOT REACHED THE INITCALL ERA — bc[1]=185 (the W-94
record front, reproduced) with THE INITCALL BREADCRUMBS DECODED:
bc[6]=2 = the initcall LEVEL (postcore), bc[7]=0xc0e0719c =
`atomic_pool_init` (System.map-verified) = THE DEATH HERO (the DMA
atomic-pool initializer — the fn RUNNING at death), bc[2]=0xBEEF = the
per-initcall WDT2 kick nonce (main.c:1629-1642 — the ladder mechanism
itself, now documented). ~10 walls past the CMA in one boot from a
COLD machine 3 weeks prior: fixmap → CMA → devicemaps → vmalloc/iotable
→ svm → map_lowmem → kmem_cache_create → taskstats → earlycon → core
initcalls → POSTCORE → atomic_pool_init.**

**The warm-band front distribution (7 warm runs, kernel #159-class
bytes, all --l2on): 126 (×2) / 153 / 156 (×2) / 185 (×2).** The sweep
machinery = the floor-lifter (the whole-DRAM sweep = complete 8192/8192
chunks, 0 timeouts, BY t≤7 s on every flight — ~5-6× the W-91-era
measure; the 23 s blue→magenta question = SOLVED, the sweep is FAST).
The W-96 r2 "+0x300" = the SOLE outlier of the r[0]=kern+APP(history)
law — parked one-run-deep. The WDT2 down-counter read = WLDR (unarmed)
through the payload era (the counter read = the LOAD register until
kicked; the live down-count channel = CLOSED pre-jump; the off→red 68 s
LED question = parked on the bootrom-phase explanation).**

**NEXT (W-99, designed, not built): the RING FLUSH CADENCE = batch-64
= too coarse in the initcall era (W-98's death-era prints sat in the
unflushed tail — the console carried nothing past PB-CMA). One
kernel-side variable: tighten the omap4bc.S batch flush (batch-16 or
the initcall-era per-initcall flush), rebuild → rule-16 → poller run.
THE SECOND THREAD: atomic_pool_init's own death analysis (the
CMA/gen_pool path vs the TLB-op class at this depth) = informed by
whatever the W-99 console says — the ring speaking through the death =
the richest evidence class left.**

## Where the boot stands (the session-14 detail, for context)

**OBSERVABILITY: the console ring was ALIVE THROUGH THE CMA for the first
time in the warm runs (1165 chars, ends at PB-CMA — matching marker 126),
the BerryShell-V4 door keeper rode all three WDT2 cycles + reboots
unattended, and jump.sh's exact SSH option set works through the new
RNDIS path (qemu VM + USB-passthrough).** Fresh baseline before run 1 =
decayed DRAM (cold) — archived in ~/agent-runs/w96-baseline/.

**NEXT (leads after W-96):** (1) the atags archaeology — PB-RES r[0] =
dtb_phys + 0x40 with the CORRECT totalsize 0x15519 (= 87,321); the
pointer delta = a per-run constant; read the decompressor's ATAG-compat
path (head-S ~384-460, r8 = the stub's TTBR0) before the next instrument;
(2) the ring-flush cadence past the CMA (the batch-64 = too coarse in the
126/156 region); (3) the sweep A/Bs, one variable at a time; (4) the CPU1
release (the bequest) = the strategic front.

**LEAD (1) RESOLVED + RESHAPED (root audit, all python — see
agent-runs/TASK-002-atag-compat.md for the full record):** the "+0x40" =
a cross-run comparison artifact — DISSOLVED. Every historical ring's r[0]
= kern + APP(own-era pack) EXACTLY (W-88/W-90a/W-91/W-93 verified).
The ATAG-compat merge = CLOSED (never fires; cannot move __atags).
The LIVE thread instead: **the deployed-shape +0x300 class** — run 2's
ring r0 (low20 0xee9a8 = #155-pack-shaped) vs bc[14] (0xee6a8 = the
on-disk #159 pack) disagree WITHIN one run. W-97's poller run settles it
live: (α) ls -l /tmp/zImage mid-run, (β) bc[2] live-read pre-jump,
(γ) bc[14]-vs-r0 agreement on the readback. **W-97 = the live poller
instrument (host-side only; the WDT2 down-counter covers the two LED
tensions too); kernel #159 + the payload remain byte-unchanged.**

## Where the boot stands (the session-13 detail, for context)

**SESSION-13 (2026-09-12, W-90a/b/c — THE FOSSIL MECHANISM NAILED): THE
STALE-VIEW LOTTERY = THE L2 AS A CROSS-RUN FOSSIL RECORD.** W-90a (the
payload's new per-line 0x7F0 CIPA sweep over the decompressor destination
band [0xa0000000, 0xa1069000), placed in the jump tail after the GICD-off)
= the deepest L2-on run ever: bc[1]=126, parse PASSED (bc[12] landed),
ring 1251 chars — AND the smoking gun: the kernel read fdt_totalsize =
15519 (W-88's DTB size) from the appended DTB whose real header = 87321
(python-verified), because **W-88's kernel-era cached L2 line at that
exact PA (blob+tree_zlen) survived the WDT2 reset, QNX's reboot, and our
NOCACHE writes**. The L2 retains lines across runs; the "lottery" = which
fossil lines overlap the current run's PAs (per-run placement luck). The
old bc-47 "image sweep" = 1 line per 4KB = never cleaned anything.

**W-90b/c added the buffer sweep (0x770 INV-ONLY — the buffer's fossils
can be dirty and 0x7F0's clean would poison the payload's NOCACHE-verified
fresh copy) + the trampoline page (0x7F0, clean-never-discard). W-90b =
INVALID (my `size -= 32` tail underflow on a non-32-multiple size = an
infinite 1.95G-op sweep; the bc[27]=476782 heartbeat pinned it; NOTE:
1.95G device stores QNX-side = NO device-op cliff — the payload userland
IS unbounded, ~33M ops/s). W-90c = the sweeps 3/3 clean BUT the boot died
at the W-84 triad (bc[1]=142 + 0x3E7 + ring 0) = **THE LOTTERY IS NOT
CURED by the dest+buffer sweep — the stale source = ELSEWHERE**: the
leading candidate = the fossils on the memblock-ALLOCATED pages (the
early C's first cached reads of allocations anywhere in the bank) and/or
the L1 I-cache across runs.**

**SESSION-13 CONTINUED (W-92/W-93 — THE "15,519 ANOMALY" = A HEX/DECIMAL
MISREAD, DISSOLVED; THE DEEPEST L2-ON RUN EVER):** the PB-RES size
"15519" = HEX (%llx) = 0x15519 = 87,321 = the DTB's correct totalsize —
the "old 15,519-B DTB" never existed and the W-90a DTB-fossil evidence =
void (see the W-93 erratum in docs/03; the +0x68 = the #156 tree zImage
growing by the capture block itself). Kernel #157's capture (setup.c,
the DTB header kernel-side into bc[20..23]; the mmu.c pmd-dump
colliders removed) PROVED the kernel's DTB view = perfect: the magic
0xedfe0dd0, the totalsize 87,321, sane offsets; the reserve = correct.
**W-93 = bc[1]=156 = THE DEEPEST L2-ON RUN EVER** — past the CMA (126),
past map_lowmem, into the svm-memset region (the W-38-era wall). The
REAL anomaly = the create_mapping "in user region" WARN (map_lowmem saw
a garbage region: PA 0 @ VA 0x20 in W-90a, PA 0x42800000 @ VA 0x62 in
W-92 — placement-dependent) while PB-MEM showed mem=1 correct — the
memblock.memory = corrupted between the print and map_lowmem, or the md
= stale-viewed. THE NEXT CAPTURE: the memblock.memory array at
map_lowmem's entry (the W-92 technique).

**OBSERVABILITY (proven, keep on every run):** the ring batch flush (the
L2-on console), the LED phase markers (blue = payload, magenta at bc 42 =
~15 s pre-jump), the sweep heartbeats (bc[19]/bc[26] = the dest chunk/
timeouts, bc[27]/bc[28] = the buffer's, bc[29]/bc[30] = the whole-DRAM's),
bc_write(48)/bc_write(36) = sweeps-survived, and kernel #157's DTB
capture (bc[20..23] — the mmu.c pmd-dump colliders = removed).

The two L2 states: L2-off = the deterministic TLB-op wedge (the W-72..77
era) but DRAM-truth stores; L2-on = the fossil lottery (partially tamed:
the sweeps moved the front 126 → 156). Neither is bootable yet; the
cures in flight = the map_lowmem region-loop capture + the CPU1 release
(the real fix, the bequest).

## Where the boot stands (the session-12 detail, for context)

**W-84 RESULT (2026-09-12, the first genuine L2-on run since W-46, kernel
#153 unchanged): the boot REGRESSED — death between markers 144 and 145
(the early-C region), NOT past the CMA. The L2 was ON confirmed on-device
(bc[8]=1 probe, bc[10]=1 payload). The FDT chain worked (bc[12]/bc[14]
fresh). Ring1 = 0 = STRUCTURAL: the W-77 de-CIPA'd ring writes are
L1-dirty and die at the reset with the L2 on — the console is
UNAVAILABLE in the L2-on state until the ring flush is restored. The
W-35 triad (142 + 0x3E7 + ring 0) reproduced with a CLEAN placement
(0xa2e00000) — the L2 state = the variable. All bc[6]+ slots = proven
W-83 residue (the pgd shapes 0x041e = the SMP=n desc shape, the S bit
OR'd only from the SMP TTB flag — mmu.c:619).**

**W-89 + THE SESSION-12 SYNTHESIS (2026-09-12): THREE L2-on boots
(W-84, W-88, W-89, kernel #153-#155) = THREE DIFFERENT death points —
W-84 = the early C [144→145] (0x3E7 ✓), W-88 = [98 → 96]
(local_flush_tlb_all in devicemaps_init, PASSED 125/the clear_fixmap
TLB op on the way!), W-89 = mid-parse_early_param (0x3E7 ✓). The L2-on
early C = THE SESSION-9 STALE-VIEW LOTTERY (decompressor-era L2 lines +
per-run eviction luck), not a deterministic front. The W-88 deep run =
the lucky draw. The TLB-op skips = marginal relief; the poison =
upstream. THE NEXT CURE = the wide stale-line invalidation (the
monitor 0x101 or the NS 0x770 inv-by-PA over the image + pgd + data
regions) BEFORE the C world reads — the W-32c/111 sweeps cover the pv
and the pgd only.**

**OBSERVABILITY (proven W-86..W-88, both channels):** (a) the payload's
LED phase markers — blue = the payload, MAGENTA = pre-GICD-off (the
frozen color = "died at the jump" in video; NOTE: the i2c devctl is
ILLEGAL after the GICD-off = the W-86/87 lesson); (b) the ring BATCH
flush (every 64 chars, guarded on the L2 enabled, NS 0x7F0 + the
bounded 0x730) = the console log survives in the L2-on state (W-88 =
1165 chars, the full log to the death). The kernel-side LED colors =
CLOSED (the --ledprobe: the direct NS I2C4 access = SIGBUS, the MMCHS
class).

The two L2 states: L2-off = the deterministic TLB-op wedge (the W-72..77
era) but DRAM-truth stores; L2-on = the stale-view lottery (per-run
random early-C deaths). Neither is bootable; the cure = the stale-line
invalidation (L2-on) or the CPU1 release (the real fix, the bequest).

## Where the boot stands (the session-11 detail, for context)

Mainline Linux 6.15.11 (omap2plus, non-LPAE, patched, **CONFIG_SMP=n as of
W-80**) is jumped from QNX via the **zImage path**, **--l2on** as the run
mode. **ERRATUM (session 12): --l2on has DISABLED the L2 since the W-46
rewrite (a692300) — every session-11 run ran with the L2 OFF (probe.S's
bc[8] = the PL310 CTRL readback = 0 in all of them). The session-11
"L2-state-dependent" framing is superseded: the variable that moved the
front = dropping the devb slay. See the erratum at the end of the
session-11 section in docs/03.**

1. The zImage decompressor delivers the appended DTB natively; the setup.c
   FDT-recovery chain works end-to-end (the machine-model line prints).
2. **THE CONSOLE IS ALIVE**: the earlycon ring carries the full early log
   (the machine model, the memory policy, the cma reservation, the PB
   prints) — alive since the session-4 era (#51), silenced somewhere in
   the session-10/11 runs, back in the W-80/83 readbacks.
3. **The CMA block passes**: the TLBIALL is SKIPPED (the W-72/W-83-proven
   pass) and the boot reaches dma_contiguous_remap-done (bc[1]=126).
4. **THE WEDGE FAMILY (the session-11 discovery)**: the machine wedges on
   SCU-routed global ops with CPU1 held — (a) the TLB maintenance ops
   (deterministic with the L2 off; flaky with the L2 on), and (b) the
   ldrex/strex exclusives (the spinlocks — deterministic with the L2 on +
   SMP=y; the first printk was the kill site; harmless with !SMP's plain
   spinlocks). RULED OUT with direct evidence: the barrier domain/encoding
   (the session-10 "dsb nosh" chain never existed on the hardware — GNU as
   rejects the name; GCC's IAS silently emitted the same full-system mcr;
   the W-68 f57ff062 literal = an ISB-class encoding with an invalid
   option), ACTLR.SMP/FW (all three states wedged), CPU1 parked vs held
   for the TLB ops (the W-73 park failed AND broke the post-reset
   recovery — the PRCM hold bit persists across warm resets and the
   released CPU1 resurrects QNX via the SAR path; THE PARK IS FORBIDDEN
   until the SAR neutralization + the kernel-side re-hold exist), the pgd
   content (the W-74 zeroing), the descriptor cacheability (the W-75
   strip — live and proven, and the "poison pair" = actually valid section
   descs in both shapes: the W-37 reading was an attribute-bit misread),
   the SCTLR cache state (the W-76 C/I=0), and the CP13 TLS writes (the
   W-63 class — the W-81 skip changed nothing at 150; the set_my_cpu_offset
   skip is KEPT as harmless).
5. **The current front = the first TLB op after the CMA (clear_fixmap in
   early_fixmap_shutdown)**, death bc[1]=126→125. The TLB ops need the
   CPU1 release (the bequest's SMP bring-up) or a payload-flow fix — the
   runs 23-32 era (the old --t3 payload) is the only era where TLB ops
   ever completed; the era's payload + the current kernel = the next
   session's opening bisect (the git archaeology).
6. The DMA masters: the devb slay = a WEDGE CORRELATE (the W-69..77
   slay-era runs wedged 6/6 at the TLBIALL; the no-slay runs passed the
   CMA). **The payload mode matrix after the W-84 fix: --l2on = L2on +
   no-slay; --dmaquiet = L2on + slay (it no longer disables the L2!);
   --t3 = L2off + no-slay.** W-85 = the slay A/B under the L2-on
   (--dmaquiet), ideally after a battery pull (DRAM+L2 wiped = a clean
   machine, no residue noise).
7. Marker-number discipline: setup.c's pb_bc(130-136) pairs COLLIDE with
   mmu.c's PB_MMU_BC numbers — discriminate via the mirror channel (rule
   17 in docs/README). Extended forensics bc[16]-bc[27] + the W-72 pgd-dump
   slots bc[28..31] (0x90000070-7C) via `memdump3 90000040 0x40`.

## What was fixed, by session

- **Session 7 (2026-09-03) — the bc=127 wall**: the runtime-PHYS_OFFSET/
  DTS-bank mismatch (bottom-up memblock allocs below PHYS_OFFSET →
  `__phys_to_virt` under TASK_SIZE → the "in user region" BUG). Fixes:
  the 129-slot placement sweep, `kern_off` placement,
  `fdt_patch_memory`, the PB-CMA print, and the DTS bank baked to
  match zreladdr. [UPDATE 2026-09-04 (W-21): the bank = 0xa0000000 +
  512MB — the old 0xa4000000+448MB default put the kernel outside its
  own memory.]
- **Session 8 (2026-09-04) — the fixup delivery paradox**: the called
  `__fixup_pv_table` never executed via ANY mechanism; INLINING it into
  head.S's streamed region (W-17) broke the wall — the boot reached the
  C world (bc=127/146-era). The paradox mechanism remains open
  (KNOWN_ISSUES #10); the rule stands: MMU-off helpers are INLINED,
  never called. Also session 8: the bank fix (W-21), the surviving-slot
  markers (W-22), the dual-level pv invalidate (W-24 — later superseded).
- **Session 9 (2026-09-04) — the pv cure + the stale pgd pair**: the
  "wandering" early-C deaths (svm alloc W-25/31/32a, FDT walk stack
  smash W-26, map_lowmem pte alloc W-27, MMU-enable W-29, head.S tail
  W-35) all resolved to (a) the stale-pv regime (CURED) and (b) the
  stale pgd pair (workaround in #110). The DMA quiesce (--dmaquiet)
  did NOT cure the randomness → the corruption class = stale-view
  (characterized in contradictions/), not a rogue DMA master. The
  swipe/tap interaction variables were exonerated (W-32a hands-off =
  byte-identical death). The placement-overlap guard came from W-35.

## The DTB delivery issue (Image path — PARKED)

On the uncompressed-Image path, the kernel reports
`Warning: Neither atags nor dtb found` twice and falls back to a 16 MB
memblock region — yet both the payload (REVERIFY) and the probe validate
the DTB at `params[1]` before the jump. The vet constant is exonerated
(`head-common.S` defines `OF_DT_MAGIC 0xedfe0dd0` for LE builds) and the
register chain was correct at probe time at least once. **Parked in
favor of the zImage path** (D6); investigate only if the zImage path
ever fails.

## Next run (W-39): build #110 — the 2MB allocator shave

W-38's pmd pattern: the pair [0xdfc/0xdfd] = IDENTICAL bogus table
pointers (0xbfc1141e, both halves = the __pmd_populate signature),
DETERMINISTIC across runs AND placements; [0xdf8] = a CORRECT section
desc (0xbf81141e — map_lowmem's mapping of the range is healthy, only
the LAST pair is stale); [0xdfe+]=0 ✓; [0xdf0/0xdf4]=0 (CMA-cleared ✓).
Verdict: the last pair of the linear map holds STALE QNX-era page-table
content (0xbfc11400 = a QNX-era pte table for that DRAM region) — the
write-back-loss class, same as the pv variables — NOT a random wild
write. Build #110: arm_lowmem_limit -= 2MB after map_kernel (nothing
allocated in the poisoned top; the svm lands at ~0xbfbfffd4, pair
[0xdfa/0xdfb] — the previously-untested middle pair), stores re-enabled
with markers 167 (dumps) / 0x567 (store 1) / 151 (memset done), bc[19]
= readback, bc[25] = VA 0xdfa pmd. Expectations: if the middle pair is
healthy → the memset completes → the boot advances (toward the 171
wall, KNOWN_ISSUES #3 l2x0 hazards ahead); if the middle pair is ALSO
stale → the stale region is wider → extend the shave or self-heal the
pgd (the W-32c store+clean pattern applied to the pmd). Marker-number
audit: setup.c's pb_bc(130-136) pairs COLLIDE with mmu.c's PB_MMU_BC
numbers — discriminate via the mirror channel. See docs/03 W-25..W-38
and SESSION-HANDOFF/BOOTSTRAP_SESSION_10.md.

## The secure monitor — RE closed (session 7)

- No QNX binary contains the SMC dispatch (`trustzone-omap4` = the
  /dev/trustzone crypto resmgr; `libsecure_dispatcher` = crypto/KDS;
  `procnto` = 0 SMCs). The monitor is the TI ROM monitor.
- The documented service table (0x100 L2X0 DBG_CTRL, 0x102 L2X0 CTRL,
  0x101 L2 clean+inv by PA, 0x108 SCU_PWR/suspend, 0x109 L2X0 AUXCTRL,
  0x113 L2X0 PREFETCH) — **ERRATUM 2026-09-11 (bootrom RE, TRM §27.5
  Table 27-61): the claim "no tag/data-latency service exists" was
  WRONG — 0x112 writes the PL310 Tag AND Data RAM Latency registers
  (r0 = tag, r1 = data). The ROM itself never touches PL310 directly
  (0 direct L2 constants in the dump); L2 work routes through these
  services. 0x112 = untested on this HS unit (rejection risk like the
  PPA 0x25/0x23 pair) but one mon_call + readback answers it. See
  bootdumps-2026-09-11/BOOTROM-RE.md and
  newdocs/audit-approach-2026-09-11.md.**
- PPA probe (run PPA-1): 0x25 and 0x23 rejected (0xFF02), 0x26/0x27
  (devpm's suspend pair) accepted with no PL310 readback change.
  Secure-side L2 reconfiguration is **exhausted** as a fix path.
- New lead: the eMMC secure-boot chain (user-area sectors 5-240) contains
  the QNX initial loader with NS-side SMC wrappers for **0x112** (×2) — an
  undocumented service. Not yet semantically identified. See
  `newdocs/session-notes/session-07.md`.

## Kernel-side known hazards (ahead of the boot)

When the boot first reaches `l2x0_of_init` (init_IRQ):
- `l2c_enable` does a by-way write to `L2X0_INV_WAY` (0x7FC) — the
  on-device DEADLOCK op (NS by-way ops wedge the machine).
- `l2c_wait_mask` introduces a poll loop.
- The DTB latency writes are safe (routed through
  `omap4_l2c310_write_sec` → default WARN+skip).
Patch `l2c_enable` or run `CONFIG_CACHE_L2X0=n` (current config has it on)
when the boot gets there.

## Operational state

- Payload in `kexec/` (see [../kexec/README.md](../kexec/README.md)):
  the placement sweep + guard, kern_off, DTB patching, the WDT2
  lifecycle, the DISPC kill, the devb slay; modes
  `--dmaquiet/--l2on/--t3/--probe/--ppa/--l2lat/--l2test` —
  **--dmaquiet is the run default (session 9)**.
- Kernel tree: `/home/psyden/kernel/linux` (6.15.11, config recoverable
  from any built Image via `scripts/extract-ikconfig`); the repo's
  kernel state = `kernel-patches/` (regenerate after every change).
- Device: SSH root@169.254.0.1, key at `playbook-dev/rsa` (local-only,
  never commit). WDT2 warm-reset cycle ~59 s. LED sequence observable
  (user video-records runs; the device clock = GMT-3, the host = UTC).
