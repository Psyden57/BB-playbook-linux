# Session 2 Notes (2026-08-31 daytime — LED RE, the diagnostic-channel build-out, head.S markers 100-108, the first ring capture, the bc=108 ceiling)

Backfilled at session-02 documentation time from the session-2 conversation
context. Session 2 = the daytime of 2026-08-31 (LED-RE.md written 11:35, the
handoff written 16:52; session 3 began the same evening and took this
session's handoff as its starting point). Session 1 (2026-08-30) proved the
T2 jump; session 2 turned T3 from "kernel enters stext, nothing else
observable" into "kernel provably runs to the head.S fixups, its text lands
in a DRAM ring, and the FAN5702 LED narrates the stages" — then hit the
bc=108 ceiling and handed the 108→110 gap to session 3.

Scope caveat: the exact per-run boundaries below are reconstructed from the
session-2 readbacks; two late runs are reported with both of their observed
states because the run-to-run attribution was itself part of the problem
(nondeterminism — see ANALYSES #1).

## THE RUN MAP (session 2, in order)

All runs: uncompressed Image, payload --t3/--probe via the pre-jump.sh
manual cycle, bc readback @0x90000000, LED watched by the user (video).

1. **First kernel entry**: bc=100 (stext) landed; user observed the display
   backlight switch ON at the same moment — the first external (non-RAM)
   confirmation the kernel executes. The kernel then died silently; rings
   held pre-run garbage.
2. **Ring capture attempts all failed** → the ring headers (+0x80/+0x84)
   held 0xAAAAAAAA churn from the QNX reboot; the kernel's first ring index
   read was garbage, so its first `strb` went wild and aborted inside
   printascii before any marker could land. Fix: the payload **sanitizes
   bc[0..5] + all three ring headers at arm time**.
3. **bc=101 persisted across rebuilds** → bisect of omap4bc.S found the
   **ring-VA bug**: the ring write used 0x94000080 — a *physical* address —
   as the MMU-on virtual address. Fix: MMU-state-dependent base (VA
   0xD4000080 when MMU on; raw PA when off), state captured into r4 at
   busyuart entry (see CONFIRMED #4).
4. **Marker 106 added pre-printascii → bc=106, death inside printascii**
   → cont's stack was set while in a mode whose SP is banked (the value
   never reached SP_svc; busyuart's `push/pop {r1-r4}` then used a dead
   stack). Fix: `cps #0x13` FIRST, then load sp=0x89000000
   (stub3.S cont).
5. **Still died at the same point** → second omap4bc.S bug: busyuart
   re-tested MMU state with `teq r4, #0` *after* ring_put's `cmps` had
   clobbered the flags — the UART/ring decision was made on the wrong
   flags. Fix: capture the MMU flag at entry, never re-test (CONFIRMED #4).
6. **bc=101 again** → disassembled head.o and found the next bug class:
   **inline literal pools and the smoke-test string were being EXECUTED as
   instructions** (the assembler placed `.word` pools/string data in the
   instruction stream after the stores). Fix: the pbmark macro pattern —
   branch over the data (`b 99f` / data / `.ltorg` / `99:`), markers limited
   to ARM-immediate-encodable values, pbmark restricted to r0+ip clobbers
   (reorder call sites so it never eats a live argument).
7. **Restructured head.S → bc=103** and **ring1 count=0x14: the smoke-test
   string "PLAYBOOK-HEAD-101\r\n" (19 chars) was captured intact** — the
   first-ever kernel-text capture, proving the whole chain end-to-end:
   head.S → DEBUG_LL → omap4bc ring_put → PL310 clean+inv-by-PA per char →
   WDT2 warm reset → QNX reboot → SSH readback.
8. **Markers 107 (post-fixup_smp) / 108 (post-fixup_pv_table) added →
   bc=102** — first nondeterminism (marker *below* the previously proven
   103 on a superset build).
9. **The r7=4 ghost**: cont read the DTB phys from the enter_stub register
   chain and got 4 — grep of qnx2linux.c showed GCC had reused the pinned
   register as a loop counter. Fix: cont (and the probe's DTB check)
   reads **params[1] @ IRAM 0x40309800+4** instead — the payload-written,
   probe-verified params block is authoritative; the register chain is
   forensic only (bc[4] = the chain's value at probe time). Also `.ltorg`
   added so cont's literal pool lands inside [cont_start, cont_end).
10. **bc=108** (post `__fixup_pv_table`) with **bc[4]=0xa1e377f8 = the
    correct dtb_phys** — the deepest kernel progress of the era; the
    register chain happened to be correct this run AND the kernel passed
    both fixups. Death between 108 and 110 (post-paging_init).
11. **DTB red herring closed** (see CONFIRMED #5): the "garbage" bc[2]
    values are the probe's raw LE loads of the BIG-ENDIAN FDT header fields
    — valid data, not corruption.
12. **Early ring-page mappings added to `__create_page_tables`** (sections
    VA 0xC8800000/0xD0000000/0xD4000000 → PA 0x88000000/0x90000000/
    0x94000000 — deliberately the same VAs the linear map later uses, so
    paging_init's re-map is seamless) + **DTS: no-map removed from all
    bc/ring pages** (a no-map page is outside the linear map, so the
    kernel-proper's first post-paging_init ring write would abort).
13. **Final run**: bc=102 with kernel-written bc[2]=0x60540100 /
    bc[4]=0x9df377f8 — values only the kernel-proper could have written
    (probe- era formats with the runtime dtb_phys) — i.e. the kernel ran
    past the early stores into the 100-108 window again. Nondeterministic
    vs run 10's 108 on a near-identical build. Session 2 ended here and
    wrote the handoff: **kernel reaches 108, dies before 110; gap =
    `__create_page_tables` tail + `__turn_mmu_on` + `__mmap_switched` +
    start_kernel head + setup_arch head + paging_init; prime suspect = the
    new ring-map block** (session 3 exonerated it at store time; session 4
    found the real bug inside it — the shift bug — at translation time).

## CONFIRMED (facts from this context, grep-verified against the tree where noted)

1. **The four DEBUG_LL/head.S assembly traps** (all hit live this session;
   the pbmark/omap4bc idioms exist because of them):
   a. *Inline data executes*: `.word` pools and string literals placed
      after stores run as instructions. Rule: every embedded datum gets
      `b 99f` / datum / `99:` (or a guarded `.ltorg` pool).
   b. *Marker immediates must be ARM-encodable*: `mov r0, #300` does not
      assemble into the pbmark shape; markers stay <256/rotatable.
   c. *pbmark clobbers r0+ip*: at call sites where r0 is live (e.g. the
      printascii string pointer), order the marker so the clobber happens
      after the last use — or the marker silently corrupts the very print
      it advertises.
   d. *SP is banked*: writing sp before `cps #0x13` sets SP_sys, not
      SP_svc — the kernel-mode `push/pop {r1-r4}` in busyuart then walks a
      dead stack. Cont sequence is `cps #0x13` → `ldr sp, =0x89000000`.
2. **The TLBIALL wedge (removed from cont)**: cont's TLBIALL hung the
   machine — on OMAP4 it broadcasts to CPU1, which D2 holds in PRCM warm
   reset and which cannot acknowledge; it was also redundant (the flat
   table is switched wholesale at TTBR0). ARCHITECTURE.md carries the
   one-line rule; the mechanism (CPU1 broadcast) is recorded here.
3. **The params block (IRAM 0x40309800) is authoritative, the register
   chain is forensic**: GCC's register-asm pinning (r7=DTB, r2=...) is NOT
   reliable across builds — r7 was reused as a placement-loop counter
   (probe saw 4), and the chain value changes build-to-build. Anything that
   must survive enter_stub → cont → probe → kernel reads the params block;
   bc[4] exists to *audit* the chain, not to carry it.
4. **omap4bc.S MMU-state discipline**: the MMU-on/off decision must be
   captured ONCE at busyuart entry (into r4) — ring_put's `cmps` destroy
   the flags, and any later `teq` re-test acts on ring flags, not SCTLR.M.
   The ring base is then VA 0xD4000080 (MMU on) or the raw PA (off).
5. **The DTB "corruption" red herring** (grep-verified):
   `arch/arm/kernel/head-common.S:16-18` (and compressed/head.S:15-17)
   define `OF_DT_MAGIC` = 0xd00dfeed under `__ARMEB__` and **0xedfe0dd0
   otherwise** — the LE-read of the big-endian magic. On-disk DTB bytes are
   `d0 0d fe ed`; read as a LE word that is 0xedfe0dd0, so `__vet_atags`
   (head-common.S:49) PASSES with a valid DTB. Same for the totalsize:
   FDT header fields are **big-endian per the FDT spec**; the winchester
   DTB totalsize bytes `[00 01 54 60]` = BE 0x00015460 = 87136 = the exact
   file size. bc[2]=0x60540100 is that BE field read as LE — a validity
   check, never corruption evidence.
6. **bc[12] (0x90000030) has an identified writer** (answers KNOWN_ISSUES
   #4's "no identified writer"): the cont's step-211 marker — stub3.S
   `str r7, [r11, #0x30]` — records the enter_stub r7 chain value, which is
   dtb_phys whenever the register pinning held for that run. It is chain
   forensics, not a fresh- DTB proof (see CONFIRMED #3).
7. **The smoke-capture proof semantics**: ring1 count 0x14 with the smoke
   string intact certifies (a) the kernel executed through the smoke test
   with MMU off, (b) the per-char PL310 CIPA flush works (hot ring lines
   otherwise never evict and die in the reset), (c) DRAM survives the WDT2
   warm reset, (d) the readback path is trustworthy. Any future "ring is
   empty" result on a kernel that reached the smoke test indicts the
   flush/VA path, not the capture concept.
8. **Operational facts locked in this session**: /tmp on the device is
   wiped on every reboot (deploy everything, every cycle); the SSH option
   set (ssh-rsa/hmac-sha1) is required by the device's old dropbear; every
   ssh/scp needs an explicit `cd` (the recurring mistake); jump commands
   get `timeout 30` (pre-120s era) so a hung payload cannot wedge the
   session; the user watches the LED and should be asked before every run;
   failed payload runs **leak their 24 MB buffers by design** (the
   allocator repeats a rejected block if it is freed), and enough leaked
   runs in one boot can starve QNX into a WDT2 death — count leaks, expect
   a reboot every ~10-12 failed placements; one degraded-USB boot occurred
   (RNDIS dead, bogus IP, wifi stuck — fixed only by a power-cycle), so a
   dead RNDIS is not proof of a soft hang.

## ANALYSES (formed this session; dispositions noted)

1. **Nondeterminism in the death point (102/103/108 on near-identical
   builds)** — never explained in-session. Candidate causes recorded: RAM
   churn flipping single bits in the bc page (bc[1] 0x66 vs 0x67 and the
   bc[2] top-byte 0x20-bit delta between runs are both one-bit deltas —
   but neither was confirmed as churn vs a genuine marker), per-run buffer
   placement variance, and build-to-build codegen differences. Disposition:
   superseded by later sessions' placement/corruption work, but the
   methodology rule stands: **cross-check bc[1] against the ring count
   before trusting it** (a bc below proven execution means the channel,
   not the kernel, regressed).
2. **"WDT2 gives a deterministic 15 s kernel window"** — wrong on the
   window: session 5 measured 58.6 s from the live registers. The kick
   discipline built here (kick pre-jump, expect a reset unless something
   services it) was correct and load-bearing.
3. **DTB delivery evidence for the (still open) r2-chain loss**: session 2
   proved (a) the chain can be right at probe time (bc[4]=0xa1e377f8
   exactly matched the placement math), and (b) the vet constant cannot be
   the loss point (CONFIRMED #5). The kernel still ended up DT-less in
   later sessions (W-4/W-5) — so the loss is downstream of the probe, in
   the kernel's own r2 handling, exactly as PROJECT_STATE now says.
4. **LED stage map (designed this session; early stages visually
   confirmed)**: blue = payload armed (via QNX I2C before the jump);
   EN toggles per probe stage (I2C4 is clock-gated at idle and PRCM writes
   are NS-firewalled, so bare-metal code can only toggle GPIO1_13 EN —
   no color control post-jump); display backlight ON ≈ kernel stext
   (bc=100). The 110/111 EN-low/high mappings were designed in but never
   visually confirmed in session 2 (the kernel never got there).

## DEAD ENDS (do not retest)

- **Reading the register chain as truth** (r2/r7 from enter_stub) — GCC
  reuses pinned registers; use the params block.
- **TLBIALL in cont** — wedges on CPU1 broadcast; redundant anyway.
- **I2C control of the FAN5702 from bare metal** — I2C4 is clock-gated at
  idle (access = abort) and the PRCM gate write is NS-firewalled; EN
  toggles only.
- **Debugging via the pre-sanitization rings** — stale 0xAAAAAAAA headers
  make the first capture abort the kernel; the payload must zero the ring
  headers at arm time (later reduced to bc[0..5]+headers — see
  KNOWN_ISSUES #4 for the resulting canary subtleties).

## WHAT SESSION 2 PASSED FORWARD (the bequest session 3 built on)

- The trustworthy diagnostic stack: DSB'd pbmark macros (session 3
  formalized the DSB ordering into pbmark/pbmark3/pbmarkv), sanitized
  rings, the per-char PL310 flush, the LED narration, jump.sh + waitdev,
  the 120 s ssh timeout lineage (session 2 set 30 s; the SIGHUP lesson was
  session 3's).
- The head.S marker ladder 100-108 + the smoke test — sessions 3-7 extended
  it to 84+; the 107/108 duplicates noted in session-03 CONFIRMED #5 come
  from this session's bisect structure.
- The params block as the single source of truth for kernel entry/DTB.
- The bc=108 handoff (dies before 110; ring-map block prime suspect) —
  session 3 took it at face value, exonerated the block at store time, and
  session 4 found the shift bug inside it at translation time.
- The big-endian FDT analysis — the "garbage DTB header" family of red
  herrings should never be re-chased (CONFIRMED #5).
- LED-RE.md (FAN5702 full RE) — D1's "no serial console" decision leans on
  the LED as the only non-RAM channel; every later session's run
  video-recording practice comes from this session's contract with the
  user.
