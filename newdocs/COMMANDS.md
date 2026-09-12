# Command Reference (live)

The complete, current command set. `docs/04_KEY_FILES_AND_COMMANDS.md` holds
the older deep tables (file inventory, memory map); this file holds what we
actually type, including everything added since the repo existed.
Paths use the session-7 machine layout — adjust to yours (see SETUP.md).

## Device connection

```bash
# the SSH option set is REQUIRED (old dropbear on the device):
SSH="ssh -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa \
  -o PubkeyAcceptedKeyTypes=+ssh-rsa -o ConnectTimeout=8 \
  -i ~/playbook-dev/rsa root@169.254.0.1"
$SSH "echo up"                          # connectivity check
$SSH "on -C 0 /tmp/memdump3 90000000 0x40"   # read the bc page
```

`jump.sh` (in `kexec/`) wraps deploy + run + readback and defines the same
options in `SSHARGS`. Payload mode is chosen by env: `PAYLOAD_MODE=--l2on`/
`--dmaquiet`/`--t3`/`--probe`/`--ppa`/`--l2lat`; kernel image = argv[1]
(`./jump.sh zImage` — **zImage is the preferred path**).
**--l2on is the session-11 default** (the L2 stays ON — --dmaquiet's
L2-off = the deterministic TLB-op wedge, see the session-11 notes):
**PAYLOAD_MODE=--l2on ./jump.sh zImage**.

## The debug loop

```bash
# 1. read the bc ladder + mirrors after the WDT2 reboot (jump.sh does this;
#    manual form):
$SSH "on -C 0 /tmp/memdump3 90000000 0x40"
# 1b. session-9 extended slots (DISPC readbacks, placement, pmd pattern):
$SSH "on -C 0 /tmp/memdump3 90000040 0x30"
# 2. read the console ring (count @0x88000080, chars @0x88000100; window 3840):
$SSH "on -C 0 /tmp/memdump3 88000080 0x4e0" > /tmp/ring.txt
# 3. decode memdump3 output: it prints words BIG-ENDIAN — pack each word
#    little-endian to get the bytes:
python3 - <<'EOF'
import re, struct
chars = bytearray()
for line in open('/tmp/ring.txt'):
    m = re.match(r'\s*([0-9a-f]+):\s+([0-9a-f]+)', line)
    if m:
        a, w = int(m.group(1),16), int(m.group(2),16)
        if 0x88000100 <= a < 0x88000100+0xF00:
            chars += struct.pack('<I', w)
n = 0x44a                      # the count from 0x88000080
print(chars[:n].decode('ascii','replace'))
EOF
```

## Post-mortem: dump kernel code/data from DRAM (the W-7 technique)

The decompressed kernel survives in DRAM until QNX tramples it — dump it
immediately after the reboot, before drawing conclusions:

1. `System.map` (in the kernel tree) gives the symbol's VA
   (e.g. `c00085c8 T __fixup_pv_table`).
2. Convert to the physical address the zImage path uses:
   **PA = VA − 0x20000000** (PHYS_OFFSET 0xa0000000 from auto-zreladdr; for
   the Image path, PA = VA − 0xc0000000 + kern_phys instead).
3. `$SSH "on -C 0 /tmp/memdump3 <PA> <len>"` and disassemble locally
   (capstone, below). Caveat: the 0xa0-0xa1 region is inside QNX's own
   allocation zone — regions at 0x88/0x90/0x94/0x9FE (the mirrors) are the
   reliable survivors.

## Disassembly

**Objdumps exist — use them first** (a session-7 correction: the earlier
"no ARM objdump on the host" claim only checked `PATH`):

```bash
# OUR kernel (buildroot toolchain — full symbols on vmlinux):
~/toolchains/armv7-eabihf/bin/arm-linux-objdump -d ~/kernel/linux/vmlinux
# QNX binaries (the SDP's objdump knows the QNX ELF flavor that reads as
# "architecture UNKNOWN" to the host x86-only objdump):
~/qnx660-master/host/linux/x86/usr/bin/arm-unknown-nto-qnx6.6.0eabi-objdump \
  -d device-binaries/trustzone-omap4
```

Capstone + pyelftools remain useful for **raw memdump3 dumps** (no ELF, just
a PA + word-reversed bytes) and scripted cross-binary sweeps:

