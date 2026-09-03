# Machine corruption vs code bugs (the "silent DRAM corruption" theory)

## The conflicting claims

**Session 3** (2026-08-31 evening, SESSION-RECORD_2026-08-31): a single-bit
DRAM corruption observed on-device — the DTB magic in bc[6] (0x90000018),
written 0xedfe0dd0 by the payload pre-jump, read back post-WDT2-reboot as
0xadfe0dd0 (bit 30 flipped). Not the 0xAA churn pattern. This was the
session's stated prime suspect for the kernel's silent deaths
(placement-dependent corruption of the kernel image/table). Caveats recorded
at the time: (a) the bc page was OUTSIDE the later memtest's coverage
(session 4 swept the fallback buffer region only); (b) write path was
NOCACHE, read path memdump3 after a warm reset + QNX reboot — a lost write
could mimic a flip; (c) single observation; (d) several of session 3's
"deaths" were later re-attributed to infra bugs (ssh-timeout SIGHUP, missing
WDT2 kicks), so the session's run matrix is weak evidence — but the flip
itself stands as an unexplained single event in a page that is otherwise
read-back-reliable.

**Session 6** (docs/03 evening/late sections, HANDOFF_2026-09-02_night):
"The machine corrupts/loses DRAM transactions under QNX's secure-domain L2
config... Every wild write/|0x80 byte/corrupted pv-patch site this session =
this, and it is placement-dependent." Evidence cited: probe.S bare-metal
loop stalled at ~11 GB of pure cacheable WB stores; nondeterministic early-C
deaths; wild values appearing in kernel data structures.

**Session 7 (W-series runs):** the bc=127 wall — the headline "corruption"
symptom — is fully explained by a code/config bug (kernel load placement vs
DTS bank ⇒ memblock below PHYS_OFFSET ⇒ unlinear-mappable allocations ⇒
`__phys_to_virt` below TASK_SIZE). No machine corruption needed. The
"corrupted barrier stack struct" value (0x1f7f0000) equals the UNPATCHED
pv-stub placeholder computation, and the "corrupted dma_mmu_remap base"
reading was likely a mis-diagnosis of a legitimate placement-varying value.

## Evidence for each

For corruption (session 6): the 11 GB probe loop stall (bare metal, no
kernel involved); multiple placement-dependent deaths.
For code bugs (session 7): every checked "corruption" resolved to a
greppable code fact; W-4 booted cleanly past 127 with the L2 ON once the
placement was fixed.

## What keeps the question open

**The DTB loss (W-4/W-5).** The DTB was verified in DRAM by the payload
(REVERIFY memcmp) and by the probe (magic read OK), then the kernel's
`__vet_atags` saw something else. That is a value dying between two verified
reads of the same address — either a code bug in the r2 chain (head.S
save/restore, cont register passing) or exactly the "lost write" class of
failure session 6 described. W-5 ruled out the 1 GB probe loop but nothing
else.

## What would resolve it

1. Jump the zImage (the decompressor handles r2 natively). If the DTB
   arrives and the boot is clean, the Image-path r2 chain gets the blame
   and the corruption theory is reduced to the historical probe-loop stall.
2. If the DTB is lost on the zImage path too: instrument head.S to store
   r2 + [r2] into bc slots post-restore, and correlate with a probe-side
   recheck immediately before `bx r9`.

## Current status

UNRESOLVED but with a strong lean: code bugs explained every inspected
symptom; one live anomaly (DTB loss) could go either way.

## ADDENDUM 2026-09-03 (session-6 native context: one session-6 claim retracted, two measurements kept)

1. **Retracted: "the barrier stack struct was corrupted".** Session 6's
   records (docs/03 night, the omap4-common.c comment) attributed the
   0x1f7f0000 mapping-BUG to a corrupted `dram_io_desc[0].virtual` in
   omap_barriers_init. But the follow-up run with `omap_barriers_init`
   DISABLED reproduced the identical BUG — the mapping was
   `dma_contiguous_remap` all along (it sits exactly between markers 127 and
   126; the barrier sits in devicemaps_init, later). The session-7
   unpatched-pv-stub reading is consistent with the corrected attribution.
   Residual anomaly: dma_mmu_remap[0].base read 0xa1000000 while the CMA
   printed 0xbe800000 — the base AND the virtual were both wrong in the same
   struct (two anomalies, or one wild write). Unexplained; the PB-CMA print
   will catch the next occurrence.
2. **Kept: the device-op cliff calibration.** With the L2 ON, sustained
   DEVICE (SO) stores wedge at ~4-5k ops (smoke ~480 fine; --l2test phase C
   wedged inside a ~4k-op loop; the device-era console died at ~890 chars /
   ~5k ops). Cached stores do NOT show this (the bss clear = megabytes,
   fine). Any "corruption" observed on a device-store path may just be this
   cliff; size device-access loops accordingly.
3. **Kept with nuance: the 11 GB bare-metal probe-loop stall.** It ran with
   the L2 ON under QNX's 1/1/1-cycle PL310 latencies and CACHED WB stores —
   i.e. it is the one corruption-era datum with no kernel code involved, but
   it shares the L2-config variable with everything else. If the latencies
   are ever fixed (SMC 0x112 lead, KNOWN_ISSUES #6), rerun that loop before
   declaring the machine corrupt.
