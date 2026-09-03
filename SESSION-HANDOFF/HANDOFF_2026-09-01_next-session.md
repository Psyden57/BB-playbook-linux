# HANDOFF + NEXT-SESSION PROMPT — 2026-09-01 evening
# PlayBook QNX→Linux kexec: M=1 wall SOLVED; early-C wedge = current front

Read this top to bottom before touching anything. The embedded PROMPT at the
end is what you paste into the next session after reading the references.

---

## PART 1 — WHERE THINGS STAND

### Milestone: the M=1 wall is SOLVED
For two sessions the kernel died silently at the MMU-enable transition
(bc ceiling 122). Root cause (found 2026-09-01): the PlayBook ring-map block
in `arch/arm/kernel/head.S` `__create_page_tables` used PA>>12-style values
(`0x88000`, `0x90000`, `0x94000`, `0x48200`, `0x9FE00`) shifted with
`lsl #SECTION_SHIFT` (=20, non-LPAE). 0x88000<<20 = 0 (bits truncated past
bit 31) → all special descriptors aliased to PA 0x00000000 (boot ROM) /
PA 0x20000000 (GPMC):
- ring VAs 0xC8000000/0xD0000000/0xD4000000 → PA 0
- PL310 VA 0xFEB00000 → PA 0x20000000
- abort-trap vectors VA 0x0 / 0xFFFF0000 → PA 0 (ROM!) — so "no abort fired"
  was never real evidence
Fix: shift those values by **12** (`lsl #12`), giving tt[0xC80]=0x88000C12,
tt[0xD00]=0x90000C12, tt[0xD40]=0x94000C12, tt[0xFEB]=0x48200C12,
tt[0x000]=tt[0xFFF]=0x9FE00C02. Verified in the rebuilt vmlinux disassembly.
Also explains the old Image-era "reaches 108, never 110" trail.

### Current state after the fix
Kernel (mainline 6.15.11, omap2plus, zImage+DTB, non-LPAE) now passes
M=1, __mmap_switched, and enters start_kernel C. Fine markers (flush-free SO
stores to the bc page — proven to reach DRAM with no cache maintenance):
118 → 150 (post set_task_stack_end_magic) → 151 → 142 → 144 → 145 →
143 (post cgroup_init_early) → 140 → 141 → 136 (setup_arch) → 130-135 →
110 (post paging_init) → 111 (post setup_arch).

### The new blocker: random silent stops in early start_kernel C
- Same binary, different runs: bc stops at 145, or 168, or 150. NOT deterministic.
- No abort: the post-M abort trap is NOW functional (vectors fixed) and its
  flag (0x9FE000A0) is 0 every run.
- No panic: a panic notifier dumps "PB-PANIC: <msg>" into the rings via
  early_print/printascii; rings are always clean.
- cgroup bisect markers (160/161/0x100+i/0x120+i/164-170 in
  kernel/cgroup/cgroup.c) put one stop right after init_and_link_css (168).
- Leading theory: the kernel's early C is the first sustained cacheable
  write-back traffic through the L2 in QNX's configuration; random
  interconnect/eviction stalls stall the CPU silently.
- Ruled out this session: DRAM corruption (memtest clean), DISPC scanout
  contention (blanked, unchanged), ACTLR.SMP broadcast (entry value 0x1),
  panic/BUG paths (rings clean).

### PL310 facts (all verified on-device 2026-09-01)
- r3p2 (cache-id 0x410000C4). QNX leaves: control 0x100=1 (enabled),
  aux 0x104=0x1E070000, tag-lat 0x108=0x00000000,
  **data-lat 0x10C=0x00000111 (1-cycle — aggressive)**, prefetch 0xF60=0x5.
- NS write to control (0x100) → **bus hang** (no abort, 60s WDT2).
- NS write to data-latency (0x10C) → **synchronous SIGBUS** (fltno=5, QNX
  catches it, device stays up).
- NS by-way clean+inv (0x7FC) → **background-op deadlock** (twice; erratum
  727915 class). NEVER use by-way from NS on this unit.
- NS READS of all PL310 registers work (memdump3 0x48242000/0x48242100 etc.).
- Maintenance by-PA (0x768) + sync (0x730) from NS: always safe (used
  everywhere).
