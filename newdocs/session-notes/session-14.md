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
(the W-95 question, re-asked on a cold machine).
