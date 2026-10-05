# Session 16 Notes (2026-10-05 — W-99: the flush-cadence build + THE LIVE PV CATCH)

## THE SESSION'S SHAPE

Boot from BOOTSTRAP_SESSION_16 (session-14's wrap product). Read order done
(HANDOFF/DEVELOPMENT/SETUP/PROJECT_STATE/session-14/docs-README-rules/
PLAYBOOK-REFERENCE §5/§8/§10/docs-03 W-95..W-98/session-02 traps). Door
keeper + device link verified (single hold instance, rides reboots). ONE
user-gated device run: W-99 r1. Subagent delegation: TASK-003 (preflight
audit of the W-99 edit) — DeepSeek 4.1 flash, high effort, 7 min, exit 0.

## THE W-99 BUILD (offline)

- TASK-003 = the 2-literal omap4bc.S audit (tst r5 #63->#15; mov r2
  #36->#42). Report: ~/agent-runs/TASK-003-w99-preflight-report.md; root
  review appended to TASK-003-w99-preflight-audit.md (verified: the
  count-order trace, the coverage numbers python-derived, the 1165-tail
  arithmetic, the early_write->pb_flush_rings second-flusher CONFIRMED
  from source). Verdict: GO WITH FIXES, all adjudicated.
- **The persistence model as of W-99**: THREE paths write ring1 to DRAM:
  (1) pb_flush_rings (SMC 0x101, [0x80,0x580), runs per COMPLETED console
  write — the primary path; its window = chars <= 1151); (2) the omap4bc
  batch flush (NS PL310 0x7F0 CIPA; W-99: every 16 chars, window
  [0x80,0x5C0), = chars <= 1215 — the never-completed-printk insurance);
  (3) a wider empirical mechanism (unidentified; the observed text runs
  past BOTH windows: W-96r2 +14 chars, W-99 +31 — candidates: L2
  dirty-line survival + QNX-side eviction writeback, or monitor range-op
  semantics). The console is NOT the bottleneck — the boot dies before
  printing more.
- Edits: the 2 literals + comment corrections (file-top: 3840-char ring
  [was stale "1024"], no-map removed [DTS has no no-map — the pages are
  reserved, kept in the linear map]). Build (clean shell, CROSS_COMPILE)
  -> rule-16: disassembly deltas EXACT (tst #15: 2->2 sites; tst #63
  2->0; mov #42 +2; mov #36 -2); Image size identical; DTB byte-identical;
  packed = tree zImage + DTB + 0 pad. Pack #160 = 5,224,865 B sha256
  b77d1197... (backups + SHA256SUMS in ~/agent-runs/w99-artifacts/; the
  #159 pack stays in w95-artifacts).
- Snapshot: regenerated via the documented recipe, then SPLICED (only the
  omap4bc.S section replaced) to keep the diff minimal; the timestamp-
  strip step added to COMMANDS.md's recipe (mtimes = churn). git apply
  --check clean on pristine.
- Instrument fix: jump.sh ring readback 0x20 -> 0xf80 (the built-in
  capture was the HEADER ONLY; every prior full ring dump was manual).
  poll-jump (alpha) expected sizes updated for #160. DEVELOPMENT.md
  decode command updated.

## W-99 r1 RESULTS (full record: docs/03)

- Front: bc[1]=0x7e=126 with the proper pair (0x17e/0x27e); nonce fresh;
  warm draw #3 at 126 (band: 126x3, 153, 156, 185 — W-99 = the third 126).
- Ring: count=index=1246; FULL TEXT ON RECORD (first time — the new
  readback): ... PB-CMA (va=3cff0000 pv_off=ffffffffe0000000) -> BUG:
  not creating mapping for 0xbe800000 at 0x3cff0000 in user region ['\r'
  only - the '\n' never written].
- ★ THE CATCH: python: 0xbe800000 - 0x81810000 = 0x3cff0000 EXACTLY = an
  UNPATCHED pv site (the placeholder delta) with __pv_offset correct =
  the PB-CMA print's designed discriminator, caught live for the first
  time. The wrong va fed create_mapping -> user-region refusal -> death
  MID-PRINTK (tightest death localization yet).
- The 126 class = a REGION (dma_contiguous_remap aftermath); the
  unpatched-pv overlay = draw-dependent (absent from W-96r2's 126).
- LED: blue+5 / magenta+15 / off+38 / red+107. off->red = 69 s = the
  second ~+10 s-over-WDT2 instance (bootrom-phase delay stable).
- Live: sweep 8192/8192 by +13 s; placement 0xa3000000; deployed
  /tmp/zImage = 5,224,865 EXACT (the payload's own bc[2]-live read).

## ARTIFACTS

- ~/agent-runs/w99-run1-{bc,ring,mirrors}.txt (fresh dumps), w99-run1-led.txt,
  live-20261004-222541.log, w99-artifacts/ (pre/post images + packed + dtb +
  SHA256SUMS), TASK-003-* (report + reviewed brief).

## OPEN / NEXT

1. The unpatched-pv-site audit: DONE same-session (TASK-004; report +
   root review in ~/agent-runs/TASK-004-*). Mechanism (a) = a stale (L2)
   line serves the C-world fetch of a .text pv stub (HIGH/MEDIUM):
   - 136 sites / 272 instructions enumerated; no coverage gap (S1/S2/S3/
     I1 all in __pv_table; S1 = the observed dma-mapping.c:287 site).
   - The inline fixup's patch loop (head.S:373-393) = ZERO cache ops
     (W-77's removal rested on a since-reverted C=0; the C world is
     cached).
   - The W-32c block covers only the two pv-variable lines; no stub line
     is ever cleaned/invalidated.
   - CROSS-DRAW PROOF (root): W-96 r2 va=0xde800000 (patched) vs W-99 r1
     va=0x3cff0000 (placeholder), same function/era ⇒ draw-dependent.
   - §6 FIXED by the root: the discriminator must be INV-ONLY (0x770),
     never CIPA (clean would poison pre-patch bytes into DRAM).
2. W-100 candidates (pending user pick):
   (i) the one-loop discriminator (extend W-32c with an inv-only pass
       over the pv sites) — a run settles (a);
   (ii) the robust cure = per-site inv inside the fixup itself (MMU-off,
       PL310 PA — the omap4bc-proven pattern). ONE kernel-side variable
       either way.
3. More draws: the 185-path (atomic_pool_init evidence) + the 126-repeat
   determinism; the cadence's effect = still untested-for-effect.
4. The CPU1 release = the strategic cure front (unchanged).
