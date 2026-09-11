# Bootrom RE — session 10 (2026-09-11)

Subject: the TI OMAP4430 boot ROM, 48 KB at 0x40028000-0x40033FFF
(`omap4430-bootrom-0x40028000.bin`, fresh from-device dump, post-battery-pull).
Tools: capstone (ARM + Thumb sweeps, alignment-search), byte-scan for SMC
encodings (E1600070 + Thumb patterns), literal-pool extraction.

## HEADLINE: the documented monitor service table includes the L2 LATENCY
## service — session 7's "no tag/data-latency service exists" is WRONG

TRM SWPU231AP (public, rev. April 2014), §27.5 "Services for HLOS Support"
("The ROM code provides different services that can be called on **GP
devices** for L2 cache maintenance, wake up of slave CPU(s), etc. These
services are implemented in monitor mode (the service must be called by
writing the **function ID into the R12 register** and using the SMC
instruction)"):

| R12 | Service | Params |
|-----|---------|--------|
| 0x100 | Set L2 Cache **Debug** register | r0 = value |
| 0x101 | **Clean+invalidate a range of physical addresses** ("a workaround of the clean and invalidate L2 cache") | r0 = start PA, r1 = size |
| 0x102 | Enable/disable L2 (Control register) | r0 = value |
| 0x103 | Read AUX_CORE_BOOT_0/1 | → r0/r1 |
| 0x104 | Modify AUX_CORE_BOOT_0 (set/clear bits) | r0=set, r1=clear → r0 |
| 0x105 | Write AUX_CORE_BOOT_1 (CPU1 boot addr) | r0 |
| 0x106 | Read WKG_CONTROL_0/1 | r0 = CPU id |
| 0x107 | **Clear WKG_CONTROL_0/1 bits** | r0 = CPU id, r1 = mask |
| 0x108 | SCU Set Power Status (+L1 clean if OFF) | r0 = state (2=CSWRET, 3=OFF), r1 = L1 state (00=RET, FF=OFF) |
| 0x10A-0x111 | Lockdown TLB entry select/read/write (VA/PA/attrs, NS forced 1 on writes) | r0 (+r1) |
| **0x112** | **Write the PL310 Tag and Data RAM Latency Control Registers** | **r0 = tag latency, r1 = data latency** |
| 0x113 | Write PL310 Prefetch Control register (ES2.2+) | r0 |

**ERRATUM (dated 2026-09-11):** newdocs/PROJECT_STATE.md's "the documented
0x100-0x113 table is complete for L2: there is NO tag/data-latency service"
(session 7, derived from QNX-binary RE) is contradicted by the TRM's own
Table 27-61 — **0x112 writes the Tag AND Data RAM latency registers, r0/r1**.
The observed QNX latency value (payload readback bc[4]=0x111 = 1/1/1) is
exactly what this service sets.

**ON-DEVICE TEST (session 10, 2026-09-11, smctest ladder, no jump):**
1. Baseline 0x103 from System mode ANSWERED (ret r0=2 — the monitor is
   alive; AUX_CORE_BOOT_0=2 consistent with a released CPU1).
   PL310 readback intact: ctrl=1, aux=1e070000, tag(0x108)=0,
   data(0x10C)=0x111, prefetch=0.
2. 0x112 accept-test (SAME values — no functional change): the caller
   **external-aborted** (SIGBUS fltno=5, ref=0716bff0) — the monitor took
   an exception path on 0x112, NOT a rejection return. **The latency fix
   path via 0x112 from NS is CLOSED on this HS unit** (worse than the PPA
   0xFF02 rejections: it aborts). L2 state verified intact after the
   abort (ctrl=1, aux=1e070000, tag=0, data=0x111).
   Box survived fine; no WDT2 cycle needed. Payload: kexec/smctest.c
   (extended with the `--lat112`-style ladder).
3. Anomaly: the first memdump3 read of aux(+0x104) read 0x1e340340
   (= the +0x004 CACHE-TYPE value), the read minutes later = 1e070000
   (the stable value, matching session-7's PPA-1). Suspect: a devpm
   suspend/resume between the reads rewrites/restores L2 aux (devpm's
   0x26/0x27 pair). NOTE for future runs: aux state can apparently vary
   mid-QNX; take readbacks close in time.

**Why this matters**: the L2 latency config is the shared variable of
everything unexplained: the device-op cliff (SO stores wedge at ~4-5k ops
under QNX's 1/1/1 config), the 11 GB probe-loop stall, the PTW
stale-view behavior. The W-33-era conclusion "fixing QNX's L2 latencies
via a monitor service (the session-6 0x112/latency lead)" was on the
right track and was dropped on session 7's (wrong) negative RE.

## The ROM's own SMC sites (byte-scan, both modes; 4 total, all ARM mode)

| Site | Service | Context |
|------|---------|---------|
| 0x40028134 | `movw ip, #0x107; smc #0` | CPU1-wakeup path: `mrc c0,c0,5` cpunum, `wfe` loop, flag at [r0] bit 0x200, r0=1/r1=0x200 args |
| 0x4002813c | `movw ip, #0x103; smc #0` | same path, chained (read AUX_CORE_BOOT_0/1) |
| 0x400289c8 | `movw ip, #0x107; smc #0` | second CPU1 path variant (polls [r3,#0x400] bit 0x200, then `ldr r0,[r3,#0x800]`) |
| 0x4002ceb8 | `mov ip, #0xF0; smc #0` | UNIDENTIFIED — not in the 0x100 table; ARM/Thumb interleaved region (near the 0x4002ce9c Thumb cluster: `push {...}`/`cbnz`/`bx lr`) |

- Convention confirmed at the ROM's own call sites: **service id in
  ip/r12, `smc #0`** — matches the payload's `mon_call` shape exactly.
- No Thumb-mode SMC anywhere (two sweeps agree).
- No direct PL310 access in the ROM: no full-word 0x4A32xxxx/0x4824xxxx
  constants; no movw/movt pairs constructing them. L2 work routes through
  the services above (0x100/0x101/0x102/0x109/0x112/0x113).

## Data regions / boot-chain manifest

- **String table at 0x4002ff00+**: `R&D`, `2ND`, `CHMMCSD`, `CH`,
  `CHFLASH`, `CHRAM`, `HLO`, `MLO`, `ULO`, `PRIMAPP`, `X-LOADER`,
  `CHSETTINGS`, `KEYS`, `ISSW` — the chain stages/settings-section names
  the ROM searches the boot media for (matches `bootblob_MLO.bin`,
  `bootblob_PRIMAPP.bin`, `bootblob_KEYS.bin` in device-binaries/, and the
  TRM's CHSETTINGS/CHRAM/CHFLASH/CHMMCSD tracing-vector semantics:
  "CHSETTINGS found/executed", "CHRAM executed", "CHMMCSD clocks/bus
  width executed").
- **Component tables right after** (0x4002ffc0+): {size, addr, …} triplets
  with **IRAM handoff pointers 0x4030d0xx** (0x4030d000/0x4030d100
  seen) — the ROM's IRAM workspace, same region the hd0 0x9034 stub and
  the bootstub signature at ROM entry (0x40028020: pointers
  0x4030d004-0x4030d01c) reference.
- **Pointer ladder at 0x40030000**: 8× `ldr pc,[pc,#0x18]` + target words
  0x28090/0x28084/0x289e4/0x29420… = the ROM self-referencing its
  **0x00028000 boot-alias** (Q0 boot space) — consistent with the TRM's
  boot redirection of the 48-KB window.
- **0x4A3208A0** pool word: inside the SAR-space block
  (0x4A320000-0x4A325FFF "Reserved", 0x4A326000+ = SAR space 1 — the
  CPU1 trampoline region at 0x4A326B00). The ROM's secure
  context-save/resume path touches SAR RAM, not L2.

## PRCM/ clock code (Thumb, ~0x4002b660-0x4002b6f0 and 0x4002b9a8+)

- A **CM module clock-control family**: pointer table indexed by module
  id (`ldr.w r1,[r2,r0,lsl#2]`), then `[r1,#0x10] |= 2` (CLKCTRL ENABLE)
  / `[r2,#0x40] &= ~4` style ops — the ROM turns clocks on/off itself
  via a table of CM base addresses.
- `movw r2,#0x2101; str r2,[r3,#0x24]` patterns — register writes with
  0x2101 at +0x24 (not WDT SPR values; unidentified, likely a PRM/WKUP
  register family).

## Open RE threads (next passes)

1. Decode the {size, addr} triplet tables fully (0x4002ffc0-0x40030120)
   and the strings' consumers — the exact CHSETTINGS section parser and
   what config blocks it applies (clocks? EMIF? anything L2?).
2. Identify the ip=0xF0 service at 0x4002ceb8.
3. The registry blob (cookie `0xD7B02D1F @base+0x34`, id 0x18 "Bootrom's
   starting address") is still unfound in hd0/hd4/hd5 — try deeper hd0
   offsets or a live IRAM dump (0x4030d000 area) at runtime.
4. Cross-check the ROM's CPU1-wakeup flow against ARCHITECTURE.md's
   SAR-RAM trampoline notes (0x4A326B00-CD0, monitor-verifies saved
   context via 0x26/0x27) — the ROM's own 0x103/0x107 sites are the
   ROM-side of that same flow.

## ADDENDUM 2026-09-11 (session 10, evening): the ROM is TWO blocks + a 4 KB
## hidden hole — the monitor dispatch region

- 0x40020000-0x40026F24: readable ROM block (~28 KB) — discovered by
  probing below the documented 48 KB window. Ends mid-dump at
  0x40026F24 (the next word SIGBUS'd = unmapped/secure).
- 0x40026F28-0x40027FFF: **~4 KB non-readable hole** — matches the TRM's
  `STD_FUSE_ROM_HIDE` note ("BOOT ROM upper 4-KB region" protection).
  **This hole is the prime candidate for the SMC dispatch region** —
  which is why no dispatch was found in either visible block, and why
  NS callers of unimplemented-for-NS services (0x112) abort instead of
  returning a code.
- 0x40028000-0x40033FFF: the 48 KB window (the original dump).
- 0x40034000+: SIGBUS (boundary confirmed twice).
- **0x40026098-0x400260e8 = the secure handoff tail** (ARM): NSACR = 0x20C00
  (CP10/CP11 granted to NS), **MVBAR set from a literal pool pair**
  (`ldr r4,[pc,#0x80] − ldr r5,[pc,#0x80]` computed base →
  `mcr p15,c12,c0,1`), VBAR = 0 (`mcr p15,c12,c0,0`), SCR = 0x30,
  then `mov ip,#9; smc #0` and an infinite `b self` — service **0x9**
  (outside the 0x100 table; the handoff's final call). This is the
  ROM's exit-to-boot-chain machinery, not HLOS monitor code.
- Captures: `bootrom-lower-0x40020000.memdump3.txt` (28 KB block).
- Service table status after the 0x112 test: 0x100/0x101/0x102/0x103/
  0x106-0x108 work (or are proven) from NS; **0x112 = aborts**; 0xF0
  unidentified. The latency reconfiguration has no NS path; the only
  remaining leads are the hidden 4 KB (needs a secure-context dump —
  not available from QNX NS) and the audit's NS-register experiments
  (0x770 invalidate-by-PA remains untested and unblocked).
