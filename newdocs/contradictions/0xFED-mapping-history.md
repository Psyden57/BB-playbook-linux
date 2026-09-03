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