- TI monitor SMC #0, r12=**0x101** = L2 clean+inv by PA (r0=PA, r1=size):
  verified-good, used by the payload.
- TI monitor SMC r12=**0x105**: returned 0, did NOT disable the L2 (control
  readback stayed 1), and BROKE the jump chain (enter_stub's bc=70 never
  landed in any mirror; probe never ran; LED stuck blue ~2.5 min). REVERTED.
  Do not reuse 0x105.

### Kernel-side protective change (keep)
Kernel built with **CONFIG_CACHE_L2X0=n**: the kernel never touches the PL310
config registers (an NS l2c_enable would hang exactly like our payload test).
Runs L1-only. Revisit L2 enablement later via monitor services.

### Environment facts
- Payload host: `/home/psyden/playbook-dev/kexec`, build via
  `source ../qnx-env.sh && ./build.sh` (QNX SDP 6.6 GCC at
  /home/psyden/qnx660-master/host/linux/x86/usr/bin).
- Kernel tree: `/home/psyden/kernel/linux` (mainline 6.15.11 + patches).
  Kernel toolchain: Bootlin/Buildroot armv7-eabihf at
  `/home/psyden/toolchains/armv7-eabihf/bin/arm-buildroot-linux-gnueabihf-`.
  Build: `make ARCH=arm CROSS_COMPILE=... -j$(nproc) zImage` then
  `kexec/mkkernel.sh zImage` (appends omap4-winchester.dtb).
  NOTE: .config currently has CONFIG_CACHE_L2X0=n, CONFIG_SMP=y,
  CONFIG_SMP_ON_UP=y, no LPAE, no KASAN/lockdep.
- Device: root@169.254.0.1 via USB RNDIS, key `kexec/../rsa`, legacy algo
  SSHARGS in jump.sh. Device /tmp wiped on every reboot; jump.sh redeploys.
