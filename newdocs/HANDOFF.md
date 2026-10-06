# Multi-Session Handoff Protocol

This directory (`newdocs/`) is the persistent source of truth. Later
sessions (human or agent) must be able to continue from it without the
original conversation history.

## Read order for a new session

1. [PROJECT_OVERVIEW.md](PROJECT_OVERVIEW.md) — what/why.
2. [PROJECT_STATE.md](PROJECT_STATE.md) — current technical state; **wins
   over older docs where they disagree**.
3. [ARCHITECTURE.md](ARCHITECTURE.md) — components and channels.
4. **SAFETY-CRITICAL**: [../PLAYBOOK-REFERENCE.md](../PLAYBOOK-REFERENCE.md)
   §5 (NVRAM) and §8 (recovery plans) — **NVRAM and RPMB interactions brick
   units irrecoverably** (two units of the wider research effort are already
   bricked this way). The kexec work stays in QNX userland + DRAM and must
   never go near either. Also §1.10 (watchdog), and the README safety model.
5. [DEVELOPMENT.md](DEVELOPMENT.md) — the daily loop + rules.
6. [COMMANDS.md](COMMANDS.md) — the complete command reference (device,
   debug loop, capstone RE, patch snapshot, git).
7. The latest `session-notes/session-NN.md` — freshest knowledge.
8. [contradictions/](contradictions/) — what is genuinely unresolved.
9. Deep details on demand: `docs/00-08`, `docs/03` (run log), root *.md.

## What each session must do

1. **Before device work**: read the files above. Never re-test closed
   dead-ends (they are listed in docs/03 + KNOWN_ISSUES). Never violate the
   hardware rules (docs/06 + ARCHITECTURE.md "NS access rules").
2. **Ask the user before every device run.** The user hard-reboots, charges
   the device, and video-records LED timings.
3. **After every run**: append to `docs/03_DEBUGGING_SESSIONS.md`, update
   `PROJECT_STATE.md`, write `session-notes/session-NN.md`.
4. **Do not silently overwrite** an earlier session's interpretation. If
   evidence contradicts it, record the contradiction in
   `contradictions/` and, if the evidence is conclusive, update the
   affected file AND leave a dated note saying what changed and why.
5. **Verify claims against the codebase** — grep the actual source. Do not
   rely on recalled file contents (this rule exists because a session
   nearly chased a phantom bug on recalled pseudo-source).
6. Keep secrets out: `rsa`, device identifiers (PIN/BSN/serials are
   redacted in the repo — do not re-add them).

## Current handoff state

- Latest session notes: [session-notes/session-20.md](session-notes/session-20.md)
  — the W-104 redesign (the reset-safe neutralization: transient diversion
  + the cont pre-kernel restore) + TASK-013 preflight + build #165 + ★ F1:
  CLEAN RECOVERY — the W-103 r1 stall did NOT reproduce with the restore in
  place (restore-before-reset validated at N=1) + the wrap
- **Latest bootstrap: `SESSION-HANDOFF/BOOTSTRAP_SESSION_21.md` — READ IT
  FIRST** (the tightened read order, the session-20 state, the W-104
  instrument + the F1 records, the pending relay cross-check, the F2/F3
  ladder queue, and the user's note: **you can ask the previous sessions'
  agents questions** — the user relays them)
- Next action: the relay cross-check of the F1 verdict → then the F2/F3
  ladder decision (the release flight done reset-safely — see the design
  §8); an F1-repeat for N=2 is optional/cheap; draws r3+ remain available
  (--l2on is reset-safe); the 126-family (with the new CMA-failure
  sub-shape) + the strategic fronts unchanged
- Local-only assets and why: [SETUP.md](SETUP.md) §6
