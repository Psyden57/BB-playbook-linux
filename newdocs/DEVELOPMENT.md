# Development Workflow

## Daily loop (the jump cycle)

```
 1. edit kexec/ sources (payload/stub/probe) or the kernel tree
 2. kexec: bash build.sh          | kernel: make ARCH=arm CROSS_COMPILE=... Image
    (see newdocs/SETUP.md)        |         + dtbs; then kexec/mkkernel.sh Image|zImage
 3. PAYLOAD_MODE=--dmaquiet ./jump.sh zImage   (from kexec/; = --l2on +
    slay devb + the DISPC kill — the session-9 default run mode)
 4. watch the LED: blue = payload/kernel start, off = kernel running, red = reset
 5. jump.sh automatically polls the bc ladder and dumps readbacks after reboot
 6. decode the console ring: ssh root@169.254.0.1 "on -C 0 /tmp/memdump3 88000080 0x4e0"
    then decode word-reversed (memdump3 prints words big-endian)
 7. update docs/03 (run log) + the handoff
```

Rules of the run (from docs/ + hard experience):

- **Ask the user before every device run.** The user can hard-reboot (power
  hold) and video-records runs — request LED timings (blue-on duration,
  blue-off → red gap) for every jump.
- Verify the bc[15] nonce on every readback (stale pages from previous runs
  happen).
- Never: inline-in-stub SMCs, NS PRCM writes, NS PL310 CTRL/AUX/latency
  writes, by-way ops (0x7FC), unbounded polls, UART3 writes from the kernel.
- A payload SIGSEGV does **not** reboot the device; a WDT2 expiry does (~59 s).
- An aborted payload run disarms WDT2 (no surprise reboot); the next run's
  kick re-enables it properly.
- Editing `stub3.S`/`head.S`/`omap4bc.S`? Read session-notes/session-02.md
  CONFIRMED #1 first — the four assembly traps (inline data executes as
  code; marker immediates must be ARM-encodable; pbmark clobbers r0+ip;
  SP is banked — `cps #0x13` before loading sp).
- **Verify fixes in the shipped binary, not the build log** — a rebuild has
  silently produced a stale packed image before (session 3, run 8); build
  size deltas are the fastest change-check.
- **Cross-check bc[1] against the ring count** before trusting a low
  marker — a bc *below* proven execution means the channel regressed, not
  the kernel.
- **Payload death taxonomy**: (a) ssh-timeout SIGHUP kill mid-setup → no
  reboot, bc frozen at the last armed step; (b) procnto crash → WDT2 →
  reboot, bc survives from pre-crash; (c) successful jump → WDT2 → reboot
  with kernel markers in bc. jump.sh's 120 s timeout exists because of (a).
- **Failed payload runs leak their 24 MB buffer by design** (the allocator
  re-hands a freed rejected block); enough leaks in one boot starve QNX into
  a WDT2 death — expect a reboot every ~10-12 failed placements.
- **Distrust bc[6]+ on readback**: bc_arm sanitizes only bc[0..5] + ring
  headers; deeper slots can hold stale canaries from previous runs
  (KNOWN_ISSUES #7 has the identified writers).
- **NVRAM and RPMB are off-limits forever** (brick hazards — see README
  safety model and PLAYBOOK-REFERENCE.md §5/§8).

## Building the payload

`kexec/build.sh` builds `hello.bin`, `stub.bin` (cacheops+stub3 objects) and
the `qnx2linux` binary with the QNX 6.6 gcc. Everything is plain C + GAS,
no QNX libraries beyond libc. The binary is scp'd to the device by jump.sh.

## Building the kernel

The complete kernel diff lives in **[`kernel-patches/`](../kernel-patches/)**
(base: mainline 6.15.11; `git apply` verified; config = `winchester.config`).
From a pristine 6.15.11 tree:

```
git apply kernel-patches/0001-playbook-winchester-6.15.11.patch
cp kernel-patches/winchester.config .config
make -j12 ARCH=arm CROSS_COMPILE=<toolchain-prefix> zImage dtbs
/home/psyden/playbook-dev/kexec/mkkernel.sh zImage     # appends the DTB
```

`mkkernel.sh` appends `omap4-winchester.dtb` and writes `kexec/kernel/`.
**zImage is the preferred/correct boot path** (its decompressor natively
delivers the appended DTB and derives zreladdr; see PROJECT_STATE.md).

Kernel files with project patches (all marked `PlayBook` in comments —
the authoritative diff is [`kernel-patches/`](../kernel-patches/README.md)):

| File | What |
|------|------|
| `arch/arm/kernel/head.S` | bc ladder, r1/r2 save/restore, ring-map fixes (M=1 fix), DEBUG_LL smoke test |
| `arch/arm/include/debug/omap4bc.S` | DEBUG_LL: UART3 + DRAM ring capture |
| `arch/arm/mm/dma-mapping.c` | PB-CMA diagnostic print in dma_contiguous_remap |
| `arch/arm/mach-omap2/omap4-common.c` | L2 write_sec routing |
| `arch/arm/boot/dts/ti/omap/omap4-winchester.dts` | memory bank, pl310 node, reserved-memory (bc/ring pages) |
| `arch/arm/mach-omap2/omap-secure.c` | (steal behavior interacts with placement) |

### Config recovery (important!)

`CONFIG_IKCONFIG=y` bakes the .config into every built kernel. If `.config`
is ever mangled (a broken syncconfig can do it):

```
scripts/extract-ikconfig arch/arm/boot/Image > .config
```

Forced cmdline (CONFIG_CMDLINE): `console=ttyO2,115200n8 earlyprintk
keep_bootcon ignore_loglevel maxcpus=1 root=/dev/mmcblk0p2 rootwait`.

## Debugging toolbox

- `memdump3 <phys> <len>` (on-device): RAM dump, **words big-endian —
  reverse each 4-byte group when decoding**.
- `smctest <service> <r0> <r1>`: interactive SMC from QNX.
- `scatter`: tests which DRAM pages survive the jump+reset cycle.
- `memtest`, `memw32`: raw DRAM verification.
- bc ladder map: `docs/04_KEY_FILES_AND_COMMANDS.md`.
- The kernel's PB prints (PB-ADJ, PB-MEM, PB-CMA) land in ring1 via
  pr_notice/earlycon.

## Where things are documented after a run

1. `docs/03_DEBUGGING_SESSIONS.md` — append the run (numbered W-series as
   of session 7) with: payload log, bc readback, ring decode, analysis.
2. `newdocs/PROJECT_STATE.md` — update "where the boot stands".
3. `newdocs/session-notes/session-NN.md` — session-specific knowledge.
4. Contradictions/uncertainties → `newdocs/contradictions/`.
