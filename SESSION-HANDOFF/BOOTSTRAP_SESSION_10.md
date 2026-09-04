# BOOTSTRAP SESSION 10 (2026-09-04, written at the end of session 9)

You are continuing the PlayBook Linux port. Session 9 ran W-25 → W-38
(14 device runs, kernels #102 → #110) and root-caused two of the
project's longest-standing classes: the per-run "random" early-C deaths
and the "silent corruption" theory. Read everything below before any
work. The device is in a normal post-reboot QNX state; the next run
(W-39, kernel #110) is BUILT AND UNRUN.

## MANDATORY READ ORDER

0. `newdocs/HANDOFF.md` (the standing protocol: what every session must
   do) + `newdocs/DEVELOPMENT.md` (the daily loop) + `newdocs/SETUP.md`
   (host paths) — the standing rules layer
1. `newdocs/session-notes/session-09.md` — the session-9 run map,
   confirmed facts, mechanisms, bequest (THE core context)
2. `newdocs/PROJECT_STATE.md` — the live state (through W-39 next)
3. `newdocs/ARCHITECTURE.md` + `newdocs/COMMANDS.md` — the machine and
   the command set (note: --dmaquiet is the default run mode now)
4. `docs/03_DEBUGGING_SESSIONS.md` — sections W-25..W-38 (the run-by-run
   evidence; append-only, never rewrite)
5. `docs/README.md` — rules 1-17 (rule 17 = the pb_bc mirror-channel
   discrimination rule, new this session) + the decision tree + the
   extended bc slots
6. `newdocs/KNOWN_ISSUES.md` — session-9 status + the historical detail
7. `newdocs/contradictions/` — especially
   machine-corruption-vs-code-bugs.md (the 2026-09-04 addendum)
8. `PLAYBOOK-REFERENCE.md` §5 (NVRAM), §8 (recovery), §10 (ops lessons)
   — the safety model; NVRAM and RPMB remain untouchable

## QUICK ACCESS (facts the old bootstraps carried)

- Device: QNX 6.6, SSH root@169.254.0.1, key `playbook-dev/rsa`; the
  required option set is in newdocs/COMMANDS.md (old dropbear: ssh-rsa
  + hmac-sha1). Boot to SSH after a reset = 2-3 min; /tmp wiped per
  reboot (jump.sh redeploys everything).
- The Git repo (private, github.com/Psyden57/BB-playbook-linux, HTTPS +
  cached PAT) = the source of truth; the kernel tree itself is
  machine-local.
- Objdumps EXIST: `~/toolchains/armv7-eabihf/bin/arm-linux-objdump`
  (our kernel, full symbols on vmlinux) and `~/qnx660-master/host/
  linux/x86/usr/bin/arm-unknown-nto-qnx6.6.0eabi-objdump` (QNX ELFs).
  Capstone for raw memdump3 dumps (word-reversed, big-endian print).
- The device clock = GMT-3, the host (WSL) = UTC — convert when
  comparing LED timelines to logs.
- The user records every run on video and types exact LED timelines —
  ask for them after every run (recordings are sometimes lost; offer
  re-runs when timings matter).

## WHERE THE CODE STANDS (per file — kernels #102 → #110)

- **init/main.c**: the W-32c pv block in start_kernel right after
  pb_bc(141) (first printk) — marker 163, retry count → bc[19]
  (0xD000004C). This is THE pv cure: direct store of the build
  constants + DCCIMVAC. The old pb_bc ladder (140/141/143/144/145/150/
  151/111/118/171/172/176-180/186/187) intact.
- **arch/arm/mm/mmu.c**: the bisect marker block inside iotable_init
  (150/159/160/161, the W-38 pmd-pattern dumps → bc[19]-bc[25], store
  markers 0x567/151 — armed in #110); the 2MB allocator shave after
  map_kernel's 127 (arm_lowmem_limit -= 0x200000); alloc_init_pte
  markers 156/157/158 + __create_mapping's 155; the pb_bc_put extern
  declared before alloc_init_pte; the W-24-era invalidate block in
  adjust_lowmem_bounds was REMOVED (moved to main.c as W-32c).
- **arch/arm/mm/dma-mapping.c**: the remap markers 145/146/147.
- **arch/arm/kernel/setup.c**: the pb_bc(130-136) pair ladder (COLLIDES
  with mmu.c's numbers — rule 17); the LED-off diagnostic block after
  paging_init.
- **arch/arm/kernel/head.S**: unchanged this session (the inline fixup
  + markers 143/144/delta-bc[6] + the streamed-region rules).
- **kexec/qnx2linux.c**: the --dmaquiet mode (= --l2on + `system("slay
  -f devb-mmcsd-winchester")` after the last file read, rc → bc[14] =
  0xD1EBxxxx) + the DISPC kill with readbacks bc[16]/bc[17] + bc[18] =
  the chosen placement written BEFORE the memtest + the 64KB stdout
  buffer in main(). buf_placement_bad now reserves [0xa0000000,
  0xa1000000) for the zreladdr inflation region.
- **kernel-patches/**: regenerated through #110, apply-check clean.

## THE CURRENT DEATH (the front)

bc[1] = 161 — the svm memset (44 B of struct static_vm at PA
0xbfdfffd4, VA 0xdfdfffd4) dies on its FIRST stores: the pmd for that
VA reads as a bogus TABLE descriptor (0xbfc1141e → a QNX-era pte table
at 0xbfc11400 — never allocated this boot) instead of map_lowmem's
section desc. The PTW walks into garbage → the machine's silent wedge.
Deterministic across runs AND placements (5 svm deaths). pv is CORRECT
through the whole boot (the W-32c fix works — PB-ADJ prints
d0000000/c0000000, PB-CMA pv_off=ffffffffe0000000, the BUG line gone).
The W-38 pmd pattern: [0xdf8] = CORRECT section (0xbf81141e);
[0xdfc]/[0xdfd] = the stale pair; [0xdfe+]=0; [0xdf0/0xdf4]=0
(CMA-cleared); [0xdfa] = UNTESTED — that's W-39's probe.
- **The pv-stale regime is CURED** (build #106, the "W-32c block" in
  start_kernel, marker 163): directly store the build constants
  (__pv_offset = 0xffffffffe0000000, pfn = 0xa0000) then DCCIMVAC —
  tries=0 every run since. bc[19] slot 0xD000004C holds the retry
  count. KEEP.
- **The stale pgd pair is the current wall**: the pair [VA 0xdfc/0xdfd]
  (the LAST mapped pair of the linear map, PA 0xbfc00000/0xbfd00000)
  reads as identical bogus TABLE descriptors (0xbfc1141e → a QNX-era
  pte table at 0xbfc11400, deterministic across runs AND placements).
  map_lowmem's section write for that pair doesn't survive in the
  PTW's view (the write-back-loss class, same as the pv variables was).
  Any allocation in the top 2MB of the linear map wedges on the walk
  (the svm memset deaths: W-25/31/32a/34/36/37 — five runs at bc[1]=161).
- **DMA quiesce = --dmaquiet** (the default): devb slain after the file
  reads; DISPC killed + register-verified (bc[16]/bc[17] = 0/0); WiFi
  SDIO never brought up. The randomness SURVIVED the quiesce → the
  source is the stale-view class, not a rogue DMA master.
- Swipe/tap interactions EXONERATED (W-32a hands-off = byte-identical
  to W-31). Hands-off is the baseline protocol anyway.

## FIRST TASK: run W-39 (kernel #110, already built and packed)

`PAYLOAD_MODE=--dmaquiet ./jump.sh zImage` — hands-off, record video,
ask the user for exact timings.

What's in #110 (all shipped-binary verified):
- The 2MB allocator shave in paging_init (after map_kernel's 127):
  arm_lowmem_limit -= 2MB → 0xbfc00000 — nothing is allocated in the
  poisoned top 2MB (cost: 2MB of RAM reserved forever, bring-up only).
- The svm (44 B, struct static_vm) will now land at PA ~0xbfbfffd4 →
  VA 0xdfbfffd4 → pair [0xdfa/0xdfb] = the previously-UNTESTED middle
  pair.
- The memset is re-armed with markers: 167 (pmd dumps done), 0x567
  (store 1 done), 151 (memset done); bc[19] = the word-0 readback.
- The pmd pattern dump is extended: bc[19] = VA 0xdfd, bc[20] = VA
  0xdfc (pair-mate), bc[21] = VA 0xdfe (beyond limit — expect 0),
  bc[22] = VA 0xdf8 (expect the CORRECT section 0xbf81141e), bc[23] =
  VA 0xdf4 (CMA-cleared, expect 0), bc[24] = VA 0xdf0, bc[25] = VA
  0xdfa (the NEW probe — the middle pair!). Read via
  `memdump3 90000040 0x30` (jump.sh does NOT do this automatically —
  run it manually right after the jump.sh readbacks).

Interpretation table:
- bc[1] ≥ 151 → **THE MEMSET COMPLETED — the boot advances** → the next
  front is downstream (early_fixmap/devicemaps/bootmem → the 171 wall →
  KNOWN_ISSUES #3's l2x0 hazards at init_IRQ). Keep going with the
  ladder.
- bc[1] = 167 with no 0x567 → the middle pair [0xdfa/0xdfb] is ALSO
  stale → the poisoned region is wider than 2MB → extend the shave or
  self-heal the pgd (write the expected section desc + DCCIMVAC before
  any access through it — the W-32c pattern applied to pmds).
- bc[19] (the pmd for the NEW svm VA, 0xdfbfffd4) = a healthy section
  desc (0xbfa00XXe-shaped) vs a TABLE pointer tells which pair state
  immediately.
- Anything else → re-run the decision tree (docs/README).

## THE OPEN QUESTIONS (carried from sessions 8-9)

1. **The stale-region width**: is only the LAST pgd pair poisoned, or
   the whole top-of-map cache line / more? W-39's bc[25] (VA 0xdfa)
   answers it.
2. **The W-26 stack-protector catch's mechanism**: a wild write smashed
   fdt_get_property_namelen's frame — the stale-view model needs the
   boot-stack lines to be stale-era content too (unverified).
3. **bc[2]=0x3E7's writer** — probe+zImage-correlated, back in the
   head.S-era deaths (W-29's 121, W-35's 142); the bc[0x80-0x8F] dump
   found no signal.
4. **The MMU-off delivery paradox mechanism** (session 8) — unsolved;
   the INLINE-don't-call rule stands.
5. **W-29's head.S-era 121 death** — predates the placement guard and
   the pv fix; probably the stale-era dice. Treat as covered unless it
   recurs.
6. **The 171 wall** (taskstats/kmem_cache) — the boot must first cross
   the paging region; the era matrix (KNOWN_ISSUES #1 historical)
   applies.
7. **NS 0x772 (PL310 invalidate-by-PA, no clean)** — untested; if it
   works from NS, it enables the REAL cure for every stale L2 line
   (invalidate without poisoning DRAM).
8. **W-20's "133 = map_lowmem done" reading** — doubtful (the setup.c
   marker collision); unresolved (no kernel-tree git history).

## DEAD ENDS (do NOT retry these)

- Direct MMC2/MMCHS register access from NS: SIGBUS fltno=5 AND the box
  then froze completely (power-hold to recover). Device registers
  generally: DISPC = accessible; MMC2 = not; PRCM = secure-filtered.
- SMC 0x101 (clean+inv) against a STALE L2 line: the clean poisons DRAM
  (W-33's 8/8 verify failure). No invalidate-only monitor service
  exists.
- The W-24-style __pa()-based flush in a stale-pv regime: __pa consumes
  the stale pv it's repairing — self-defeating.
- Slain-devb runs: the eMMC rootfs execs fail ("cat: cannot execute") —
  expected, not a fault.
- The UART pad hunt: dead (superseded by the DRAM rings + PB-PANIC).

## SUBSEQUENT TASKS (in order)

1. **W-40+: the boot past the paging region.** Once the svm memset
   completes, the ladder continues: 152 (create_mapping done), 153
   (add_static_vm), 147 (iotable done), 126/125/129/128 (fixmap/
   devicemaps/bootmem), then main.c's start_kernel markers (150/151/
   143/145/140/141/163...), then the 171 wall (cgroup_init/taskstats —
   read the session-4/5-era analysis in KNOWN_ISSUES #1 BEFORE
   bisecting it), then the l2x0 hazards (KNOWN_ISSUES #3 —
   l2c_enable's by-way 0x7FC DEADLOCKS from NS; decide: patch
   l2c_enable to skip the by-way op, or keep CONFIG_CACHE_L2X0=n).
2. **The stale-pgd cure** (after the shave proves the model): either
   (a) the pgd self-heal (the W-32c pattern: write the expected section
   desc + DCCIMVAC the pgd line in the C world before anything walks
   through it), or (b) test NS 0x772 (PL310 invalidate-by-PA, no clean)
   via smctest from the payload — that would enable the REAL fix
   (invalidate the stale L2 lines without poisoning DRAM).
3. **The 171 wall** resumes with the era matrix (KNOWN_ISSUES #1): if
   the boot reaches it, the taskstats/kmem_cache bisect resumes with
   the session-5-era marker style (181/182/183-185).
4. Cleanups (only after the boot is deep): prune the dead CIPA
   machinery, prune the retired markers, regenerate kernel-patches
   after every change, commit+push after every run.

## OPERATIONAL RULES (the ones that bite)

- Rule 13: ALL hex arithmetic through python, never mental math.
- Rule 14: grep/read every source claim; recalled code is fabricated
  often.
- Rule 15: native read/edit/write tools; no python3/sed file edits
  (python3 as a calculator is fine).
- Rule 16: verify the SHIPPED binary (objdump the packed artifact's
  source vmlinux / the payload binary) before every run.
- Rule 17: setup.c's pb_bc pairs (130-136) collide with mmu.c's
  PB_MMU_BC numbers — discriminate via the mirror (0xD4000004).
  Shared slots get overwritten by later phases — use bc[19]-bc[25].
- The pv fix (W-32c) and the placement guard are LOAD-BEARING — keep
  them.
- MMCHS/device registers = NOT NS-accessible (SIGBUS + box freeze —
  power-hold to recover). DISPC registers are NS-accessible.
- Payload SIGSEGV/SIGBUS ≠ reboot, EXCEPT the external-abort class
  (fltno=5) which can wedge the interconnect.
- WDT2 window = 58.6s; the runs' death-to-reboot gap = the last kick
  + 58.6s. The TWL6030 PMIC watchdog (127s) = full power-off, DRAM
  LOST.
- Device reboot → SSH in 2-3 min. /tmp is wiped per reboot.
- jump.sh output unfiltered (no tail); the user records video and
  types exact LED timelines — always ask for them after each run.
- Ask the user BEFORE every device run.
- Commit + push after every run and every change.

## CONTRADICTION LEDGER STATE

- The machine-corruption theory: CHARACTERIZED — stale-view (QNX-era /
  decompressor-era L2 content served to the C world or the PTW), not
  random damage. One open datum: the W-26 stack-protector catch
  (fdt_get_property_namelen frame smash) — the stale-view model would
  need the boot-stack lines to be stale-era content too.
- W-20's "133 = map_lowmem done" reading is doubtful (the setup.c
  marker collision) — recorded, not resolved (no kernel-tree git
  history to date when setup.c's markers landed).

## BOOTSTRAP PROMPT FOR SESSION 10

"This is session 10. Read SESSION-HANDOFF/BOOTSTRAP_SESSION_10.md and
follow its read order completely before any work. First task: run W-39
(kernel #110, PAYLOAD_MODE=--dmaquiet ./jump.sh zImage, hands-off,
recorded) and decode the extended bc slots via memdump3 90000040 0x30 —
the front is the svm memset (bc[1]=161, the stale pgd pair); build #110
shaves the allocator limit by 2MB and probes the middle pair [0xdfa/
0xdfb]."
