# Session 24 Notes (2026-10-09/11 — THE WEDGE ARC CLOSED: W-107 FLOWN, THE PARKED-ALIVE RELEASE LANDED, AND THE PROJECT'S DEEPEST PARKED-ALIVE DRAW)

## THE SESSION'S SHAPE

Boot from BOOTSTRAP_SESSION_24. The session's whole arc = **W-107's flight** —
and the wall the preflight missed (the pre-SMC clean block) that had to be
crossed first. Sequence: **W-107 r1/r2** (the same bc=51 death, N=2) → the
subagent route RESOLVED (the user: it was a config issue — spawns use the ROOT's
model/provider, stepfun/step-5-preview-free via kilo, `--reasoning high`
retained; the skill still carries the old constant, DELIBERATELY) → **W-107d**
(the no-jump control: gating the GTC, N=3 — the sweep block is the killer) →
**W-107e** (the instrument: **bc[20]=0x100 = the pre-SMC clean's globals loop,
page 0, inside `l2c_ns_clean_range()`** — candidate A falsified, the kernel calls
with the GTC off are fine) → **W-107h** (the bypass, EVIDENCED confound-free:
`mon_call(0x102)` is in `g_l2on`'s else branch) → ★★★ **W-107 PARKJUMP r1 = THE
PARKED-ALIVE LANDING** (bc[31]=0x5A52F2F2, CPU1 executed the blob, the kernel
booted) → draw 2 (126) → draw 3 (142) → the W-107i slot census (★ the kernel
WRITES bc[31]: the W-94 snapshot) → build #177 → draw 4 (bc[1]=111 =
setup_arch returned — the family's deepest). Ten flights, ten clean recoveries,
zero writes in every recovery, no release-class casualty.

## THE ARC'S GOAL STATE — ACHIEVED (draw 1)

**W-107 PARKJUMP r1 (--parkjump, #176)** = **THE PARKED-ALIVE RELEASE, LANDED**:
`bc[31] = 0x5A52F2F2` (the blob v2's own store — the only writer of that value)
⇒ **CPU1 executed the parked blob at 0x40309A00**, the first landing since W-106
and the first ever in the kernel era; the kernel-era slot writes (bc[22]=
0xa3703348, bc[23]=0xa0008000, bc[1]=142) ⇒ the kernel booted with CPU1
alive-and-spinning; the cont's `[A08]`-restore landed (canonical post-run); the
release class engaged and the worst-recorded shape did NOT occur
(reset→network-back ≈ 203 s, ZERO writes).

## THE WALL WE CROSSED (the four-death instrument chain)

The blocker was NOT the release — it was a pre-existing payload killer:
- **W-107 r1/r2** died at **bc=51** in the `51→48` window (bc[29]=8192 chunks =
  the whole-DRAM sweep COMPLETED; bc[19]=0).
- **W-107d** (the no-jump control) reproduced it (N=3) with the jump, kernel and
  release ALL removed ⇒ **the window kills on its own**.
- **W-107e** instrumented the window: **bc[20]=0x100 = the pre-SMC clean's
  GLOBALS loop, page 0, inside `l2c_ns_clean_range(gpa & ~0xFFF, 0x1000)` with
  the GTC off** — the two `mem_offset64()` calls RETURNED (candidate A, the prime
  suspect, falsified).
- **W-107h** gated the clean off under the park modes (the confound assessed
  EVIDENCED: `mon_call(0x102)` is in `g_l2on`'s else branch ⇒ the clean protects
  a path these flights never take). The skip gate's marker bc[20]=0xF0 proved it.
- **The TASK-027 preflight's F1 fold** (the GTC re-enable, the do_l2test
  phase-D shape) — the family-first "GTC off→on + return to QNX" — FLOWN and
  benign on W-107h.

## THE SPECTRUM (parked-alive, N=4)

| draw | placement | bc[1] | family | landing |
|---|---|---|---|---|
| 1 | 0xa3200000 | 142 | the placement/fixup class | PROVEN (marker, kicker-era readable) |
| 2 | 0xa3700000 | 126 | the SCU/TLB family | OPEN (bc[31]=1 = the W-94 snapshot, see below) |
| 3 | 0xa1f00000 | 142 | the placement/fixup class | PROVEN |
| 4 | (a 4th) | **111** | **past setup_arch — the family's deepest** | uninformative (the residue flaw) |

**The prediction HOLDS so far**: 3 of 4 draws are NOT the 126 family. ★ the
era's overall depth record stays with W-100 r3 (bc[1]=184/185, the initcall loop,
the held shape) — draw 4 is the first parked-alive run to clear setup_arch and
~73 markers short of that record.

## ★ THE W-107i CENSUS — THE KERNEL WRITES bc[31] (the last draw-2 puzzle)

`pb_bc_put(0xD000007C, memblock.memory.cnt)` (kernel patch :1607). ⇒ draw 2's
bc[31]=0x1 is the W-94 SNAPSHOT value (1 memory region), not the no-landing
init ⇒ draw 2's kernel reached memblock init ⇒ its landing question is
**RE-OPENED** (the landing could have happened and been overwritten). Draws 1/3's
markers survived because those kernels died at 142 BEFORE the snapshot — depth,
not sparing. #177 relocates the durable marker to **0x9FE000A4** (never
kernel-written): 0x5A52F7F2 landed / 0x5A52F7F1 expired, with bc[31] demoted to a
DEPTH channel.

