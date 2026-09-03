# Roadmap

Ordered. The first item is the next device run.

## 1. Jump the zImage (next session, first run)

`PAYLOAD_MODE=--l2on ./jump.sh zImage`. Expected: DTB delivered
("Machine model: BlackBerry PlayBook"), full ~448 MB memory, CMA reserved,
boot proceeds past the 16 MB-degraded state. Then the bc=171 wall gets its
first real test.

## 2. Retest / debug the 171 wall with full memory

If it still dies at `taskstats_init_early → kmem_cache_create` with a real
DTB: use the PB-CMA print + ring1 evidence. Check the unpatched-pv-stub
signature (KNOWN_ISSUES #4) before suspecting hardware corruption.

## 3. Deal with l2x0_of_init before the boot reaches init_IRQ

Patch `l2c_enable` to skip the by-way op (0x7FC) on winchester, or set
`CONFIG_CACHE_L2X0=n` (keep QNX's L2 setup). See KNOWN_ISSUES #3.

## 4. RE SMC 0x112 (the undocumented loader service)

Full disassembly of `bootblob_arm9000.bin` (local-only; from the eMMC
secure-boot chain), find 0x112's callers and arguments. If it is an L2
tag/data-latency write service, the QNX 1/1/1 latency config becomes
fixable pre-jump.

## 5. Rootfs (T4)

eMMC (`root=/dev/mmcblk0p2`) with a minimal ARM rootfs (buildroot or
debootstrap). The Droid 4 / PandaBoard mainline support is the reference.
eMMC is on SDIO/ADMA — untested territory per the older hypothesis lists.

## 6. Cleanup / secondary

- Investigate the console-silencing after the memblock prints (KNOWN_ISSUES #5).
- Park the uncompressed-Image r2 mystery (only revisit if zImage fails).
- Decide the license (DECISIONS D9) — required before making the repo
  formally reusable.
- CI: probably a lightweight lint/build-check for the kexec payload
  (needs a QNX toolchain — likely NOT worth CI; document instead).

## Non-goals (explicit)

- Replacing the boot chain / IPL (HS device — impossible and out of scope).
- Android/something-bigger-than-mainline-Linux; the goal is mainline.
- UART pads on the board (hunted, none found — closed).
