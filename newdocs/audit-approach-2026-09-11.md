# Approach audit (session 10, 2026-09-11) — Psyden's three questions

Prompted by READFIRST/Psyden.txt's closing section. Every claim below was
grepped/read from a real file (rule 14); the one hex/offset claim
(PL310 register map) was checked against the running kernel tree's
`arch/arm/include/asm/hardware/cache-l2x0.h` and `arch/arm/mm/cache-l2x0.c`,
not recalled. Verdict up front: **the core design (jump from QNX userland)
is sound; no hidden SDP/monitor setup was missed that invalidates it — but
the audit found one systemic unverified assumption (the NS-side PL310 line-op
offset), one whole unexplored userland cache primitive (libcache), and one
never-run era-matrix cell (current kernel × L2-off). All are cheap to test
and none invalidates the wall evidence gathered so far.**

---

## Q1: "What if there's critical MMU/SMC setup in the QNX SDP that wasn't considered?" (mmu.h)

### What mmu.h actually is

`~/qnx660-master/target/qnx6/usr/include/arm/mmu.h` (578 lines, read in
full) = generic ARM CP15 inline helpers: CR register bit defines, TLB
flush ops (v4/v6/v7), TTBR0 get/set, ASID set, DSB/DMB, BTC flush,
`VTOP`/`KTOP` macros for QNX's own page-table layout. **Nothing here is
load-bearing for us and nothing contradicts the design.** The kernel-side
CP15 work already implements all of it natively.

### The REAL SDP finding: libcache — a userland cache-maintenance API never tried

`sys/cache.h` + `libcache.a` (present in the SDP:
`target/qnx6/armle-v7/usr/lib/libcache.a`) expose:

- `cache_init(flags, &cinfo, dllname)` → scans the **syspage cacheattr
  list** (line size, flags, control callout per cache — including the
  OUTER/L2 cache entry with its `control` callout pointer,
  `syspage.h:357-367`).
