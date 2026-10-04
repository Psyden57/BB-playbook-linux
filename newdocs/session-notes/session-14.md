# Session 14 Notes (2026-10-04 — the workstation migration + BerryShell-V4 + the device link)

## THE SESSION'S SHAPE

Host migration: WSL2 (Windows) → CachyOS + a qemu Debian 13 VM (the workspace)
driven by a Hermes agent session. Prior work lives in this repo (W-92..W-95 =
the last runs, session 13 era). This session: restore the workspace, restore
the device link through the qconn door, then the W-96 determinism runs
(pending the user's go for device time).

## WORKSPACE RESTORE (done 2026-10-03/04)

- Both toolchains verified on the fresh Debian VM: buildroot `arm-linux-gcc`
  14.3 (kernel builds) and QNX SDP 6.6 `gcc 4.7.3` (payload builds; needed
  `libc6-i386` + `zlib1g:i386` — the host tools are 32-bit).
- Packages installed: `bc, libc6-i386, zlib1g:i386, flex, bison, lz4,
  libncurses-dev, ripgrep` (a fresh Debian 13 lacks kernel-build basics).
- Kernel tree incremental build: OK. **NOTE: the 2026-10-03 sanity build
  relinked `arch/arm/boot/zImage` (compile.h timestamp) — the packed
  `kexec/kernel/zImage` = kernel #159, 5,225,409 B, sha256 `42bf3025…` is
  THE artifact for the next runs. Do NOT run mkkernel.sh before W-96; a
  backup of the packed image + DTB sits in
  `~/.hermes/cache/scratch/w95-artifacts/`.**
- Payload rebuilt from unchanged sources; objdump feature checks pass
  (0x7F0/0x770 sweep sites, WDT2 enable/disable seqs, marker 70).

## THE DEVICE LINK: BerryShell-V4 + the qconn door (new this session)

- PlayBook USB (RNDIS) is passed through to the VM: `enx1674118f9cd9` =
  **169.254.0.2/30** (device = .1). SSH port 22 is CLOSED until the qconn
  door (TCP 4455) authenticates.
- `~/BerryShell-V4.py` (agent-built from BerryShell-V3, protocol layer
  verbatim-verified by the root agent; hold-mode resilience fix by root;
  lives in HOME, not the repo): modes `auth` / `exec` / `hold`.
  - `hold` = the door keeper: handshake + pubkey upload + 4 s heartbeats,
    re-handshakes automatically across device reboots. While it runs,
    plain `ssh`/`scp` (incl. all of jump.sh) work.
  - **Single-session door**: while `hold` runs, `auth`/`exec` stall — use
    plain ssh/scp; auth/exec only when no door is held.
- Verified against the live device: auth (17:30:20Z ✓), hold daemon ✓,
  jump.sh's exact SSH option set ✓, `scp` deploy+verify+rm ✓.
- Device identity (from `uname -a`): QNX 6.6.0, `OMAP4430_Winchester_ES2.2_HS_PVT4_Rev:07`
  — the 64GB kexec test unit. Device clock reads `Nov 16 23:07` = frozen/wrong
  (expected; never trust it).

## FRESH-DEVICE BASELINE (2026-10-04, read-only memdump3)

- bc page (0x90000000) = AA-heavy partially decayed content; **no valid
  magic/step — DRAM retention loss after ~3 weeks unpowered. The machine is
  effectively COLD: no L2 fossils, no valid run state.** (mirror0 also shows
  decayed content, not the 0x46 jump marker.)
- ⇒ W-96's first run = a **cold-machine determinism data point** — expect
  the W-94b-control class behavior unless the artifacts/sweeps say otherwise.
- Raw dumps archived: `~/agent-runs/w96-baseline/` (bc-fresh.txt,
  ring-header.txt, mirror0.txt). `memdump3` was left deployed at the device
  /tmp (wiped per reboot).

## THE NEXT STEP: W-96 (pending device-time go from the user)

`PAYLOAD_MODE=--l2on ./jump.sh zImage` ×2–3, same kernel #159 + same payload,
same procedure as W-95. Readbacks decoded with `~/agent-runs/decode_readback.py`
(rule 13 — python, never eyeballed); capture the full ring window
(`memdump3 88000080 0x4e0`) per run for the slab-era console text.
Question to answer: does the front repeat at 126, or drift with warmth
(the W-94b question, re-asked on a cold machine).

## W-97 RESULTS (the live-poller run, 2026-10-04 ~20:44Z)

**The instrument:** `kexec/poll-jump.sh` (built solo per the user's call):
jump.sh wrapped with a 3 s host-side poller (the bc page 0x80, the WDT2
counter, the deployed sizes at pass 1). Kernel #159 + payload unchanged.

| Thread from W-96 | Answer from the live data |
|---|---|
| (α) deployed shape | `/tmp/zImage` = 5,225,409 = the on-disk #159 EXACTLY — the +0x300 deploy-theory DEAD |
| (β) payload's zlen | (bc[2]-live read = the sweep-era values; the REVERIFY-chain = clean — the read lands post-copy) |
| sweep rate/skip | **bc[29] = 0x2000 = FULL 8192 chunks + 0 timeouts BY t≤7 s** = the sweep ran COMPLETE at ~5-6× the W-91 rate (NOT skipped) — the 23 s blue→magenta = sweep ~6 s + reads ~2 s + the placement's settle sleeps |
| WDT2 window | the counter = WLDR (unarmed) through the whole payload era — the payload's kick arms it right before enter_stub; the down-count = unobservable live (the jump = between 3 s polls); off→red 68 s = still a bootrom-phase question, the poller can't see it pre-arm |
| (γ) r0-vs-bc[14] | THIS run: AGREEMENT — r[0] = 0xa34ee6a8 = kern + APP(#159) EXACTLY (kern = 0xa2f00000 + kern_off 0x100000 = 0xa3000000; placement was NOT 2MB-aligned this run). **W-96 run 2 = the SOLE +0x300 outlier in history** — one-run-deep, unreproduced on a byte-identical artifact; parked as a fluke unless re-seen |

**THE FRONT: bc[1] = 153 WITH THE FULL PAIR (bc[2]=0x163, mirror 0x263 —
153 = `add_static_vm done`, mmu.c:1115 = INSIDE the svm block = a front
value never sampled before; the warm-band distribution = 126, 153, 156
so far, ring 1251 chars again).** The 150-166 svm wall = the next bisect
target, with the ring ALIVE through it (the reads = add ring window to
the poller set for W-98).

- Full records: docs/03 W-97 r1; the live log + readbacks archived
  ~/agent-runs/w97-run1-*.
- The user MISSED recording this run (the session's request stands:
  video on the NEXT runs).

- **Run 1 (cold machine) = 142 + 0x3E7 + ring 0** — the W-84 triad on a
  no-fossil machine. No run state survived the 3-week power-off (the
  decayed-DRAM baseline above = the proof the machine was genuinely
  cold). Cold ≠ lucky.
- **Run 2 (warm) = 126** — parse passed, bc[13]=0xdfbf7000 (the
  126-front signature), **the ring = 1165 chars = THE COMPLETE LOG TO
  THE DEATH**: banner (UTS "#157" built 2026-09-13 00:23 UTC on
  DESKTOP-E596NT4 = the Sep-12-packed artifact ✓; the project's
  "kernel #159" ledger ≠ the UTS build counter — different counters,
  do not reconcile) → PB-ADJ → PB-MEM (mem=1 res=1, m[0]=a0000000+
  0x20000000) → PB-RES r[0]=a34ee9a8+0x15519 (HEX = 87,321 ✓ = the
  correct totalsize — the pointer = dtb_phys + 0x40, the promoted
  lead) → cma: 16 MiB @ 0xbe800000 → PB-ADJ (lowmem=bfe00000) →
  PB-CMA (base/size/va/pv_off/pfn correct) → END. The log's end ==
  marker 126's claim (the first post-CMA TLB op). Sweep tally
  0xBEEF0000 (mism 0, tmo 0); bc[28..31] = sentinels intact + cnt 1/1.
- **Run 3 (warm) = 156** — the W-93 front reproduced. Slot archaeology:
  156's writer = pb_bc_put(0xD0000004) ONLY (no pair) → bc[2]/mirror
  keep the previous PAIRED marker = 127 → the readback shape
  (156, 0x17F, 0x27F) = exactly W-93's. A pair-less marker leaves
  bc[2] standing (the decoder rule — rule 17's finer teeth).
  placement 0xa2400000; ring 1165 again (same log shape); tally clean.

**VERDICT: cold = the triad, deterministic; warm = every run ≥126 (the
sweep floor holds on warm machines), depth draws 126..156 (lottery,
alive but tamed). The ring ending EXACTLY where the marker says = the
strongest marker↔log agreement yet. The +0x40 pointer delta = the
promoted lead (the ATAG-compat path, head-S ~384-460).**

- Full records: docs/03 W-96; runs archived ~/agent-runs/w96-run{1,2,3}*.
- LED timings: held on video by the user (ask when needed for the record).

## W-96 LED TIMELINE (user video, run 1; runs 2/3 = "very similar",
## footage lost to phone storage)

| t | What | Interpretation |
|---|------|----------------|
| 00:00 | jump.sh launched | (the video's t0 = the command) |
| 00:05 | BLUE | the payload's led_blue_qnx at bc~30 ⇒ deploy+slay ≈ 5 s on USB RNDIS |
| 00:28 | MAGENTA | led_color_qnx(0x0A) at bc 42 — AFTER the whole-DRAM sweep + the file reads, BEFORE the placement search/memtest/copies |
| 01:19 | OFF | the kernel era (post-jump the LED goes dark by design) |
| 02:27 | RED | the WDT2 warm reset (reboot) |

**THE ONE TENSION (record, not resolved): blue→magenta = 23 s. W-91's
measured whole-DRAM sweep ≈ 35 s alone (CIPA ≈ 1 μs/op); + reads ≈ 2 s
⇒ expected magenta ≈ +37, observed +23.** Two readings: (a) the sweep
ran ~20 s (≈1.7× faster than W-91's measure — sync-time distributions
run-to-run?), or (b) the sweep fork DIDN'T execute its full range
(a logic path) — the payload's magenta/heartbeats can't discriminate
post-hoc because the bc[29]/bc[30] heartbeat slots are re-used by the
kernel-era capture before readback.

**THE SECOND TENSION: off→red = 68 s — LONGER than one full WDT2
window (~59 s from the last kick, which sits right before enter_stub).
Off = the kernel-era LED event; the WDT2 was armed ~59 s pre-red IF
the kick ran at the jump — the +9-ish s of slack either says the
reboot-to-red LED path adds delay (bootrom/loader phase before red)
or the last kick was earlier than assumed (the placement fought — the
W-61 ladder incl. 2 s settles). NOT resolvable from LEDs alone;
W-97's live-poll covers it: poll `memdump3 4a31402c 4` (the WDT2
counter) alongside the heartbeat slots — the counter names the true
jump moment and the remaining window at every poll.
W-97's first instrument, consolidated: a live poller loop during the
jump (bc-ext slots 90000060/0x20 + WDT2 counter + bc 90000000/8),
t≈+10/+20/+30/+40/+50 s — settles sweep-rate, sweep-vs-skip, the jump
moment, and the off→red slack in ONE run, no artifacts needed.**
