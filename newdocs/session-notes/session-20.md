# Session 20 Notes (2026-10-06 — the W-104 redesign + TASK-013 + build #165 + F1)

## THE SESSION'S SHAPE

Boot from BOOTSTRAP_SESSION_20 (read order done; door verified live; ledger
verified: kernel #163 / gated payload dcba1545). ALL of queue item 1 worked
end-to-end in one session: the W-104 redesign (the reset-safe neutralization)
→ relay review (session-19: GO + folds) → TASK-013 preflight (GO-WITH-FIXES,
folded) → build #165 → rule-16 → the user-gated F1 flight → **CLEAN
RECOVERY** → the run record. Also: the USB calibration answered by the user
and folded (r1 record + skill + s20 bootstrap, session-19 commit e358083);
the W-103 r1 record corrected twice under relay (commits 306a3c2, e358083 —
the [38s] frame directly carries bc[11]=0x5A52A108: block success directly
observed, not inferred); the wrap.

## THE RELAY ROUNDS (session-19)

- **Design review**: GO + 4 folds (the corrected-record citation; the
  stale-residue benignity sentence; PRM_RSTST promoted to
  preflight-MANDATORY; the F2-ordering flag) + the calibration fold.
- **Pre-spawn review of TASK-013**: GO + 5 additive folds (memcmp length
  sources; the TASK-007-style register-liveness table; the RSTST sticky-bit
  PRE/POST pair + never-W1C; the QNX-RE sweep for NS-read evidence; the
  expected USB/LED outcome channels) — all applied.
- **The F1-verdict cross-check: requested at wrap — ANSWER PENDING** (fold
  into session 21; it gates the F2/F3 ladder decision).
- Precedent: live frames are ground truth — the [38s] understatement + the
  STEP_STUB=11 / bc[1]=0x31=49(hex) nit were both settled by python
  tabulation of the raw logs.

## THE W-104 DESIGN (route-(a) stage 1 redesign; ~/agent-runs/W-104-design.md)

The r1 finding (a persistent A08 diversion stalls the reset cycle) → the
architecture = TRANSIENT NEUTRALIZATION + PRE-KERNEL RESTORE: the W-103
instrument (blob + repoint) stays, and the continuation (stub3.S cont_start,
IRAM 0x40308000 — the last payload code before the kernel) restores
CPU1_WAKEUP_NS_PA_ADDR = 0x4A326B00 iff params[4] (0x40309810) equals that
canonical value — armed ONLY by the --sarrep block (normal --l2on runs skip;
no SAR touch). The abort paths restore A08 (forensic value kept in bc[11] +
print). Edits: stub3.S (+8 insns + 2 pool words, delta 40 B; pool rides the
in-range .ltorg) + qnx2linux.c (the arm+verify + abort hygiene). The flight
ladder: F1 (restore, no release) → F1b (dummy-repoint control, on a dark) →
F2 (release; the §8 conflict: NEITHER A08 state is reset-safe for a released
CPU1 → may need F3-first) → F3 (kernel-side re-hold).

## TASK-013 (the preflight) + the review

GO-WITH-FIXES: the cont edit assembles/compiles clean and verbatim (8 insns,
pool in-range, delta 112→152 B); the census (single params[4] writer; two
runtime-length cont copiers; both abort paths disarm); PRM_RSTST = 0x4A307B04
(base+DEVICE_INST+RSTST; bits: COLD=0, WARM_SW=1, MPU_WDT=3, EXTERNAL=5); NO
offline NS-read evidence (device-verify). Root review: accepted; new fact =
**the payload LINK embeds a ~6-byte build-varying stamp** (3 builds: same
size, same code, differing sha) — rule-16 for payloads = size + disasm + byte
patterns, never cross-build sha.

## BUILD #165 (the payload)

Edited kexec/stub3.S (+ the stale-note comment refresh) + kexec/qnx2linux.c;
build rc=0; **qnx2linux 29,108 B sha 08fc7bbf**; byte-identical to the
audited scratch build except the 6-byte stamp. Rule-16: the 8-insn blob +
pools present; linked cont disasm exact; strings in. Backups:
w104-artifacts/{qnx2linux.pre-w104 (28,892 dcba1545), qnx2linux.w104}. Commit
1cd8ba7.

## ★ F1 (the flight; record: ~/agent-runs/w104-run1-record.md)

Fired 23:09:07Z (`PAYLOAD_MODE=--sarrep`); block verified LIVE ([26s]
bc[11]=0x5A52A108); jumped ≈+29 s; **the WDT reset FIRED AND COMPLETED**:
reboot-wait 163 s, total 4 m 07 s, off→red = 68 s (the band's 13th), host
USB chain normal, NO button. Death: **the 126-family wedge PROPER, 3rd
instance** (trio 126/0x17e/0x27e; inside early_fixmap_shutdown; mmu.c:2060)
— a FLIPPED-bucket draw (aac00000; 3rd flipped ever) with a **NEW
CMA-reserve-failure sub-shape** (`Not enough slots` / `Failed to reserve
16 MiB` → empty dma_mmu_remap → silent dma_contiguous_remap; console 1181
chars ending at PB-ADJ#2 — no PB-CMA record; r2's = 1317). The fixup
instrument PASSED again (bc[16]=272 + bc[17]=2; 2nd healthy). Cont-ran chain:
kernel entered through bx r9 + the c0de C-world writers + memcmp-verified
cont ⇒ the restore store executed. Post-run: A08 = 0x4a326b00 (restore-default
no-op); **the park blob SURVIVED at 40309A00** (e320f002/eafffffd — contra
r1's post-button "cleared" reading); RSTST 0→0 inconclusive; A0C stable.

**READ: with the single-variable cont-restore in place, the r1 stall did not
reproduce — restore-before-reset VALIDATED at N=1** (the per-draw lottery
caveat stands; an F1-repeat would strengthen).

## NEW DATUMS / N-COUNTS

- off→red band: 13th instance (68); reboot-wait 163 s; red→network-back 145 s.
- 126-family proper: 3rd (W-101 r3, W-102 r2, W-104 r1); flipped draws: 3rd;
  healthy fixup: 2nd; CMA-failure sub-shape: 1st record.
- The blob survived a WDT cycle (qualifies "IRAM cleared by resets" to the
  button/power path); PRM_RSTST readable from NS; the payload build-stamp.

## OPEN / NEXT (for session 21)

1. **The relay cross-check of the F1 verdict** → then the **F2/F3 ladder
   decision** (the §8 conflict + the blob-survival datum + the wake-marker
   design; consider F3-first ordering).
2. F1-repeat for N=2 (a second --sarrep flight; cheap strengthening).
3. Draws (--l2on) available for spectrum (W-102 r3+).
4. Watch the CMA-failure sub-shape; the 126-family tracking continues.
5. Parked smalls: bc[27]=0xf5000002 (now tied to the empty-remap path —
   datum recorded); TASK-005 follow-ups.
6. Strategic: F2/F3 (the CPU1 release done right), the 126-family wedge, the
   185-path.

## THE WRAP

- Docs: docs/03 (the W-104 F1 note), PROJECT_STATE (the SESSION-20 block),
  KNOWN_ISSUES (the session-20 header), HANDOFF pointers. Commits: 1cd8ba7 +
  the wrap commit. BOOTSTRAP_SESSION_21 written.
- PENDING AT WRAP: the relay F1-verdict cross-check (fold into session 21).
