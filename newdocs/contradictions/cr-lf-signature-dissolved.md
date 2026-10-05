# The "CR→LF death micro-signature" (session 17) vs the decoder off-by-one (session 18)

## The claim (session 17)

Three datapoints — W-99 r1 (the user-region BUG line area), W-101 r1
(PB-ADJ#1's record), W-101 r2 (PB-CMA's record) — were read as deaths "in
the 1-char window after a record's final '\r'": the ring text appeared to
end at a CR with the LF missing, at three different depths. Recorded as an
OPEN cross-draw class ("the CR→LF micro-signature") in PROJECT_STATE,
KNOWN_ISSUES ("NEW OPEN CLASS 1") and the session-17 notes, with a candidate
mechanism ("the first console-adjacent code after a completed write").

## The resolution (session 18, conclusive — an artifact, not a mechanism)

ring_put (arch/arm/include/debug/omap4bc.S) PRE-increments the wrapping
index before the character store: char k lands at base+0x100+k. The ring
text therefore starts at +0x101, NOT +0x100. The session-17 decoder
(decode_readbacks_generic.py) reconstructed the text from raw[0x78:] — one
byte too low — so every decode prepended one residue byte AND dropped the
true LAST character. The "missing LF" was that dropped character: with
raw[0x79:], ALL four datapoints end \r\n-COMPLETE — including W-99 r1's
line ("...in user region\r\n") and W-101 r1/r2/r3.

No death is tied to a record's CR; the class is RETIRED. The dissolution
also restated two death windows (W-101 r2 → the svm-store wedge inside the
dma_contiguous_remap's iotable_init call; W-101 r1 → after the COMPLETE
PB-ADJ#1 record) — see docs/03's "SESSION-18 DECODE CORRECTION".

## Files updated (dated note left in each)

- docs/03_DEBUGGING_SESSIONS.md — the "SESSION-18 DECODE CORRECTION" section.
- newdocs/PROJECT_STATE.md — the SESSION-18 block (CR→LF line corrected).
- newdocs/KNOWN_ISSUES.md — the SESSION-18 header ("OPEN CLASS 1" retired).
- ~/agent-runs/w101-run1-record.md + w101-run2-record.md — dated correction
  footers (the entries stand as written; corrected).
- ~/agent-runs/w101-artifacts/decode_readbacks_generic.py — the fix
  (raw[0x79:]) + a first/last-char sanity note.
