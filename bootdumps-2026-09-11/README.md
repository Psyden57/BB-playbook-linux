# Boot dumps — 2026-09-11 (session 10)

Fresh dumps taken from the booted, rooted device over SSH (`memdump3` v2 /
dd numeric-bs + sync). Provenance verified: the device itself, after the
user's full battery-pull hard reset (DRAM + RTC wiped, ~1 min unplugged) —
no possible residue from earlier sessions.

Files:

| File | What | Provenance |
|------|------|------------|
| `hd0head.bin` | `/dev/hd0` first 2 MB (dd bs=65536 count=32) | fresh dd, 2026-09-11 |
| `hd4.bin` | `/dev/hd4` first 128 KB — starts with the QNX IFS magic 0x4A56BE92 = the **boot IFS** on a boot hw partition | fresh dd, 2026-09-11 |
| `hd5.bin` | `/dev/hd5` first 128 KB — ASCII manufacturing/device data ("88 10PRU2QCIK014M03D23 …"), nonzero extent 0x0-0x10206 | fresh dd, 2026-09-11 |
| `omap4430-bootrom-0x40028000.bin` | the TI boot ROM, 48 KB at 0x40028000-0x40033FFF (TRM Table 2-1 "Boot ROM internal"), memdump3 output raw | fresh memdump3, 2026-09-11 |
| `omap4430-bootrom-0x40028000.memdump3.txt` | the memdump3 word-per-line capture (u32 values as printed) | raw |

`/dev/hd6` = NOT dumped: the devctl read failed ("Inappropriate I/O
control operation") — almost certainly the RPMB hw partition.
**RPMB is untouchable (brick hazard); do not retry.**

Reliability check (the "are old dumps trustworthy?" question): the old
`device-binaries/emmc-first1mb.bin` is **byte-identical** to today's fresh
`hd0head.bin` over its full 1 MB (cmp = 0 diff bytes) — previous hd0-chain
dumps (bootstrap_loader etc.) stand on verified-fresh data.

## Bootrom facts (first pass, capstone)

- NS-readable from QNX (`memdump3 0x40028000`), 32-bit Ex/R per TRM.
- Upper boundary: read at 0x40034000 = SIGBUS fltno=5, **box stayed alive**
  (the freeze in W-28 was MMCHS-region-specific, not the abort class per
  se). Read at 0x00000000 = SIGBUS too (GPMC space unmapped at runtime).
- Entry word 0xEA000242 (branch to +0x910), then a `ldr pc, [pc,#0x18]`
  exception ladder with IRAM pointers 0x4030d004-0x4030d01c — the same
  stub signature as the hd0 0x9034 image header.
- Strings in the ROM: `CHSETTINGS`, `CHFLASH`, `CHMMCSD` (the CH-settings
  parsing family), `X-LOADER`, `MLO`, `PRIMAPP` (boot-image names it can
  load), plus the 0x4002ce9c-region Thumb code (KDS/crypto per the §5.6
  registry-note context).
- **SMC sites (4 found, E1600070 encoding byte-scan):**
  - 0x40028134: `movw ip, #0x107; smc` — CPU1-wakeup path (cpunum +
    `wfe` loop, SCU-hosted flag at [r0] bit 0x200)
  - 0x4002813c: `movw ip, #0x103; smc #0` (same path, chained)
  - 0x400289c8: `movw ip, #0x107; smc #0` (second CPU1 path variant)
  - 0x4002ceb8: `mov ip, #0xF0; smc #0` (ARM/Thumb interleaved region)
- The service-in-**ip (r12)** convention is CONFIRMED at the ROM's own
  call sites — matches the payload's `mon_call` shape.
- Services **0x103, 0x107, 0xF0 are UNDOCUMENTED** (outside the
  0x100-0x113 L2X0 table session 7 mapped from QNX binaries). Full-table
  extraction = the follow-up RE task.

## TODO (RE queue)

1. Full ROM RE: Thumb+ARM sweep, every SMC site + ip immediates (raw
   byte-scan, capstone per-region mode).
2. CHSETTINGS/CHFLASH/CHMMCSD parser RE — what the chain configures
   before QNX (L2 latencies? the 1/1/1 source).
3. The registry blob (cookie `0xD7B02D1F @base+0x34` with id 0x18
   "Bootrom's starting address") is still NOT found in these dumps
   (hd4 = IFS, hd5 = manufacturing data) — try deeper hd0 offsets or the
   hd4 IFS internals. (The 0x40028134-region pointer 0x4030d004 = the
   IRAM handoff area, worth dumping live too.)
