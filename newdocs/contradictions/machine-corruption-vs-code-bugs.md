# Machine corruption vs code bugs (the "silent DRAM corruption" theory)

## The conflicting claims

**Session 6 (docs/03 evening/late sections, HANDOFF_2026-09-02_night):**
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
