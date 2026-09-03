# Setup

This project needs four things on the host: a QNX 6.6 SDP (for the payload
toolchain), an ARM Linux cross toolchain (for the kernel), a mainline kernel
tree, and a PlayBook reachable over SSH. Everything else is in the repo.

## 1. Host layout used by the scripts

The repo lives anywhere; the docs assume:

```
~/playbook-dev/          this repository
~/qnx660-master/         QNX SDP 6.6 (unzipped installer tree)
~/kernel/linux/          mainline Linux tree with the project patches applied
~/toolchains/armv7-eabihf/   ARM cross toolchain (buildroot-style, gcc 14.x)
```

Paths are examples — `qnx-env.sh` and `mkkernel.sh` reference them; adjust
to your machine.

## 2. QNX SDP 6.6

- Get the QNX SDP 6.6 installer tree and unzip it to `~/qnx660-master`
  (it is freely downloadable from QNX for non-commercial use).
- `source qnx-env.sh` (repo root) sets `QNX_HOST/QNX_TARGET/PATH/CC`.
- Verify: `arm-unknown-nto-qnx6.6.0eabi-gcc --version`.

## 3. ARM Linux cross toolchain

Any armv7 hard-float GCC works. The kernel build command
(see `docs/04_KEY_FILES_AND_COMMANDS.md`):

```
make -j12 ARCH=arm CROSS_COMPILE=/home/psyden/toolchains/armv7-eabihf/bin/arm-linux- zImage
```

**Never run a bare `make` on the kernel tree without CROSS_COMPILE** — a
host-architecture syncconfig can rewrite `.config` and drop project-critical
symbols (this actually happened; recovered via
`scripts/extract-ikconfig`, see DEVELOPMENT.md).

## 4. Kernel tree + patches

Mainline 6.15.11 with the project patches (see DEVELOPMENT.md for the file
list). The DTS lives at `arch/arm/boot/dts/ti/omap/omap4-winchester.dts`.
Config: use the config recovered from a built Image
(`scripts/extract-ikconfig arch/arm/boot/Image > .config`) or regenerate
from `omap2plus_defconfig` + the deltas listed in `docs/08_KERNEL_DEBUGGING.md`.

## 5. The PlayBook device

- Tablet OS 2.0.0.4869, WiFi-only unit; root obtained via the UFS injection
  described in `README.md` (the injection point section) — the `/radio/`
  path trick.
- Networking: USB/BB dev mode gives SSH at `root@169.254.0.1`; the SSH key
  lives at repo root `rsa` (**local-only, never commit**).
- `jump.sh` expects the tool binaries on the device at `/tmp/`
  (`qnx2linux`, `Image`/`zImage`, `omap4-winchester.dtb`, `probe.bin`,
  `memdump3`) and handles deploy + run + readback automatically.

## 6. Optional local-only assets (not in Git)

| Path | What | How to recreate |
|------|------|-----------------|
| `device-binaries/` | QNX/BlackBerry binaries dumped from the device + disassemblies | `scp` from `/base/usr/lib`, `/base/sbin` etc. of a rooted unit; disassemble with capstone/pyelftools (the box has no ARM objdump; the QNX ELFs are "architecture UNKNOWN" to binutils) |
| `dumped4869ifs/` | the QNX IFS/rootfs dump | dumped from the running OS |
| `optimized-docs/` | QNX 6.6 documentation set | QNX SDP docs |
| `swpu231ap.pdf` (+`.txt`) | TI OMAP4430 TRM | TI / public mirrors |
| `pmaports-main.zip` | postmarketOS pmaports reference | GitHub download |
| `rootufstest/` | test UFS rootfs | local testing artifact |

## 7. Sanity checklist before the first run

1. `source qnx-env.sh && (cd kexec && bash build.sh)` builds the payload.
2. `ssh -i rsa root@169.254.0.1 echo up` — device reachable.
3. Device charged (a full jump + reset cycle takes a couple of minutes and
   the WDT2 window is ~59 s).
4. LED visible (blue = payload/kernel start, red = reboot) — record runs on
   video; the timings are diagnostic data.
