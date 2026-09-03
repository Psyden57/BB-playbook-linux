# kexec Architecture Deep Dive

## 2026-09-02 NIGHT UPDATES (session 6 — SUPERSEDES the sections below where they conflict)
1. **The L2-off mode is RETIRED.** `--l2test` A/B: device stores with the L2 ON
   = clean; the identical stores with L2 OFF = machine wedge. The "171 wall"
   = the bypass traffic cliff + the head.S C/B/S strip (the kernel ran
   UNCACHED — the 120-wall). The mode = **--l2on** (the payload skips the
   0x102 disable; bc[10] = the PL310 CTRL readback = 1 proves the state).
2. **The kernel is CACHEABLE now.** head.S's C/B/S strip (an M=1-era
   diagnostic) is REMOVED: kernel RAM = WBWA via r7; the diagnostic sections
   0xFEB/0xFEC/0xFED/vectors = DEVICE via r6; the ring/bc sections
   0xC80/0xD00/0xD40 = CACHEABLE via r7. ACTLR.SMP stays 1 (clearing it
   wedged boots). Sustained DEVICE (SO) stores wedge the machine with the L2
   on — never reintroduce per-char device-store consoles.
3. **All flushes = pb_smc_flush = monitor SMC 0x101** (C-flow shape, r12=0x101,
   r0=PA, r1=size, synchronous — no sync polls). pb_bc_put = store + guarded
   SMC flush. pbmarkv (head.S) = DCCMVAC+CIPA+BOUNDED poll. **Boot-wide audit:
   ZERO unbounded polls** (the setup_arch LED-off/LED-on blocks were the last;
   both now pb_bc_put). The old "flush-free markers" claim (below) is
   superseded: with the L2 on, flush-free stores strand dirty in L2 and die in
   the WDT2 reset.
4. **Console = RINGS ONLY.** senduart and the LSR drain are REMOVED from
   omap4bc.S: UART3 dies post-idle (L4PER auto-idle) and a posted store to
   the dead THR stalls the store buffer (the next ring load hangs — the
   "132-wall"), and a dead-target read stalls forever. The monitor's UART3
   capture (ring3) is INACTIVE. ring1 (0x88000080 count / +0x100 chars,
   3840-char window) = the primary console evidence. The ring_puts are
   CACHEABLE stores; early_write flushes all three ring windows per 128-byte
   chunk via pb_flush_rings (3 × SMC 0x101). memdump3 output = BIG-ENDIAN
   words: reverse each 4-byte group when decoding.
5. **The "0xFED was never mapped" claim (item 4 below) was WRONG** — the
   pristine DEBUG_LL block in head.S always mapped 0xFED via addruart. The
   real console killers were the flush machinery and the UART3 store.
6. **CONFIG_CACHE_L2X0=y now** (+ the pl310 DTS node @ 0x48242000, tag/data
   latency <3 3 3>). WARNING UPDATE 2026-09-03: the "guard the driver against
   the NS latency SIGBUS" advice is WRONG — the latency writes already route
   through omap4_l2c310_write_sec (default = WARN+skip), no patch needed. The
   REAL hazard when the boot reaches l2x0_of_init: l2c_enable's by-way write
   to L2X0_INV_WAY (0x7FC = the on-device deadlock op) + the l2c_wait_mask
   poll. See 07_TROUBLESHOOTING and docs/03 session 2026-09-03.
7. **The corruption root cause = the secure-domain L2 config.** PL310 data
   latency = 0x111 (1/1/1) live; L3/EMIF auto-idle = also secure-only. Every
   "wild write" (0x1f7f0000 mapping VA, the corrupted dma_mmu_remap[0].base,
   the pv-patch deltas) = this class, placement-dependent. bc[2]=0x3E7 (999)
   in readbacks = QNX-boot leftovers, NOT a kernel wild write.
8. **The current wall**: bc=127 (paging_init: map_kernel done) →
   dma_contiguous_remap maps a corrupted dma_mmu_remap[0].base (0xbe800000 →
   0xa1000000) → `BUG: not creating mapping for 0xa1000000 at 0x1f7f0000` →
   the CMA unmapped → abort. Deterministic, both L2 modes. The fix = the
   monitor RE (a latency/PPA service) — see 05_NEXT_STEPS.md.
