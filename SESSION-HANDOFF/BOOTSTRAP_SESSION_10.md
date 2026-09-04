# BOOTSTRAP SESSION 10 (2026-09-05, written at the end of session 9)

You are continuing the PlayBook Linux port. Session 9 ran W-25 → W-38
(14 device runs, kernels #102 → #110) and root-caused two of the
project's longest-standing classes: the per-run "random" early-C deaths
and the "silent corruption" theory. Read everything below before any
work. The device is in a normal post-reboot QNX state; the next run
(W-39, kernel #110) is BUILT AND UNRUN.

## MANDATORY READ ORDER

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
   machine-corruption-vs-code-bugs.md (the 2026-09-05 addendum)
8. `PLAYBOOK-REFERENCE.md` §5 (NVRAM), §8 (recovery), §10 (ops lessons)
   — the safety model; NVRAM and RPMB remain untouchable

## WHERE THINGS STAND

- Kernel: 6.15.11 non-LPAE omap2plus, zImage path, --l2on (QNX's PL310
  kept enabled), CONFIG_CACHE_L2X0=n. Repo's kernel state =
  `kernel-patches/` (regenerate after every kernel change; the tree at
  /home/psyden/kernel/linux is machine-local; pristine 6.15.11 at
  /home/psyden/kernel/pristine).
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
