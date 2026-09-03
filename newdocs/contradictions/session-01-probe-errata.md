# Session-1 probe results vs later sessions (errata from the original session)

Session 1 (2026-08-30) was the first session with device access. Several of
its monitor/watchdog conclusions were later corrected. Recorded here so the
old claims (which still stand in the deep docs with erratum markers) are not
mistaken for current knowledge. Full narrative:
[session-notes/session-01.md](../session-notes/session-01.md) ERRATA section.

## 1. The monitor probe FAILs were call-shape artifacts

**Session 1 claim**: "0x103/0x104/0x105 FAIL; 0x101 crashes/hangs the
monitor; the secure-side L2 path is exhausted; the auxcoreboot services are
absent."

**Mechanism of the error**: every session-1 probe went through the
`/dev/trustzone` raw-SMC devctl (0xC0280501), which marshals the service id
into **r0** and hardcodes **r12 = 0**. That shape works for the driver's own
crypto/KDS set (the monitor's SMC#1 crypto entry is r0-addressed) but is
meaningless for the TI HAL services, which are **r12-addressed on SMC #0**.
A "0x103 probe" was therefore an attempt to call service 0 (or garbage) —
the FAILs/crashes say nothing about 0x103/0x104/0x105's existence.

**Current knowledge (sessions 6-7)**: the C-flow `mon_call()` shape (service
in r12, args r0/r1, r3 = param block `{count, params...}` for 0x101) works;
**0x100-0x113 all exist and work — 0x101 (L2 clean+invalidate by PA) is the
payload's L2 flush mechanism**, and 0x102 (L2X0 CTRL) is the L2-disable path.

## 2. WDT2 disable sequence

**Session 1 claim** (still in PLAYBOOK-REFERENCE §1.10 and SESSION-HANDOFF
Rounds 3/4): "disable = 0xDDDD→0x0000" — inferred from the observed WSPR
state 0x4444 and an assumed TI generation split.

**Correction (session 7, grepped from the kernel's omap_wdt.c)**:
enable = 0xBBBB→0x4444, **disable = 0xAAAA→0x5555** (each write followed by
a WWPS @+0x34 pending poll). The 0xDDDD/0x0000 pair has no source. Session
7 also implemented the full lifecycle in the payload (arm on setup, kick =
TGR complement, disarm on aborts).

## 3. Service 0x108 naming

**Session 1 interpretation**: "0x108 = context saved / arm SAR wake path"
(from devpm's `wait_for_interrupt` flow position).

**Correction (session 7)**: 0x108 = **SCU_PWR / omap4 suspend** — the exact
mainline `sleep44xx.S` shape (3 call sites in devpm). The call's role in the
CPU1-offline flow is real; the name came from guessing the flow's purpose.

## 4. Related already-recorded corrections (cross-references)

- "WDT2 window = 15 s" (from `wdtkick -t 15000`) → 58.6 s measured live
  (session 5); see session-02.md ANALYSES #2 and HANDOFF_2026-09-02_late.
- "The frozen-at-70 runs were XN" → the section-alias offset mismatch
  (SESSION-HANDOFF §line-322, `buf_placement_bad`); XN on data pages is
  real but was not the freeze cause (PROT_EXEC had already been added).
- "No L2 tag/data-latency SMC service exists" → confirmed by session 7's
  complete table (unchanged), but the neighboring "the whole monitor route
  is exhausted" conclusion was wrong per item 1.