9. **The marker ladder below (item 7) is historical** — the current ladder =
   payload → head.S 100–124 → 120/125/126 (mmap_switched/bss) → 117 →
   start_kernel ACTLR (bc[3]) → 118 → setup_arch 136/130/131/132 →
   parse_early_param probes (bc[10..14]: cmdline head/len, handler marks,
   the latch probe) → 133/134 → paging_init PB_MMU_BC 134/133/127/126/125/
   129/128 → 110/111 (LED off/on) → cgroup/taskstats/slub markers (now
   reachable past the wall).

## 2026-09-02 UPDATES (the L2-disable era — read after the 09-01 updates)
1. **L2 disable WORKS — the C-flow mon_call shape only.** SMC #0, r12=0x102,
   r0=0 (L2X0 CTRL := 0). Reliable (4/4) when called from do_t3's C flow via
   `mon_call(0x102, 0, 0)` post-CPU1-hold; the enter_stub-inline shape hung
   9/9 in every context (full matrix in 03). The C-flow call is made right
   after the L1 clean (bc 32), with GICD still on and IRQs on; bc markers 51/52
   bracket it; readbacks: bc[2]=CTRL (expect 0), bc[4]=dlat (stale), bc[6]=status.
2. **WDT2 register truth** (drivers/watchdog/omap_wdt.h): TGR 0x30 (trigger —
   ANY write reloads CRR from LDR; the written value is ignored), SPR 0x48
   (start/stop: 0xBBBB/0x4444 = start, 0xAAAA/0x5555 = stop), CRR 0x28, LDR 0x2C,
   CLR 0x24 (prescaler). LDR = 0xFFE2B400 → **58.6 s** @ 32.768 kHz, PTV=0.
   The payload's old `~WTGR` complement-write = a valid TGR write ✓.
3. **WDT2 section PA-encoding bug (FIXED run 30)**: the head.S 0xFEC section
   (VA 0xFEC00000 → WDT2) had copied the 0xFEB pattern's `orr r3,r3,#0x200`
   — but that 0x200 carries PA bits (0x48200 → PA 0x48200000); with it the
   WDT section decoded to PA 0x4A500000 and every VA-based kick (pb_bc/
   pbmarkv/PB_MMU_BC) was a no-op. All "died at marker X" runs 17–29 were
   the 58.6 s window expiring wherever the kernel was — the kernel was alive.
   PA-bit discipline: in section setup, `orr` values BEFORE `lsl #12` are PA
   bits; r7 (mmuflags) is or'd AFTER the lsl.
4. **The 0xFED UART section (FIXED run 32)**: omap4bc.S's addruart returns
   UART virt 0xFED20000 but head.S never mapped section 0xFED — every MMU-on
   console write hit unmapped VA 0xFED20014 and the L3 swallowed it (async
   error, no abort taken) → ALL console output after early boot vanished
   silently (runs 24–31). head.S now maps 0xFED00000 → 0x48000000.
5. **Kernel console = earlycon0 = early_printk.c → printascii → omap4bc.S**
   (the PlayBook DEBUG_LL — NOT omap2plus.S): every char goes to UART3
   (0x48020000) AND three DRAM rings via busyuart's ring_put (ring3 0xD4000080,
   ring2 0xD0000080, ring1 0xCC000080 — see the ring1 caveat below), plus a
   PL310 CIPA+sync per touched line. **ring3 (PA 0x94000100) is the secure
   monitor's UART3 capture buffer** — persistent across resets, the primary
   post-mortem evidence. CAVEAT: ring1's VA in omap4bc.S (0xCC000080) maps to
   PA 0x8C000000 — an unmapped alias (the real ring1 = PA 0x88000000); those
   writes are silently dropped by the L3. CAVEAT 2: runs 24–31 emitted zero
   UART3 bytes (console dead pre-0xFED-fix) — run 23's kernel emitted 23 KB
   (preserved in ring3; recovered to SESSION-HANDOFF/ring3-recovered-log-2026-09-02.txt).
6. **Payload SIGSEGV ≠ reboot** (run 31): a user-mode abort in the payload
   (e.g. the PRCM CLKSTCTRL write — the PRCM is secure-filtered) kills the
   process; QNX survives; SSH stays up. Only jump-context deaths (post-GICD-off)
   reboot. PRCM CM1/CM2 registers are secure-filtered: the auto-idle disable
   via direct NS writes is IMPOSSIBLE (removed from the payload after run 31).
