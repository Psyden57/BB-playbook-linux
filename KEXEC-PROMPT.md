# MISSION: kexec-style Linux boot on the BlackBerry PlayBook (QNX → Linux jump)

You are working on legitimate security/systems research on hardware the user owns:
BlackBerry PlayBook tablets (EOL since ~2015). The goal is to boot Linux
(postmarketOS-style) on the device by jumping from the running QNX OS into a Linux
kernel — a "kexecboot"-style flow modeled after https://github.com/tmlind/droid4-kexecboot
(note: the Droid 4 uses the **same SoC** — TI OMAP4430 — so that project's kernel-jump
mechanics are directly relevant prior art).

## Required reading FIRST (workspace context)
Read these two files completely before doing anything else:
- `PLAYBOOK-REFERENCE.md` — consolidated device/OS/bootchain/flashing/NVRAM knowledge base
- `SESSION-HANDOFF.md` — session state, "CURRENT FOCUS" section, open questions

They contain the verified ground truth: platform identification, memory maps, the eMMC
layout, the boot chain, the loader protocol, and hard-won operational lessons (do not
re-derive or contradict them without evidence).

## Platform facts (verified — details in the reference docs)
- TI OMAP4430, dual Cortex-A9 (SMP), **HS (High Security) device** — the boot ROM
  verifies the boot chain; you cannot replace the boot chain or the IFS with unsigned
  data. Everything must work **from within the running QNX OS** (OS 2.0.0.4869 on the
  test unit, or 2.1.0.1917).
- QNX Neutrino 6.6, root via an Android-runtime oversight on the 2.1 unit; the user also has
  **custom root code execution during early boot on the 2.0 unit** (shortly after IFS load, during the
  boot-animation phase, near userland start) — this is the injection point for the jump
  payload, and it runs early enough that RAM is mostly unused and most device drivers are
  only partially initialized.
- RAM: 1GB @ 0x80000000-0xBFFFFFFF (two 512MB banks). On-chip RAM (IRAM/OCMC):
  **0x40304000** (from cfp "IRAM Base") — a natural place for a small jump stub, exactly
  like kexec's relocate-code-buffer concept.
- eMMC: SanDisk SEM32G via `devb-mmcsd-winchester` → `/dev/hd0` (raw user area) and
  friends; storage is accessible early.
- A **watchdog** is active (`omap4430-wdtkick` in the IFS): a hang mid-jump = automatic
  reset ~10s later. Treat this as your crash-recovery mechanism, and make sure the jump
  path either disables it or completes before it fires.
- Known hazard: reading unmapped/unclocked bus addresses via `/dev/mem` causes an
  asynchronous external abort that kills the kernel (watchdog reset). Never probe bus
  addresses blindly; use `mmap_device_memory` with PROT_NOCACHE.

## The core technical problem
QNX has **no kexec syscall** — you must implement the kernel-jump mechanics yourself,
from QNX userland (root, `/dev/mem` works). Conceptually this is what kexec does:

1. Place the target kernel image (plus ATags or a DTB, plus optionally an initramfs) at
   agreed physical addresses.
2. Relocate a tiny "jump stub" into IRAM (or a clean physical page).
3. On the primary core: quiesce what you can, then with caches cleaned/invalidated and
   MMU off, jump to the stub; the stub completes the ARM Linux boot-convention state
   (r0=0, r1=machine type or DTB pointer in r2, IRQs/FIQs off, MMU off, SMP state
   handled) and branches to the kernel entry point.
