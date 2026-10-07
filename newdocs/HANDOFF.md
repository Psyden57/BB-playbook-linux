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

- Latest session notes: [session-notes/session-21.md](session-notes/session-21.md)
  — the (M) study (TASK-014, accepted/cross-checked) + the F2 design +
  TASK-015 preflight + ★ F2 r1: the SMC-clobber find (the payload SIGSEGV'd
  one instruction after a working 0x103; the monitor destroys r4-r11) + the
  release's no-[A08]-landing datum + fix #167 + the accidental clean cycle
- **Latest bootstrap: `SESSION-HANDOFF/BOOTSTRAP_SESSION_22.md` — READ IT
  FIRST** (the v4 read order, the session-21 state, the #167 ledger + the
  crash forensics, the F2-r2 first-action queue; the user's notes: relay =
  question-driven with the aged sessions; the user owns wrap timing)
- Next action: **F2 r2 — the re-fly on #167 (user-gated)**; capture the
  bc-page forensics before firing; then the WDT-window calibration thread;
  the no-landing datum feeds F3. HARD GATE unchanged: no A08-touching flight
  without the re-hold/verify discipline. Draws remain available (--l2on);
  the 126-family + the strategic fronts unchanged.
- Local-only assets and why: [SETUP.md](SETUP.md) §6