7. **Marker ladder (current)**: payload 31–53 (+51/52 around the L2 disable) →
   enter_stub 70 (mirror0) → cont 21/211/212/213 → 23 → probe 50–59 → kernel
   100–124 (stext/mmu) → 118/150/151/142/144/145/143/140/141 → setup_arch
   136/130/131/132/135 → paging_init 134/133/127/126/125/99/98/97/96/95/129/
   128 → 110/111 → cgroup 160–170 → 171 (post cgroup_init) → 176–180 (fine
   bisect) → 186/187 (kernel_init_freeable/do_basic_setup) → initcall
   breadcrumbs bc[6]=level, bc[7]=fn, bc[2]=0xBEEF. **The kernel deterministically
   stops at 171** (post cgroup_init) — inside taskstats_init_early →
   kmem_cache_create (before the SLUB mutex; markers 176–186 never land).
   bc[2]=0x3E7 (999) co-occurs — unexplained wild write.
8. **Run nonce**: bc[15] (0x9000003C) = time(NULL)^phys, written at bc=39-era.
   ALWAYS check it against the previous readback — the ring/bc content
   persists across reboots and stale values are the #1 misread risk.

## 2026-09-01 UPDATES (read after the original sections below)
1. **Ring-map fix (THE M=1 fix)**: head.S `__create_page_tables` PlayBook block
   must shift its PA>>12 values (`0x88000`, `0x90000`, `0x94000`, `0x48200|0x200`,
   `0x9F000|0xE00`) with **`lsl #12`** — NOT `lsl #SECTION_SHIFT` (=20). The old
   build truncated the shifted values to 0, mapping rings/PL310/trap-vectors to
   PA 0 (boot ROM). Corrected descriptors: tt[0xC80]=0x88000C12, tt[0xD00]=
   0x90000C12, tt[0xD40]=0x94000C12, tt[0xFEB]=0x48200C12, tt[0x000/0xFFF]=
   0x9FE00C02 (executable). NOTE: classic head.S code uses SECTION NUMBERS
   (PA>>20) with lsl #SECTION_SHIFT — both idioms work; never mix them.
2. **Flush-free breadcrumb channel PROVEN**: plain SO stores to the bc page
   (0x90000004/8) reach DRAM with NO CIPA/SMC. All fine markers (pb_bc v6 in
   init/main.c + arch/arm/kernel/setup.c) are flush-free stores + dsb only.
   The old-style full-flush blocks (SMC/CIPA/poll) were themselves intermittent
   wedge points — never put them in a bisection path.
3. **Panic visibility**: a panic notifier (init/main.c, pb_panic_nb) dumps
   "PB-PANIC: <msg>" into the rings via early_print/printascii — works
   pre-console. Rings clean = no BUG/panic fired.
4. **Start_kernel ladder**: 118 → 150 → 151 → 142 → 144 → 145 → 143 → 140 →
   141 → 136 (setup_arch) → 130-135 → 110 → 111. cgroup_init_early bisect
   markers: 160/161/0x100+i/0x120+i in kernel/cgroup/cgroup.c, 164-170 in
   cgroup_init_subsys.
5. **PL310 from NS**: config registers (control 0x100, aux 0x104, latency
   0x108/0x10C) are SECURE-FILTERED — NS control write = bus hang, NS latency
   write = synchronous SIGBUS. Maintenance by-PA (0x768) + sync (0x730) are
   NS-writable; by-way (0x7FC) is a BACKGROUND op that deadlocks — NEVER use
   it from NS here. Monitor SMC 0x105 is NOT "L2 disable" (returned 0, no
   effect, and broke the jump chain — reverted). L2 config must go through
   the real monitor services (RE pending).
6. **Kernel built with CONFIG_CACHE_L2X0=n** (SUPERSEDED — session 6 enabled
   CONFIG_CACHE_L2X0=y + the pl310 DTS node; see the NIGHT UPDATES above) —
   the kernel never touches PL310 config; runs L1-only until L2 is properly
   enabled via the monitor later.
