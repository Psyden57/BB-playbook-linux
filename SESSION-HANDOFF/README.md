# SESSION-HANDOFF — status index

These are the per-session handoff/record documents from the pre-repo era
(sessions 1-6). They are **kept as evidence**: the `newdocs/session-notes/`
files are the backfilled synthesis, and these are the primary documents
those notes cite and condense. Nothing here is updated going forward —
corrections to their content live in `newdocs/contradictions/` and dated
erratum markers inside the files themselves.

| File | What it is | Where its content lives on |
|------|------------|----------------------------|
| `HANDOFF_2026-09-01_next-session.md` | Session-1→2-era handoff: M=1 wall solved, early-C wedge, PL310 secure-filtering rules, monitor-RE task list, the bootstrap prompt | `session-notes/session-01.md` (errata), `docs/03` |
| `SESSION-RECORD_2026-09-01_ring-map-fix_early-C-wedge.md` | The ring-map shift-bug fix record (the M=1 unlock) | `session-notes/session-04.md`, `docs/03` |
| `HANDOFF_2026-09-02_late_171-wall_new-observability.md` | Session-5's handoff: L2-disable campaign, monitor service table, run map 23-32, the "171 wall" framing | `session-notes/session-05.md`, `docs/03` |
| `SESSION-RECORD_2026-09-02_L2-confirmed_monitor-RE.md` | The L2-confirmation + monitor-RE session record | `session-notes/session-05.md` |
| `HANDOFF_2026-09-02_night_SMC-flush-pv-wall.md` | Session-6's handoff: console breakthrough, the (since-revised) corruption root cause, the monitor-RE task that session 7 closed | `session-notes/session-06.md`, `docs/03`; note its "corruption root cause" and "RE trustzone-omap4" framing is superseded (`newdocs/contradictions/`) |
| `BOOTSTRAP_SESSION_7.md` | Session 7's bootstrap prompt (the state summary this repo's newdocs replaced) | `newdocs/PROJECT_STATE.md`, `session-notes/session-07.md` |
| `BOOTSTRAP_SESSION_8.md` | Session 8's bootstrap prompt (the zImage-path pivot, the fixup wall) | `session-notes/session-08.md` |
| `BOOTSTRAP_SESSION_9.md` | Session 9's bootstrap prompt (the iotable_init front) | `session-notes/session-09.md` |
| `BOOTSTRAP_SESSION_10.md` | **Session 10's bootstrap prompt (the session-9 wrap: the pv fix, the stale pgd pair, W-39 = the first task)** — the LIVE handoff; read it first | `newdocs/PROJECT_STATE.md`, `session-notes/session-09.md` |
| `ring3-recovered-log-2026-09-02.txt` | **UNIQUE RAW EVIDENCE** — run 23's kernel console, recovered from the (since-decommissioned) monitor UART3 capture. The primary source for the console-death analyses and the 171-era matrix. Irreplaceable. | `session-notes/session-05/06.md` analyses; `docs/03` |

Note: this folder's name predates the repo. The handoffs up to
`HANDOFF_2026-09-02_*` are evidence (not updated); the `BOOTSTRAP_SESSION_NN.md`
files are the live per-session start prompts — the latest one
(BOOTSTRAP_SESSION_10.md) carries the current read order and state.
The live handoff mechanism is `newdocs/HANDOFF.md`.
