# Session 17 Notes (2026-10-05 — W-101: the fixup-side pv inv, built + flown r1/r2)

## THE SESSION'S SHAPE

Boot from BOOTSTRAP_SESSION_17 (session-16's wrap product). Read order done
(HANDOFF/DEVELOPMENT/SETUP → PROJECT_STATE → session-16 → docs/03 W-99/W-100
→ rules 1-17 + KNOWN_ISSUES → PLAYBOOK-REFERENCE §5/§8/§10). Door keeper +
device link verified. TWO user-gated device runs (W-101 r1, r2) + the full
decode battery. Subagents (DeepSeek 4.1 flash high): TASK-005 (the
console-silence audit) + TASK-006 (the W-101 preflight audit). The
session-16 agent was consulted via the user relay for the W-101 design
directive (REPLACE the W-100 pass). Committed: the W-101 kernel change
(c1340b5) + this wrap.

## THE W-101 BUILD (offline)

- **Decision (user + session-16 directive): REPLACE.** The fixup-side inv
  strictly dominates in coverage-time ("no C-world fetch, however early,
  can see a pre-patch line"); keeping both = attribution muddle + two
  writers of bc[16]. W-101 = the per-site pass MOVED into the fixup.
- Design: second __pv_table walk right after walk 1 (before `b 976f`);
  PL310 at PA 0x48242000 (MMU-off, omap4bc-proven); inv-ONLY 0x770 (a
  clean would poison DRAM — the W-90a class); CTRL bit0 guard (L2-off =
  skip all line-ops, the W-77 poison class); site PA = entry + table
  address (already physical at MMU-off); bic #0x1f line-align; bounded
  0x730 poll every 64 + final (busyuart's shape, rule 10); bc[16] = count
  (= table bytes/4) / 0xFFFFFFFF on the skip path. Registers: r0/r3/r4/r6/
  r7/ip only (contract: preserve r1/r2/r8/r9/r10).
- Edits: head.S +61 lines (lines 395-458); main.c: the W-100 block removed
  (replaced by an 8-line pointer comment).
- **THE BUILD GOTCHA (worth remembering):** the first build FAILED — all 6
  movw/movt rejected ("selected processor does not support ... in ARM
  mode"). Cause: head.o assembles at `-Wa,-march=armv6k` /
  `-D__LINUX_ARM_ARCH__=6` (its own .head.o.cmd — COMMANDS.md's GAS-note
  from session-04 #8 describes it). FIX: the file's `ldr =const` pool
  idiom (at this arch adr_l ITSELF expands to an ldr= pool —
  assembler.h:596-601). ALSO learned: `dsb` at this arch = the
  assembler.h macro (mcr p15,0,r0,c7,c10,4).
- **Rule-16 (the shipped artifact):** block disassembled instruction-for-
  instruction (0x770 store @ c00081f8; both bounded polls; the two dsb
  macro sites; pool words 0x48242000/0x90000040 @ c00082e0/e4 — BEHIND the
  `b __enable_mmu` terminator, no fallthrough; handoff to
  __create_page_tables intact); byte-delta census: 3 flags all resolved
  (2 = census boundary accidents, disasm-proven present; 1 = the dsb
  macro); __pv_table 272 @ c0e44a7c..ebc; DTB byte-identical (64ace81d);
  pack = 5,223,425 B sha256 47c175cb (backups in ~/agent-runs/
  w101-artifacts/).
- Snapshot: full regen (only hunk-renumbering + separator-blank
  normalization vs the spliced version); git apply --check clean on
  pristine. Committed c1340b5 + pushed. poll-jump alpha sizes updated.

## TASK-005 — the console-silence audit (ACCEPTED, with the root review)

- Question: why does ring1 freeze at the PB-CMA line (count 1160) on the
  deep draws. Verdict: **F1 = the early console STOPS BEING INVOKED after
  PB-CMA** (MEDIUM-HIGH; the write path itself is healthy — proven by the
  count+bc-guard argument); F4 (death-masking) HIGH for r1/r4/r2. The
  first print after PB-CMA = "Zone ranges:" (mm/init.c:1858).
- Root-verified: keep_bootcon=1 PROVEN from the dump itself ("debug: skip
  boot console de-registration." at char 705 — python-decoded by the
  root); "Zone ranges:" read verbatim; the no-print span in paging_init
  (dma_contiguous_remap → bootmem_init) confirmed; setup.c:1262's
  parse_early_param (with the project's own "earlyprintk registered"
  comment) precedes paging_init.
- Flags resolved: bc[10]'s writer FOUND = mmu.c:1103 (`pb_bc_put(
  0xD0000028, md->virtual)`); map_io = omap4_map_io (non-NULL —
  board-generic.c:272). Not-resolvable-offline flag: ring2/3 counts were
  in NO dump → the readback battery was widened (see below).
- The designed (NOT queued) next instrument: a raw-store breadcrumb
  inside early_write to split "not called" from "not flushed".

## TASK-006 — the W-101 preflight audit (GO, no fixes)

- Full register/liveness/label/encoding/directive audit — all 12 directive
  items satisfied; 7 benign deviations documented. Its one blind spot
  (movw/movt "encodable" — not under the ACTUAL -Wa,-march=armv6k) = what
  the build caught; recorded as the POST-AUDIT FIX note in the task file.

## W-101 r1 (fired 20:30:32Z, wrapper exit 20:36:59Z)

- Full record: docs/03 + ~/agent-runs/w101-run1-record.md. Bottled: the
  pass RAN (bc[16] 0→272, live-watched); death at the PB-ADJ#1 record's
  CR→LF gap (bc[1]=133); ring 791; no BUG/WARN; ring2=791/791;
  ring3.count=23832 (not run-scoped — parked). LED 5/24/45/113 — deltas
  19/21/68 (the 7th 68 s).

## W-101 r2 (fired 20:51:50Z, wrapper exit 20:56:02Z)

- Full record: docs/03 + ~/agent-runs/w101-run2-record.md. Bottled:
  ★ THE FIRST BUCKET-FLIP DRAW (placement 0xaaf00000 → kernel base
  0xa8000000; bc[6]=0xe8000000; the FDT trims the bank to
  a8000000+0x18000000); the stubs-vs-variable split-brain (W-32c forces
  the build constant over the true runtime delta); DEEP — map_lowmem +
  the W-94 memblock snapshot captured live (m0=a8000000+0x17e00000,
  sentinels intact); death at the PB-CMA record's CR→LF gap (bc[1]=167);
  ring 1231; no BUG. LED 5/12/31/99 — deltas 7/19/68 (the 8th 68 s).
- **THE CR→LF MICRO-SIGNATURE (new, 3 datapoints):** W-99 r1, W-101 r1,
  W-101 r2 all died in the 1-char window after a record's final '\r', at
  three different depths. Candidate: the first console-adjacent code after
  a completed write. OPEN — carried to session 18.

## THE NEW INSTRUMENT LESSONS (readback discipline, session 17)

- **ring2 AGREES with ring1** (both sanitized at arm; 791/791 and
  1231/1231) — a real cross-channel for "did the char stream stop".
- **ring3.count is NOT run-scoped** (23,832 → 24,272 across two runs) —
  it accumulates; do NOT use it as a per-run channel until the arm-time
  sanitize list is extended (patch candidate). ring3.idx = early_write's
  mirror cursor at the last COMPLETED chunk (its lag vs ring1's count =
  the in-flight chunk at death).
- The standard post-run battery now INCLUDES `memdump3 90000080 0x8` +
  `memdump3 94000080 0x8` (manual reads; observation-only).
- **The decode-offset bug (found + fixed):** rebuilding ring text from
  words[2:] puts the text at raw+0x78 (not 0x80 — the section starts at
  base+0x80, words[0..1] consume count/idx, text at base+0x100 ⇒
  0x100-0x88 = 0x78). The 8-byte slip eats the head and pulls RESIDUE
  into the tail (it made r1 look like a "PB-MEM mid-record" death; corrected
  to the PB-ADJ CR→LF gap). Generic decoder:
  `~/agent-runs/w101-artifacts/decode_readbacks_generic.py <raw> <prefix>`.
  Skill updated.
- The live poll's freshness proofs worked beautifully: the arm-time
  sanitize (0x488→0), bc[16] 0→272, the nonce handoff (arm → pre-jump →
  post-run). Watch those transitions rather than trusting any single read.

## THE WRAP (final state)

- **W-101 (#162) = BUILT, VERIFIED, COMMITTED (c1340b5), FLOWN ×2.**
  The pass runs live (272 invs). r1 = early CR-LF death; r2 = bucket-flip
  deep-ish CR-LF death. The class-vs-dice question OPEN (N=2).
- TASK-005 + TASK-006 = accepted with root reviews (records updated).
- Artifacts: ~/agent-runs/w101-* (2 records, 2 led files, the raw
  batteries, the generic decoder, w101-artifacts/ with the pack chain +
  SHA256SUMS + expected/compare-encodings tooling).
- All committed + pushed; BOOTSTRAP_SESSION_18 written (with the user's
  relay note); the HANDOFF pointers moved.

## OPEN / NEXT (session 18)

1. **W-101 draws r3+** — the class-vs-dice question; watch the ring tails
   for the CR→LF signature on every decode; N≥2-3 more draws.
2. **The bucket-flip decision:** payload guard candidate (forbid
   placements ≥0xa8000000 — today's guard only reserves [a0000000,
   a1000000)); the W-32c "runtime delta vs build constant" question
   (a flipped draw force-clobbers the variable — design note).
3. **The CR→LF signature audit** (offline: what runs immediately after a
   console write completes — early_write/pb_flush_rings/printk plumbing).
4. **TASK-005 follow-ups:** the early_write breadcrumb instrument
   (designed, not queued); the ring3 sanitize fix; the F1 sub-mechanism
   (the console-lock candidate).
5. **The strategic fronts (unchanged):** the 126-family wedge proper
   (the modal death — r2 died at its position too); the CPU1 release;
   the 185-path/initcall era.