- WDT2 window = **60 s** exactly (user-timed twice). QNX wdtkick period = 15s
  (that's the kick rate, not the window). Blue LED = payload armed; probe
  LED sequence = blue→off→default→off→default→off; red = bootrom/reboot.
- Ops rules: ASK before every device run; sync after dd/cp; never plain-mmap
  device memory; payload crash → procnto death → WDT2 reboot (bc survives);
  jump.sh timeout is 120s.

### Diagnostics built into the current artifacts
- Payload (qnx2linux): heartbeat markers 42-50 through setup; DISPC blank
  (0x48050000: +0x440/+0x4A0/+0x4C0/+0x500 = 0) at bc=34 — KEEP, harmless;
  bc ladder 30→34→35→42→43→31→10→11→39→40→41→37→32→41.
- Kernel: fine ladder + cgroup bisect markers (flush-free), panic notifier
  (init/main.c pb_panic_nb → early_print → rings).
- Marker function `pb_bc_mark(unsigned)` is defined in init/main.c and
  extern'd in kernel/cgroup/cgroup.c. pb_bc v6 macros (flush-free) are in
  init/main.c and arch/arm/kernel/setup.c.

---

## PART 2 — NEXT TASKS (in order)

### Task 1 — RE the monitor's L2 service IDs (the unlock)
The TI HAL (per TI docs for sibling SoCs) exposes L2 services in the 0x100
range: known-good **0x101 = clean+inv by PA (r0=PA, r1=size, r0=status)**.
Expected siblings: 0x102 set-aux, 0x103 set-latency, 0x104 enable,
0x105 disable — but 0x105 misbehaved (see above), so the map must be RE'd
from the actual monitor/QNX module rather than guessed.
The QNX-side module `dumped4869ifs/proc/boot/trustzone-omap4` (61 KB) is
QNX-PACKED — objdump sees only a QNX_info section. To get real code:
1. On the live device: `pidin arg | grep trustzone` → find the process
   (it may be a resource manager, e.g. /dev/trustzone) or the procnto that
   maps the .so.
2. Dump its mapped memory: `cat /proc/<pid>/as > /tmp/as.bin` (root) — then
   on the host, locate the module's text within the dump (search for the
   QNX_info signature / ELF base) — or use `pidin mem` to find the module's
   base address and memdump3 the text range (it's proven-live memory).
3. objdump the unpacked text (QNX objdump from the SDP handles it; the
   buildroot one may not). Find the SMC call sites: `smc #0` / `.word
   0xE320F000` / `smc #1`, and the r12 constants they load — build the
   service table. Also check the devctl passthrough (0xC0280501, SMC #1,
   fnid table) — PLAYBOOK-REFERENCE §1.10 documents {result,fnid,a1,a2,...}.
4. Identify the L2 config services by their argument shape (aux value,
   latency value, enable/disable flags) and cross-check with TI HAL headers
   from TI's public omap4 x-loader/uboot (hal_api.h: L2CACHE_*).

### Task 2 — Payload: reconfigure/disable the L2 via the REAL services
With verified service IDs, pre-jump (BEFORE the CPU1 hold, while the console
is alive):
- clean the L2 by-PA if needed (0x101 over the kernel/DTB regions — or the
  monitor's own clean-all service),
- set sane latencies (data-lat 0x333 instead of QNX's 0x111) and/or disable
  the L2 entirely,
- verify by NS readback of 0x100/0x10C and record in bc[2]/bc[4] + printf.
If the L2 is disabled: keep the no-L2X0 kernel (already built). If only
latencies are fixable: keep L2X0=n anyway for now (the kernel stays off L2
entirely; L2 enabled-but-unused by the kernel is fine).

### Task 3 — Re-jump and evaluate
- If the kernel climbs past 143 → 136 → 130 → 110 → 111 → further: the
  L2-state theory is CONFIRMED. Then: proper L2 enablement via monitor calls
  (either kernel-side patch at l2c_enable time, or leave L2 off until SMP
  bring-up — decide then).
- If the wedge persists identically: the L2 theory weakens — next suspects
  are other DMA masters (eMMC ADMA mid-transaction, WiFi SDIO) and L3/EMIF
  auto-idle. Diagnostic: the probe.S long-loop test (bare-metal MMU-on
  cacheable write loop with flush-free markers, 10-20 s) to separate
  machine-vs-kernel.

### Task 4 — When the kernel survives setup_arch (post-110/111)
The next wall will likely be at l2c-time or console/uart init. The panic
notifier + rings are your eyes. Keep markers flush-free; do NOT put SMC/CIPA
blocks into bisection paths (they wedge intermittently — that cost us hours).

---

## PART 3 — HARD-WON RULES (additions from 2026-09-01)
1. NEVER write PL310 control/aux/latency from NS (hang or SIGBUS).
2. NEVER use NS by-way (0x7FC) clean+inv — background op, deadlocks.
3. Only by-PA (0x768) + sync (0x730) maintenance from NS.
4. Monitor 0x105 is not L2-disable; unknown services can corrupt the jump —
   RE before probing.
5. Raw GAS mnemonics (dsb/dmb/smc) fail in kernel-generic files — use the
   kernel's dsb(x) macro (CP15 form) and .word encodings:
   dsb sy = 0xF57FF04F, smc #0 = 0xE320F000, dmb sy = 0xF57FF05F.
6. Flush-free SO stores reach DRAM — use them for markers; keep SMC/CIPA
   OUT of bisection paths.
7. A9 ACTLR SMP bit = bit 6 (not bit 0). The decompressor leaves ACTLR=0x1.
8. WDT2 window = 60 s. User-timed. All "15 s" references are the wdtkick
   period.
9. memdump3 prints raw u32 words; byte-reverse per word to read strings.
10. bc_arm does NOT sanitize bc[6]/bc[7] — stale probe canaries there are
    from previous runs; do not misread them as fresh evidence.
11. jump.sh must run from kexec/ (rsa is ../rsa). If a run hangs pre-jump at
    bc 39-41 (sync/eMMC stall), retry once before diagnosing.

---

## PART 4 — KEY ADDRESSES (unchanged, for fast recall)
0x90000000 bc primary (bc[1]=step@+4, bc[2]=@+8, bc[3]=@+C, bc[4]=@+10,
bc[5]=@+14, bc[6]=@+18, bc[7]=@+1C) · mirrors 0x94000000/0x88000000/0x9FE00000
· ring headers @+0x80/+0x84, chars @+0x100 · abort flag: bc[1]=0xAB ·
trap vectors at 0x9FE00000 (handler writes 0xAB to bc[1]) ·
IRAM: table 0x40304000, cont 0x40308000, probe 0x40309000, params 0x40309800 ·
PL310 0x48242000 (ID) / 0x100 control / 0x104 aux / 0x108/0x10C latencies /
0x730 sync / 0x768 by-PA CIPA · WDT2 0x4A314030 · GICD 0x48241000 ·
CPU1 reset 0x4824380C · DISPC 0x48050000 · UART3 0x48020000.

---
---

# PROMPT FOR THE NEXT SESSION

Paste everything below this line into the new session:

---

You are continuing a hands-on QNX→Linux kexec project on a BlackBerry PlayBook
(TI OMAP4430 "winchester", QNX 6.6, root via SSH at root@169.254.0.1, key at
playbook-dev/rsa). Read, in this order:
1. playbook-dev/SESSION-HANDOFF/HANDOFF_2026-09-01_next-session.md (this file —
   the complete state, rules, and task list),
2. playbook-dev/SESSION-HANDOFF/SESSION-RECORD_2026-09-01_ring-map-fix_early-C-wedge.md
   (the previous session record),
3. playbook-dev/docs/README.md and docs/03_DEBUGGING_SESSIONS.md (current
   eliminated-theory list — do not re-test any of them).

State summary: the old M=1 wall is SOLVED (head.S ring-map shift bug, fixed;
kernel reaches start_kernel C, bc 145/168/150). The current blocker is a
random silent wedge in early start_kernel C (no abort — the trap now works;
no panic — a panic notifier dumps to the breadcrumb rings and they're clean).
Leading theory: L2/PL310 state (QNX leaves 1-cycle data latency, aux
0x1E070000, L2 enabled) stalling under the kernel's first cacheable WB
traffic. All PL310 CONFIG registers are secure-filtered from NS (control
write = bus hang; latency write = SIGBUS; by-way clean+inv = deadlock;
monitor service 0x105 is NOT disable and breaks the jump — all verified,
do not retry). The kernel is built with CONFIG_CACHE_L2X0=n.

Your mission:
1. FIRST TASK — RE the monitor's real L2 service IDs: unpack the QNX-packed
   module dumped4869ifs/proc/boot/trustzone-omap4 by dumping its mapped
   memory from the live device (/proc/<pid>/as; find the pid via
   `pidin arg | grep trustzone`; base address via `pidin mem`), disassemble
   (QNX SDP objdump), and build the SMC service table from the r12 constants
   at the smc call sites. Known-good anchor: SMC #0 with r12=0x101,
   r0=PA, r1=size = L2 clean+invalidate by PA. Expected siblings: set-aux,
   set-latency, enable, disable. Cross-check with TI's public omap4 HAL
   headers (hal_api.h, L2CACHE_*).
2. With verified IDs: modify kexec/qnx2linux.c to reconfigure the L2 via the
   monitor BEFORE the CPU1 hold (console still alive): sane data latency
   (0x333) and/or full L2 disable; verify with NS readbacks of 0x48242100
   (control) and 0x4824210C (data latency) recorded in bc[2]/bc[4] + printf.
3. Re-run ./jump.sh zImage with the no-L2X0 kernel (already built). If bc
   climbs past 143 → 136 → 130 → 110 → 111 → further, the L2-state theory is
   confirmed — report and decide on proper L2 enablement. If the wedge
   persists identically, run the probe.S long-loop diagnostic (bare-metal
   MMU-on cacheable write loop, flush-free markers) to separate
   machine-vs-kernel before touching anything else.
4. Ops rules: ask me before EVERY device run; jump.sh from playbook-dev/kexec;
   WDT2 window is 60 s; payload crash = reboot (expected); the ring-map fix
   and all markers are already in the built artifacts — do not revert them.
5. Keep the breadcrumb ring-capture and panic-notifier diagnostics intact in
   every kernel build, and update the session record at
   SESSION-HANDOFF/ with each run's results.

Do not re-test eliminated theories (list in docs/03). Do not use NS writes to
PL310 config registers. Do not use NS by-way maintenance. Ask me whenever a
device run is needed.