- `CACHE_FLUSH(cinfo, va, pa, len)` / `CACHE_INVAL(cinfo, va, pa, len)`
  → executes the **syspage callout routines directly from userland**
  (`arm/cache.h`: `__CACHE_EXECUTE_CALLOUTS` — "On ARM, we execute the
  callout routines in the syspage directly").

Why this matters: QNX maintains a coherent L2 on this machine (its whole
OS runs on it). The L2 callout in the syspage is whatever QNX's startup
(building the callouts) chose — if it implements invalidate-by-PA
without clean (or clean-only), that is exactly the primitive we lack for
the stale-line cure. **The payload never used libcache.**

Related untried primitive: `msync(addr, len, MS_INVALIDATE |
MS_CACHE_ONLY)` (+ `MS_INVALIDATE_ICACHE` for exec pages) — libc-level
cache maintenance applied through procnto's cache callouts
(`sys/mman.h:122-126`).

### An RE gap in the corpus, and the direct way to close it

The QNX startup module (`device-binaries/setup-core-inactive` = the
startup binary that BUILT the syspage and its callouts) is in hand — but
its existing `.dis` (ARM-mode linear sweep) decodes **zero** `mcr`/`mrc`
instructions in ~28k lines, so the cache callouts were never actually
read. Two concrete ways to get them:

1. RE the startup module properly (Thumb-aware capstone pass) — find the
   cacheattr callouts and what the L2 `control` callout does per
   operation (clean / invalidate / by-PA / by-way).
2. Dump the **live syspage** on-device (memdump3 from the payload at the
   syspage's phys — reachable from the payload; the callout pointers are
   in the cacheattr list) and disassemble the callouts with capstone.

Either answers "what does QNX actually do for L2 invalidate?" with
evidence instead of folklore.

**Verdict Q1**: no missed hidden setup; two untried userland primitives
(libcache, msync-invalidate) + one corpus-analysis gap (the callouts).
Worth a small experiment each, not a redesign.

---

## Q2: "What if we'd dumped the bootrom/bootloader and RE'd them?"

### What actually exists (checked on disk)

- `device-binaries/bootblob_arm9000.bin` (86 KB, dumped 2026-09-03) —
  **unanalyzed**. This is the pending session-07 bequest (the SMC 0x112
  lead): the eMMC secure-boot chain contains NS-side SMC wrappers for
  **0x112** (×2) — an undocumented service (newdocs/session-notes/
  session-07.md; PROJECT_STATE "The secure monitor" section).
- `device-binaries/PRIMAPP.thumb.dis` — a 112-line snippet only (the
  first function). The RIM bootloader stages were dumped but
  **essentially not analyzed**.
- The TI boot ROM dump itself: **never obtained** (PLAYBOOK-REFERENCE §7:
  "ROM dump pending; needs correct base from the registry blob").
- Note: `bootchain-dumps/`, `nvram-backup/`, `bb10mt-main/` etc. from
  PLAYBOOK-REFERENCE §9 are **not on this machine** — that inventory
  belongs to the earlier (pre-kexec) research workspace. What is here =
  the device-binaries/ files above.

### What the pending RE could still buy us

- **0x112's semantics** — if it is an L2-adjacent service (the observed
  QNX latency value 0x111 is suspicious), the 1/1/1 PL310 latencies
  become fixable pre-jump (ROADMAP #4's reasoning stands).
- The real secure-side **service table**: session 7 proved the *QNX
  binaries* contain no L2 dispatch and concluded "monitor = TI ROM
  monitor, 0x100-0x113 table complete". But bootblob_arm9000.bin is
  actual chain code — if the ROM monitor has services beyond the
  documented table (e.g. an **invalidate-only L2 service**), it is
  there.
- The L2 config the chain itself establishes before QNX (which writes
  the 1/1/1 latencies — nobody has shown this yet; only the live
  readback 0x111 is evidence).

**Verdict Q2**: the path was partially taken; the pending piece
(arm9000/0x112) is high-value and costs zero device runs. It was never a
blocker: the NS access rules were established empirically and correctly.
But the "what if we'd RE'd them" answer is: partially yes, and the
unanalyzed blob is still sitting there.

---

## Q3: "What if it was easier to just clean the DRAM/IRAM first?"

### The reframe: DRAM isn't the poison — the L2 is

Under the session-9 stale-view characterization, the corrupting content
lives in the **L2** (QNX-era/decompressor-era lines served to the C world
or the PTW). "Cleaning DRAM" (wiping/writing DRAM) cannot remove a stale
cache line; and the one monitor service that touches lines by PA
(0x101 clean+inv) is **proved harmful** against stale lines — the clean
step writes the stale line back, poisoning DRAM (W-33, 8/8 failure).
So "clean memory first" as literally phrased would have done nothing.
What decomposes into real, testable designs is **L2-state control**:

### Path A — the never-run era-matrix cell: current kernel × L2-off

- `--t3` (the payload default) disables the L2 via monitor SMC 0x102
  before the jump (qnx2linux.c:1119-1132). `--l2on`/`--dmaquiet` keep it
  on. Sessions 9's runs were ALL L2-on; the stale-view class only exists
  because L2 serves stale lines — **L2-off removes the entire class**
  (pv, the pgd pair, the W-26 stack-smash candidate).
- Precedent: W-17/W-18 ran `--t3` L2-off with the INLINED fixup and
  reached the C world with a live console (bc=127, ring 0x491) — so
  head.S delivery is already proven L2-off; the later switch to --l2on
  was for the SO-store/ring console mechanics, not because L2-off boots
  failed.
- **Nuance (does not make it free)**: under L2-off, map_lowmem's cached
  writes vs the PTW's non-cacheable reads = an **L1-dirty staleness
  variant** (the __enable_mmu attribute strip makes PTW reads go to
  DRAM while the pgd lines may still sit dirty in L1). The proven L2-off
  pairing was the UNCACHED session-5-era kernel (everything to DRAM =
  consistent). A cacheable kernel under L2-off likely needs one targeted
  addition: a DCC-clean over `swapper_pg_dir` after map_lowmem (a few
  lines in mmu.c) before anything walks it. Cheap either way.

### Path B — NS PL310 invalidate-by-PA: the REAL cure, and a systemic unverified assumption found during this audit

**Finding: the project-wide NS-side PL310 line-op address (0x768) is not
a real L2C-310 register per mainline's map.** Evidence:

- Mainline `cache-l2x0.h` (verified in our own tree):
  `L2X0_CACHE_SYNC 0x730`, **`L2X0_INV_LINE_PA 0x770`** (invalidate by
  PA, NO clean), `L2X0_CLEAN_LINE_PA 0x7B0`,
  **`L2X0_CLEAN_INV_LINE_PA 0x7F0`**, `L2X0_CLEAN_INV_WAY 0x7FC`. The
  driver (both L210/L220/L310 variants) uses 0x770/0x7B0/0x7F0.
- Our code writes **0x768** everywhere: `kexec/probe.S:160`,
  `qnx2linux.c:346` ("proven safe from NS"), kernel `head.S` pbmark/
  pbmark3 (`ldr ip, =0x48242768 @ PL310 CLEAN_INV_LINE_PA`) and its
  comment at head.S:117. 0x768 sits between DUMMY (0x740) and
  INV_LINE_PA (0x770) in the map.
- Consequence A: the "NS flush" used throughout may be a **no-op** (and
  the empirical "safe" result is then trivially explained). The verified
  cache effects in the record all came from **SMC 0x101** (the monitor's
  clean+inv by PA — the monitor presumably uses the correct 0x7F0
  secure-side).
- Consequence B: the docs' "NS 0x772 (PL310 invalidate-by-PA, no clean)"
  (ROADMAP #4, bootstrap-10, the contradiction addendum) is wrong twice —
  the register is **0x770**, and it has never been written from NS (the
  only NS line-op ever written is the likely-dummy 0x768).

**Proposed experiment (cheap, in-QNX, no kernel needed)**: a smctest/
payload test that, for a known PA (e.g. a scratch page in the survivor
band), establishes the W-33-style divergence (DRAM value B via NOCACHE
write vs cached write value A still in L2), then writes 0x770 (inv) /
0x7B0 (clean) / 0x7F0 (CIPA) vs 0x768 and records which register
actually changes the DRAM-verified state. Also verify against the TRM's
L2CC chapter (the PDF text is not greppable for this chapter — check the
swpu231ap L2CC section by hand or via the payload readbacks).

If 0x770 works from NS (same family as the payload's 0x768 by-PA writes;
the NS-fatal ops were CTRL/AUX/latency writes and the by-way 0x7FC), the
**entire stale-line class gets its real cure**: invalidate the image
region + the pgd region without poisoning DRAM — no per-victim self-heal
ladder, no 2MB shave needed.

Related: `l2c_ns_clean_range` (qnx2linux.c:342-351) polls 0x730
unbounded — rule 10 violation if ever reused under load; harmless today.

### Path C — libcache/msync (from Q1) as a third route to the same cure

If the syspage's L2 callout implements invalidate-by-PA, `CACHE_INVAL`
via libcache (or `msync(MS_INVALIDATE|MS_CACHE_ONLY)`) gets us the same
cure from plain QNX userland without any register guessing.

---

## Q4: "Was there an easier way all along?" (the broader audit)

- **The chosen path (QNX-userland jump) is sound.** The bootchain is
  locked (RIM-signed chain, HS eFuse); the realistic alternatives
  (unsigned-OS flash via the ramloader's conditional $C0 signature
  check — the OTHER project's top RE target; replacing the IFS startup
  with a Linux image — IFS/rifsboot format + QNX-specific boot protocol
  mismatch) do not shortcut Linux on the device. kexec-style from a
  rooted QNX remains the pragmatic route.
- **The cheaper steps that were genuinely missed** (ranked by
  cost/benefit, all consistent with — not contradicting — the evidence):
  1. **NS PL310 0x770 (inv-by-PA) test** — one smctest; potentially
     cures the whole stale-view class. (Also corrects the 0x768
     assumption.)
  2. **Current kernel × L2-off (`--t3`)** — one run; tests whether the
     class simply disappears (with the pgd-clean nuance if it wedges in
     map_lowmem).
  3. **bootblob_arm9000.bin RE** — zero-device; the 0x112/latency lead.
  4. libcache/syspage-callout extraction — zero-device; may expose the
     same cure from userland.
- None of these were catastrophic omissions: the rings/bc evidence chain
  is what made the stale-view diagnosis possible at all, and W-32c + the
  placement guard are load-bearing regardless (they'll keep the boot
  correct even after a global L2 cure).

## Standing corrections to record

- "NS 0x772" → **0x770** (mainline cache-l2x0.h; ROADMAP #4 corrected
  with a dated note; BOOTSTRAP_SESSION_10 and the contradiction
  addendum keep their historical text — the audit doc is the erratum
  carrier).
- The NS-side "PL310 CLEAN_INV_LINE_PA at 0x768" assumption → UNVERIFIED,
  likely not a real op register (mainline map). Every 0x768 write site is
  now suspect-as-a-flush (still harmless as a store).