## THE INSTRUMENT'S REALIZED FLAW (W-107j, one line either way)

`0x9FE000A4` is not cleared by a warm reset and a repeat landing writes the SAME
value ⇒ **uninformative on repeat landings** (draw 4 hit exactly this; predicted
at pre-flight, recorded). **W-107j**: the cont writes a per-run nonce into
0x9FE000A8 alongside the marker, OR the C block zeros 0x9FE000A4 at setup.

## THE USER'S THREE CORRECTIONS (folded, and the pattern named)

1. **"~25 min dark"** — asserted without checking the clock; the real window was
   ~8 min. 2. **The flags-in-DRAM hex ordering** (twice): the flags live in IRAM
   (0x8051…), not the DRAM sweep's range. 3. **"the project's deepest draw"** —
   the era's record is bc[1]=184/185 (W-100 r3); draw 4 is the family's deepest.
   **The pattern: superlatives/durations asserted from a narrow window instead of
   the repo's own records. The fix: claim the scoped truth and cite the record
   beside it.** All three are retracted in-place in the run records + docs/03.

## THE SUBAGENT ROUTE (restored)

Per the user: it was a config issue, and subagents now spawn with the ROOT's
model/provider (stepfun/step-5-preview-free via kilo, `--reasoning high`
RETAINED). The skill's `deepseek/deepseek-v4.1-flash` constant is left as-is
DELIBERATELY (the change is temporary — the user's instruction). TASK-027 ran on
it and caught the F1 zombie-class defect (the root's design was wrong; the
subagent's census was right) before the run rate-limited (429) mid-report — the
findings were harvested from its log and root re-verified every claim.

## LED LEDGER (the parked-alive flights)

blue ~+4-5 s → magenta +15-24 s (d4 = +15, the family's fastest) → **off = the
jump, +36-48 s** (d4 = +36) → red ~+103-116 s. **off→red = 67-68 s = the band,
on every jump flight** (my "deeper draw ⇒ longer band" speculation is
DISCONFIRMED — the WDT anchor is the kick cadence + 58.6 s, not the kernel's
runtime). The OFF is FINER than the wrapper's "ssh gone" (t=45-50 s = the 4-miss
declaration, since the poller polls every 3 s). The lit-freeze variants belong to
the payload-death shape; the parked-alive flights are all normal-shape.

## THE OPEN / NEXT (for session 25)

1. **Draw 5+ for N≥5** (the spectrum; the project's depth record 184/185 is the
   target to match) — OR **W-107j first** (the one-line fresh-slot fix makes every
   subsequent draw's landing question decidable).
2. The draw-2 question (126 + the landing OPEN): with W-107j's marker, a 126 draw
   + landed-vs-expired settles the prediction outright either way.
3. The pre-SMC-clean mechanism memo (the exact sub-position: the line-op loop vs
   the sync poll vs the return — W-107g if the memo needs it).
4. The W-107h gap (the invisible mid-flight window on the 8-min dark) — the
   marker's absence there is a known instrument limitation, not a mystery to
   re-chase.
5. Smalls: the 215 (poll-expired) path is still unflown; `mon_call_full`'s
   {r4,r5,r7,lr} stack audit; the docs/06 CRR up-counter (landed).
6. The strategy threads (the 185-path; the F3-family; the memory tree).

## THE WRAP

- Builds: **#174 (W-107d, the no-jump control; 40,489 B sha bd43ef2c12606543)**,
  **#175 (W-107e, the instrument; 40,869 B sha 1151dac3b223682)**,
  **#176 (W-107h, the bypass; 40,869 B sha bd43ef2c12606543)**,
  **#177 (W-107i, the durable marker; 40,917 B sha e93185babfbbc704)**.
  Kernel #163 UNCHANGED throughout (sha ee303813…, 5,223,233 B).
- Designs: W-107d/e/h/i + the TASK-027 preflight (GO-WITH-FIXES, F1 folded).
- Records: w107-run1/run2, w107d-run1, w107e-run1, w107h-run1 (with the big
  retraction), w107-parkjump-draw1..4 (each with the LED fold + the draw-3/
  draw-4 corrections), plus the build records.
- Commits: the W-107d/e/h builds + runs, the W-107 PARKJUMP draws 1-4, the
  W-107i design/build, the two corrections, and the LED folds.
- **The backups**: `~/agent-runs/w104-artifacts/qnx2linux.pre-w107d/-e/-h/-i`.
- Staged for session 25: **#177 is the shipped flight payload**; the recovery
  scripts `w107-parkjump-draw5-recovery.sh` (adapt from draw 4's — and NOTE the
  draw-4 script's sed mangled its DURABLE_MARKER line: rebuild it from
  `w107-parkjump-recovery.sh` with a fresh OUT path + the `memdump3 9FE000A0 8`
  line, or the marker section silently skips).
