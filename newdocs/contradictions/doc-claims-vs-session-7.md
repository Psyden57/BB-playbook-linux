# Doc claims vs session-7 evidence (stale statements in older files)

These are not live disagreements — session 7 amended the affected files —
but the older text remains in the deep docs and may mislead. Listed here so
a future reader knows which claims are superseded and why.

## 1. "trustzone-omap4 is the secure monitor / RE it for the dispatch table"

- `docs/04_KEY_FILES_AND_COMMANDS.md` workspace table and the session-6
  handoff describe `device-binaries/trustzone-omap4` as "the secure
  monitor — already dumped and disassembled".
- Session 7 (full static RE): it is the `/dev/trustzone` crypto resource
  manager (ECDH/SHA512/RPMB); zero L2 code; the SMC dispatch lives in the
  TI ROM monitor, which no QNX binary contains.
- Evidence: strings (ECDH/secp521r1/SHA512/RPMB/KEK), one SMC site, full
  disassembly. See session-07.md #1.
- Status: docs/04 table not rewritten (the file is a historical snapshot);
  PROJECT_STATE.md + this file carry the correction.

## 2. "Guard cache-l2x0.c against the NS latency SIGBUS"

- docs/01, 05, 08 (NIGHT UPDATE sections) warn the L2X0 driver's latency
  writes SIGBUS and must be patched.
- Session 7 audit: the latency writes already route through
  `omap4_l2c310_write_sec` → default WARN+skip — no patch needed. The REAL
  hazards are `l2c_enable`'s by-way write (0x7FC, the deadlock op) and the
  `l2c_wait_mask` poll.
- Status: amended in place in docs/01/05/07/08 with dated UPDATE notes;
  KNOWN_ISSUES #3 tracks the remaining hazard.

## 3. "jump.sh defaults to Image" vs "zImage = the correct path"

- `kexec/jump.sh` line 6 defaults `KIMG=${1:-Image}`; every W-series run
  invoked `./jump.sh Image` explicitly or by default.
- docs/04 lists `kernel/zImage` as "the correct path" (decompressor handles
  the appended DTB + zreladdr). Session 6's proven boots were zImage.
- Status: unresolved design wart — the default should arguably be zImage.
  Change it after the zImage path is re-verified next session (do not
  silently flip the default).

## 4. "The 1GB DTS memory bank causes the wall" (session 7's own claim)

- Session 7 first root-caused the 127 wall to the bank/load mismatch and
  patched the DTS bank to 0xa4000000+448MB.
- Post-W-5 evidence: on the zImage path the kernel's fdt memory trim
  ("Ignoring memory range 0x80000000 - 0xa0000000", session 6 recovered
  log) handles the mismatch NATIVELY — the original 1GB bank was never the
  problem on that path. On the uncompressed-Image path the placement still
  matters (memblock below PHYS_OFFSET).
- Status: the DTS bank change stands (harmless, belt-and-braces) but the
  causal story is narrower than first written. Recorded in
  PROJECT_STATE.md and session-07.md #6.