7. **WDT2 window = 60 s** (user-timed twice, exact), NOT 15s. The "15s" in
   older comments = QNX's wdtkick period.

## Overall Flow (qnx2linux.c)

```
qnx2linux (--t3 zImage dtb [probe])
    ├─ ThreadCtl(_NTO_TCTL_IO_PRIV)  → System mode (CP15/SMC access)
    ├─ Map bc page (0x90000000) + 3 mirrors via mmap_device_memory
    ├─ bc_write(STEP_ARMED=0) + sanitize DEBUG_LL ring headers
    ├─ led_blue_qnx()  (I2C3 FAN5702 blue LED)
    ├─ wdt2_kick()  (early — fresh 15s window for multi-second setup)
    │
    ├─ Read files: zImage, DTB, optional probe.bin
    │
    ├─ Place 24MB buffer (MAP_ANON|MAP_PHYS|PROT_EXEC)
    │   ├─ Try requested phys: 0xA4000000, 0xA8000000, 0xAC000000
    │   ├─ Fallback: 12 retries, leak rejected buffers (prevent allocator reuse)
    │   └─ buf_placement_bad() guards:
    │       • VA/PA 1MB sub-offset match (trampoline alias requirement)
    │       • ±4MB around bc/ring pages (0x88/0x90/0x94/0x9FE)
    │       • > 0xBE000000 (top of RAM — decompressor relocates there)
    │
    ├─ NOCACHE map (mapdev) → memcpy zImage@+0x8000, DTB appended, verify
    ├─ bc_write(STEP_IMAGE=10)
    │
    ├─ IRAM setup (mapdev 0x40304000, 0x8000 bytes):
    │   ├─ Zero 16KB flat L1 table
    │   ├─ Identity sections: DRAM 0x800–0xBFF (AP=11, section), Peripherals 0x400–0x4FF
    │   ├─ IRAM identity @ 0x403
    │   ├─ Buffer VA → buffer PA section (identity for post-switch continuation)
    │   ├─ Copy trampoline (stub3.S) to buf+0x800 (pre-switch entry)
    │   ├─ Copy continuation (cont_start..cont_end) to IRAM 0x40308000
    │   ├─ Optional probe.bin to IRAM 0x40309000 + params @ 0x40309800
    │   └─ bc_write(STEP_STUB=11)
    │
    ├─ bc snapshot + wdt2_kick() (late — kernel's own full window)
    ├─ Re-verify EVERYTHING via NOCACHE/IRAM views (catches stray DMA corruption)
    │
    ├─ NO I/O PAST THIS POINT (console dies at CPU1 hold)
    ├─ Hold CPU1 in warm reset (RSTCTRL_CPU1=1)
    ├─ clean_inval_l1_all()  (single-core now — no cross-cache hazards)
    ├─ GICD off (GICD_CTLR=0)  — LAST kernel call
    ├─ Bare-register WDT2 kick (no kernel calls after GICD off)
    │
    └─ enter_stub():
        r8  = TTBR0 (0x40304000)
        r9  = entry (phys+0x8000 or probe@0x40309000)
        r10 = bc QNX vaddr (pre-switch)
        r11 = bc phys (0x90000000, post-switch)
        r12 = continuation phys (0x40308000)
        r7  = DTB phys
        r6  = trampoline vaddr (buf+0x800)
        cpsid if; dsb; bc=70; blx r6  → never returns
```

## Trampoline + Continuation (stub3.S + cont_start)