4. The secondary core (Cortex-A9 #1) must be parked safely — QNX's startup leaves it in
   a holding pen (OMAP4 AUXCOREBOOT / WFE semantics); determine what state it is in
   under QNX and either leave it parked in a pen Linux can collect it from, or route it
   to a safe spin loop.

## Two architectural options (evaluate both, recommend one)
**Option A — direct jump from QNX (single stage):** load the target Linux kernel and
jump straight from your early-boot QNX payload. Simplest chain, but every reboot to QNX
requires re-running the payload, and kernel selection is fixed.

**Option B — kexecboot-style two stage (user's preference, matches the droid4 model):**
your early-boot payload first boots a SMALL purpose-built Linux kernel + initramfs
("kexecboot") from the QNX handoff; that Linux has real `kexec` support (kexec_load +
kexec reboot syscall, ARM machine_kexec) and then boots the real target kernel
(postmarketOS etc.). Benefits: proper kexec semantics, a boot menu, kernel selection,
and all subsequent jumps happen inside Linux where the tooling exists. The QNX→Linux
jump is still Option-A mechanics — done once per boot.

## Key work items / challenges
1. **Target kernel acquisition.** Research prior art before building anything: there
   were community "Linux on PlayBook" efforts (XDA-era kernels; search GitHub/XDA for
   playbook linux kernel trees, TI omap4 kernel releases). Determine: which kernel
   base, ATags vs DTB, what board support exists for the PlayBook ("winchester" board),
   and whether a serial console exists (the IFS runs `devc-seromap` — its buildfile
   options in `dumpedifs/` may reveal the UART address/baud; a UART would be the
   debug lifeline — investigate whether the device's serial pads are reachable).
2. **The QNX→Linux jump stub.** Model it on ARM `machine_kexec` (arch/arm/kernel/
   machine_kexec.c + relocate_kernel.S in any 3.x kernel) and on tmlind's omap4 kexec
   patches: CPU reset sequence (clean+invalidate L1/L2, disable MMU/caches, SMP
   considerations), GIC quiesce, watchdog handling, and the memory-placement plan.
3. **Memory reservation.** The kernel image must land in physical RAM QNX won't touch
   between payload start and the jump. The early-boot injection point helps (little
   memory in use); also investigate what QNX exposes about its memory allocator
   (`pidin`, `showmem`, procnto structures via /dev/mem if needed).
4. **Device quiesce.** eMMC DMA, USB, display (boot animation is running) — decide per
   device: quiesce via /dev/mem register writes, or accept the risk and let Linux
   re-init (OMAP4 Linux re-inits everything, but an active DMA controller corrupting
   RAM under the new kernel is a real hazard — at minimum stop devb I/O and the
   display pipeline).
5. **kexecboot initramfs** (Option B): port/build kexecboot (static, tiny UI) +
   kexec-tools for ARM against the small kernel; the droid4-kexecboot repo shows the
   shape of this.
6. **Persistence & flow.** The user already controls the early-boot injection; design
   the payload hand-off (kernel image loaded from eMMC user area via /dev/hd0, or
   pushed over USB from the host) and the re-entry path back to normal QNX boot
   (default = do nothing, QNX boots normally; kexec path = opt-in).

## Environment & tools available
- Working 64GB unit on OS 2.0.0.4869 (early-boot root exec) and a second working unit
  on 2.1.0.1917 (root shell post-boot) — **use the 2.1 unit for QNX-side experiments**
- On-device root shell + `nvram-editor/nvram.c` tool; `memdump.c` (safe
  mmap_device_memory raw-memory dumps) in the workspace.
- USB loader path (Linux): `bb10mt-main/creadtest` shows a working
  bootrom→ramloader upload; the loader's `$B4`/`$B5` opcodes return flash/DRAM info.
- `dumpedifs/` contains the full QNX IFS (drivers, startup) and
  `bootchain-dumps/` has boot-chain binaries — useful for understanding what QNX
  already initialized (clocks, pinmux, GIC state) before your payload runs.

## Safety rules (hard constraints)
- The two WORKING units are precious: never write to the boot chain, never use the
  nuke/wipe opcodes, never touch RPMB. Kernel-jump experiments happen from the running
  OS and reset on failure (watchdog) — they are inherently safe, but anything that
  WRITES flash must be scoped to user-area locations you have verified.
- The bricked units are for recovery research only (on hold — do not mix goals).
- When experiments crash the device: power-cycle, it comes back to normal QNX boot
  (the injection payload is opt-in; nothing is persisted by your jumps).

## Deliverables
1. A research summary: prior-art kernels for the PlayBook, ATags-vs-DTB decision,
   serial-console findings (with the IFS `devc-seromap` evidence).
2. A design document for the chosen option (A or B) with the memory map of the jump.
3. Working code: the QNX-side jump payload (C, root, /dev/mem based) and, for Option B,
   the small Linux kernel + kexecboot initramfs build.
4. A test plan that uses the watchdog reset as the safety net, starting with a
   "jump to a trivial loop / UART hello" before attempting a real kernel.

Start by reading the two reference documents, then do the prior-art research
(PlayBook Linux kernel trees, tmlind's omap4 kexec work, kexecboot), then present
the design before writing code.
