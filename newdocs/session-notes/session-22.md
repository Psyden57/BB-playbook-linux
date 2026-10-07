# Session 22 Notes (2026-10-07 — F2 r2 → TASK-016/017 → W-105 (the release-on-live wedge) → the ROUND-3 archaeology → W-106 ★ THE [A08] LANDING)

## THE SESSION'S SHAPE

Boot from BOOTSTRAP_SESSION_22 (read order v4; the VM had freshly rebooted — keeper restarted; the device was plugged in mid-boot). Then: **F2 r2** (the first action; the re-fly on #167) → RELEASE-NEVER-TOOK + a clean cycle (TASK-016 = the reader-delegate record) → **TASK-017** (the release-mechanism re-audit) → **W-105** (`--engage`, the AUX-armed control — the release-ERA WEDGE) → **the ROUND-3 archaeology** (the warm/cold path correction) → **W-106** (`--sarrel` — ★ THE LANDING). Preflights: TASK-018 (W-105) + TASK-019 (W-106), both GO-WITH-FIXES, all folds landed. Builds: #168 + #169. Then the wrap.

## F2 r2 (W-104 run 2b) — the flight (`w104-run2b-record.md`)

- **RELEASE-NEVER-TOOK** (bc[25]=0x5A52F283; `kexec-f2.log` `CONT seen=0 … ladder=5a52f283`); the 0x103 read never changed (the RSTCTRL-vs-0x103 "contest" — later reclassified as a category error, below); NO SIGSEGV (#167 held; the poll executed); block CONTINUED → jump → **CLEAN cycle**; the draw died **pre-stext** (no kernel bc writer fired; rings 0; ring1 residue = byte-identical F1-era); A08 canonical; recovery = zero writes.
- **The LED reading**: the user's off-stamp (+84) = the RESET-dark, not the jump (the kick+58.6 math); the first "lit-freeze through the jump" variant (flagged N=1).
- **The payload stamp family refined** (builds): cross-build payload deltas = build-id + `.strtab` per-link random temp-object names + input paths + symtab `st_name` drifts — code bytes identical otherwise (rule-16 refinement; code-identical cross-build comparisons valid via the diff-group method).
- Artifacts: `w104-run2b-*` (skeleton/prerun/recovery/led/session/live) + TASK-016 (the delegate record + root review).

## TASK-017 + THE CORRECTION CHAIN

- **TASK-017** (the release-mechanism re-audit): RSTCTRL = a level (Table 4-30; the **1→0 transition** needed — W-105 later exercised it); the proposed AUX_CORE_BOOT_1 release target; the 0x103-vs-RSTCTRL category error (different subsystems); the trampoline disasm (the low-power resume vector — the W-73 shape); the write-echo caveat (RSTCTRL "returns the last write" — not an independent verify).
- **THE WARM-vs-COLD CORRECTION** (post-W-105 + the archaeology; chain = TASK-017 dated note, TASK-014 supra-note, W-105-design §13, w104-run2b-record §12): **the CPU1 WARM release follows `[A08]`** (the restoration chain → the trampoline — "always resumes QNX" when canonical); **AUX_CORE_BOOT_0/1 = the cold-boot pen only**. The AUX-armed shape (W-105) = built on the cold-path model — superseded.

## W-105 (`--engage`) — THE RELEASE-ON-LIVE WEDGE (`w105-record.md`)

- Fired 22:15:40Z. The block executed through the RELEASE (bc[25]=0x5A52F303 + bc[1]=58 = the same statement landed); **the freeze hit at the first post-release kernel entry** (no re-hold; no marker; `/tmp/jump.log` lost to the cycle). **The fresh-kick WDT2 window fired at +111 s** (kick+58.6 — the cleanest window instance; LED off ±1); RED +2 s; **clean cycle**; **A08 never touched (canonical pre/post)**; zero-write recovery; QNX healthy.
- **The archaeology (SESSION-HANDOFF.md:295–313; session-01.md:104)**: hold-alone = survivable (08-30 #1); release-on-live = the recorded danger (08-30 #2: hard crash, no watchdog, manual power-cycle; the "never release without a valid target/restore" lesson); the warm path = `[A08]`.
- Lesson crystallized: **the syscall-free window** (the freeze hit the first post-release KERNEL entry; keep dangerous windows call-free — a minimization, documented caveat: timer ticks).

## W-106 (`--sarrel`) — ★ THE [A08] LANDING (`w106-record.md`)

- Fired 22:42:46Z. The block COMPLETED: hold → repoint `[A08]`→blob → release(+SEV) → **THE MARKER LANDED (`bc[31]=0x5A52F2F2`; `seen=1`; `spinleft=109/10,000,000` ≈ µs — the warm release follows [A08] — MECHANISM PROVEN; first landing ever)** → re-hold → **`[A08]` restored canonical (readback-verified)** → `kexec-w106.log` recorded → **[THE WEDGE fired post-completion]** (`bc_snapshot_file(65)` never landed; `kexec-bc.log` unchanged; `wdt2_disable` never ran) → the still-armed WDT window (kick≈+28–30; fire≈+88–90 = +58.6) → **CLEAN cycle (canonical — the W-103-r1 stall class DEFEATED by the restore ordering)** → the device fully healthy; zero-write recovery.
- **The stale-resume hypothesis = defeated by the diversion** (CPU1 ran the blob, not the stale QNX context). **The residual wedge = the top open problem** (3rd instance; variable timing: W-105 = the first post-release entry; W-106 = ~10 calls later).

## THE OPEN / NEXT (for session 23)

1. **THE WEDGE-MECHANISM STUDY** (top item): why does a live-system release wedge QNX (the yank-class candidates: inter-core state/locks/SCU; reset-adjacent side effects)? Offline-source mining first + a discrimination flight design. **GATES W-107.**
2. **W-107** = the jump-integration (the release in the continue path — gated on 1); then the CPU1-parked-alive TLB test returns to scope.
3. **The WDT-window thread**: the 58.6 s canonical instances ×2 (W-105 +111; W-106 ≈+88–90); the kick-family values (TGR 0x666e readbacks); the counter model.
4. **The LED stamps for W-105/W-106** (ask the user — pending for both records' LED sections).
5. Draws/spectrum; the 126-family; smalls (`mon_call_full` audit; bc[27] datum; TASK-005 follow-ups).
6. The rule-16 stamp-family refinement (documented; see the builds above).

## THE WRAP

- Docs updated: `docs/03` (the W-105 + W-106 run notes), `PROJECT_STATE.md` (the s22 block), `KNOWN_ISSUES.md` (the s22 header), `HANDOFF.md`, this file, `BOOTSTRAP_SESSION_23.md`. Commits: the wrap commit (+ #168 `409e464` / #169 `0b42a9b` this session).
- Skill folds (playbook-dev-orchestration): the warm/cold path + the wedge classes; the syscall-free window; the restore-ordering discipline; the stamp family; the write-echo class.