**Trampoline** (buf+0x800, executed pre-switch via buffer's QNX vaddr):
```
bc=61; TTBCR=0; bc=62; DACR=client; bc=63; TTBR0=flat_table; bc=64;
TLBIMVA(cont_vaddr); bx r12  → continuation (phys)
```

**Continuation** (IRAM 0x40308000, identity-mapped, entered at phys):
```
bc=21; TLBIALL; M=0; C=0; I=0 (SCTLR); bc=23; bx r9  → kernel/probe entry
```

## Breadcrumb Protocol

| bc[1] | Step | Meaning |
|-------|------|---------|
| 0 | ARMED | Payload started, bc initialized |
| 10 | IMAGE | zImage+DTB copied & verified |
| 11 | STUB | Trampoline+continuation+probe placed |
| 19 | L2 | L2 flush done (T2 only) |
| 20 | JUMP | enter_stub called |
| 21 | IDENTITY | Continuation running, TLBIALL done |
| 23 | MMUOFF | MMU/C/I disabled, about to branch to target |
| 30 | TARGET | Blob/kernel/probe running |
| 31 | PLACED | Buffer placed (T3) |
| 37 | CPU1 | CPU1 held in reset |
| 39 | NONCE | bc[3]=phys (per-run nonce) |
| 40 | SNAP1 | File snapshot 1 |
| 41 | SNAP2 | File snapshot 2 |
| 60–67 | MEMTEST | Memtest payload steps (see memtest doc) |

**Mirrors**: 3 survivor-band pages (0x94000000, 0x88000000, 0x9FE00000) — scatter test proved they survive full jump+WDT2+QNX-reboot cycle.

**Ring headers**: bc[0x80/4]=count, bc[0x84/4]=index — sanitized to 0 to prevent wild strb on kernel's first print.

**Abort trap** (cpt maps VA 0x00000000 + 0xFFFF0000 → PA 0x9FE00000):
- Prefetch/data abort vectors → handler writes 0xAB to bc[1] (VA 0xD00000A0), PL310 CIPA flush, spins
- Post-reboot read of 0x90000004 (bc[1]) ≠ 0 ⇒ abort fired after MMU-on

## Page Table (Flat L1 @ IRAM 0x40304000)

```c
// DRAM 0x80000000–0xBFFFFFFF: 1MB sections, identity, AP=11 (RW), CB=00 (strongly ordered? No: 0xC02 = AP=11, section, C=0,B=0? Wait.)
// Descriptor: (PA & 0xFFF00000) | 0xC02
// 0xC02 = 0b11000000010 = AP[10:9]=11 (full access), XN=0, domain=0, C=0, B=0, type=10 (section)
for (v = 0x800; v < 0xC00; v++) tt[v] = (v << 20) | 0xC02;

// Peripheral domain 0x40000000–0x4FFFFFFF
for (v = 0x400; v < 0x500; v++) tt[v] = (v << 20) | 0xC02;

// IRAM identity
tt[0x403] = 0x40300000 | 0xC02;

// Buffer VA section → buffer PA section
tt[buf_va >> 20] = (phys & 0xFFF00000) | 0xC02;
```

## Cache Maintenance

- **L1**: `clean_inval_l1_all()` via CP15 (cacheops.S) — by set/way, privileged
- **L2**: Monitor SMC #0 service 0x101 (r12=api, r0=phys, r1=size) — clean+invalidate by phys range
  - Called for: jump buffer, breadcrumb pages, IRAM ranges
- **Order**: L1 clean+invalidate → L2 range flush → GICD off → WDT2 kick → enter_stub

## WDT2 Strategy

- QNX's `wdtkick` runs every 15s, writes complement of WTGR (0x4A314030)
- **Early kick** at bc=ARMED (before file reads + 24MB copies) — covers setup
- **Late bare-register kick** after GICD off, before enter_stub — covers kernel
- Without early kick: random WDT2 deaths during early head.S (observed at bc 102/107/115/119)

## Earlyprintk Fix (2026-08-31)

Kernel's earlyprintk banner flush (CON_PRINTBUFFER, parse_early_param) runs **BEFORE** paging_init → hits unmapped UART/PL310 VAs → abort.
**Fix**: Rebase debug VAs to section-compatible addresses:
- UART: 0xFEB20000 → 0xFED20000
- PL310: 0xFEB21000 → 0xFEB42000
- head.S early-maps PL310 section (0xFEB00000 → 0x48200000)
- debug_ll_io_init re-maps same VAs later — seamless

## zImage vs Uncompressed Image

**Uncompressed Image at arbitrary address is architecturally broken**:
- head.S derives PHYS_OFFSET = load_address - 0x8000
- If load_address ≠ RAM base (0x80000000), PHYS_OFFSET ≠ 0x80000000
- Linear map ≠ PA + 0x40000000 → bc/ring VAs wrong post-paging_init
- Sub-PHYS_OFFSET RAM breaks memblock

**zImage is the path**:
- Decompresses to zreladdr 0x80008000
- PHYS_OFFSET = 0x80000000
- Everything consistent
- Reached bc=119/122 (decompressor works; kernel enters)