```bash
pip3 install --user --break-system-packages capstone pyelftools
python3 - <<'EOF'
import re, struct
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
words = {}
for line in open('dump.txt'):                 # memdump3 output
    m = re.match(r'\s*([0-9a-f]+):\s+([0-9a-f]+)', line)
    if m: words[int(m.group(1),16)] = int(m.group(2),16)
addrs = sorted(words)
data = b''.join(struct.pack('<I', words[a]) for a in addrs)
for i in Cs(CS_ARCH_ARM, CS_MODE_ARM).disasm(data, addrs[0]):
    print(f"{i.address:08x}: {i.bytes.hex()} {i.mnemonic} {i.op_str}")
EOF
```

GAS gotchas that produced this tooling: pre-v7 baseline in some kernel
files (no `movw/movt`; `dsb` availability varies — session-04 note #8);
verify fixes in the SHIPPED binary, never the build log.

## Kernel patch snapshot (kernel-patches/)

The repo carries the kernel diff as a snapshot, regenerated from the
pristine tree (vanilla 6.15.11 at `/home/psyden/kernel/pristine`):

```bash
cd /home/psyden/kernel
OUT=/home/psyden/playbook-dev/kernel-patches/0001-playbook-winchester-6.15.11.patch
: > $OUT
# every MODIFIED file (add new files to this list when created):
for f in arch/arm/Kconfig.debug arch/arm/kernel/early_printk.c \
  arch/arm/kernel/head-common.S arch/arm/kernel/head.S \
  arch/arm/kernel/phys2virt.S arch/arm/kernel/setup.c \
  arch/arm/mach-omap2/omap4-common.c arch/arm/mm/dma-mapping.c \
  arch/arm/mm/init.c arch/arm/mm/mmu.c init/main.c \
  kernel/cgroup/cgroup.c kernel/taskstats.c mm/slab_common.c \
  arch/arm/boot/dts/ti/omap/Makefile; do
  diff -u pristine/$f linux/$f | sed -e "1s|--- pristine/|--- a/|" \
                                  -e "2s|+++ linux/|+++ b/|" >> $OUT
  echo >> $OUT
done
# every NEW source file:
for f in arch/arm/boot/dts/ti/omap/omap4-winchester.dts \
         arch/arm/include/debug/omap4bc.S; do
  diff -u /dev/null linux/$f | sed -e "2s|+++ linux/|+++ b/|" >> $OUT
  echo >> $OUT
done
cp linux/.config /home/psyden/playbook-dev/kernel-patches/winchester.config
# then: git add kernel-patches && git commit && git push
```

Apply on a fresh tree: `git apply --check` first, then `git apply` (see
`kernel-patches/README.md`). After each kernel change worth keeping:
regenerate + commit — the snapshot is the repo's source of truth for the
kernel, the tree itself is machine-local.

## Kernel build

```bash
cd /home/psyden/kernel/linux
make -j12 ARCH=arm CROSS_COMPILE=/home/psyden/toolchains/armv7-eabihf/bin/arm-linux- zImage dtbs
cd ~/playbook-dev/kexec && ./mkkernel.sh zImage     # packs zImage + DTB
```

- **Never bare-`make` the tree** (a host syncconfig can mangle .config;
  recovered via `scripts/extract-ikconfig arch/arm/boot/Image`).
- Verify fixes in the shipped binary (zImage size delta is the fastest
  "did it rebuild" check; disassemble for certainty).

## Git / GitHub

```bash
cd ~/playbook-dev
git add -A && git commit -m "..." && git push   # remote origin = private repo
```

- Remote: `https://github.com/Psyden57/BB-playbook-linux` (private;
  HTTPS + classic PAT, credential.helper store already on).
- The kernel tree and device dumps are OUTSIDE the repo — the repo's
  kernel state = `kernel-patches/` (regenerate after changes).
- Secrets stay out: `rsa`/`rsa.pub`, device identifiers (redacted).

## Device-side facts (quick recall)

- Boot to SSH: 2-3 min after reset — don't conclude "hang" early.
- `/tmp` is wiped per reboot: deploy everything, every cycle (jump.sh does).
- `on -C 0` pins to CPU0; `dd` needs numeric bs + `sync` after.
- Device clock: was frozen (~2021) — the user NTP-synced it over WiFi once
  (2026-09-04); do NOT rely on the clock yet (one observation). The payload
  nonce mixes the staging phys — keep that design regardless.
- Failed payload runs leak their 24 MB buffer by design — expect a reboot
  every ~10-12 failed placements.
