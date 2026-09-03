# The 0xFED UART-section mapping history (session 5 vs session 6)

## The conflicting claims

**Session 5** (runs 24–32 era): the console's UART writes silently vanished
from run 24 onward because head.S never mapped section 0xFED (omap4bc.S's
addruart returns UART virt 0xFED20000 with a comment claiming head.S
early-maps it — session 5 grepped head.S for "FED" and found nothing, then
ADDED the section: VA 0xFED00000 → PA 0x48000000). The added section (with
session 5's original comment) is still present in the current head.S.

**Session 6** (docs/00 "Console truth (2026-09-02 night)"): "the old '0xFED
was never mapped' diagnosis was WRONG — the pristine DEBUG_LL block in
head.S always mapped it via addruart; the real console killers were the
flush machinery and the UART3 store" (both removed in session 6's
rings-only rewrite).

## Why they conflict

Session 5's grep evidence is real (the pre-run-32 head.S had no 0xFED
section descriptor — that is why the section was added, and the added code
survives). Session 6's claim that head.S "always mapped it via addruart" is
hard to reconcile literally: addruart is a register-address macro and cannot
create a page-table mapping, and session 6's own head.S rewrite lists the
diagnostic sections as 0xFEB/0xFEC/0xFED/vectors (i.e. 0xFED exists NOW —
but that is consistent with session 5's addition surviving, not with it
having always been there).

The practical disagreement underneath: session 5 attributed the console
silence to the missing mapping; session 6 attributed it to the flush
machinery + the UART3 store (both removed). Since session 6's rewrite
removed the UART writes entirely, the two fixes are not in conflict on the
current code — but the HISTORY of whether 0xFED was ever missing matters
for understanding the run-24–31 silence.

## What would resolve it

`git log -p -- arch/arm/kernel/head.S arch/arm/include/debug/omap4bc.S`
once the repo history reaches back to the session-5 era (the repo was
created in session 7, so the pre-repo state may be unrecoverable — in which
case this stays a recorded discrepancy).

## Current status

UNRESOLVED as history; MOOT for the current code (the console is
rings-only; UART3 writes are removed entirely; the 0xFED section exists in
head.S either way).

## ADDENDUM 2026-09-03 (session-4 native context: direct evidence for session 6's claim)

Session 4 (2026-09-01 evening, BEFORE session 5's edits) read the then-
current head.S DEBUG_LL block (then at ~lines 445-454, `#ifdef
CONFIG_DEBUG_LL` → "Map in IO space for serial debugging") and it computed
the UART section mapping FROM addruart's outputs:

```
addruart r7, r3, r0
mov   r3, r3, lsr #SECTION_SHIFT
mov   r3, r3, lsl #PMD_ENTRY_ORDER
add   r0, r4, r3            @ table entry for the PHYS section
mov   r3, r7, lsr #SECTION_SHIFT   @ r7 = the UART VIRT from the macro
ldr   r7, [r10, #PROCINFO_IO_MMUFLAGS]
orr   r3, r7, r3, lsl #SECTION_SHIFT
```

Since omap4bc.S's addruart returns 0xFED20000 as the virtual (rebased in the
2026-08-31 earlyprintk fix), this computed idiom maps section 0xFED without
the literal "FED" appearing anywhere in head.S. That explains session 5's
grep failure (the grep was for the literal) and supports session 6's "always
mapped via addruart" claim. Caveat kept honest: this proves the computing
code existed pre-session-5; it does not prove the computed descriptor was
correct at runtime (the value depends on the addruart return and the
io_mmuflags load, and this block sits right next to the ring-map block whose
shift bug session 4 found the same evening — the same lsl-mismatch class was
initially suspected HERE too and explicitly checked: this block uses
`lsr #SECTION_SHIFT` on a real PA/VA, the correct idiom).

Lean: session 6's reading is correct; the discrepancy is closed with this
testimony unless someone produces a pre-session-5 head.S without the block.
