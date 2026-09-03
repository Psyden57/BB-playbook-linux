# Memtest Payload — Standalone DRAM Integrity Test

## VERDICT (2026-09-01): EXECUTED — DRAM CLEAN
Ran on-device (2 rounds, 24MB @ 0xa0600000 + all 4 canary pages): **0 errors**.
DRAM corruption is NOT the cause of the kernel deaths. The historic single-bit
flip (0xedfe0dd0 → 0xadfe0dd0) remains unexplained but unreproduced; the
M=1 wall was subsequently root-caused to the head.S ring-map shift bug (see
01_KEXEC_ARCHITECTURE.md §2026-09-01 updates) — DRAM was never the issue.
NOTE (2026-09-02 night): the silent corruption was root-caused to the
secure-domain L2 config — PL310 data latency = 0x111 (1/1/1 cycles) set by
QNX (NS write = SIGBUS; no monitor latency service in the RE'd API), plus
L3/EMIF auto-idle (also secure-only). The DRAM itself is fine (this memtest,
run with the L2 on, was clean). The fix = RE the trustzone-omap4 monitor for
a latency/PPA service (docs/05, HANDOFF_2026-09-02_night_SMC-flush-pv-wall).
NOTE (2026-09-02): the earlier "silent deaths" were ALSO (a) no-op WDT2
kicks (a head.S section PA bug — fixed) expiring the 58.6 s window, and
(b) the L2-off bypass path wedging under sustained traffic (the mode is
retired — see docs/03 §2026-09-02 Evening).

## Purpose (original mission)
Confirm or rule out **DRAM corruption** as the root cause of:
1. Kernel's silent M=1 hang (bc ceiling 122) — since SOLVED (ring-map bug)
2. Payload setup intermittent deaths (file reads/copies, RNDIS timeouts)
3. Observed single-bit flip: DTB magic 0xedfe0dd0 → 0xadfe0dd0 (bit30) in bc page at 0x9FE00000

## Design Principles
- **No kernel, no jump, no CP15/SMC/GIC/CPU1 touches** — pure userspace DRAM test
- **Same placement logic as do_t3** — results map 1:1 onto kernel-run buffer locations
- **NOCACHE view only** — `mmap_device_memory(..., PROT_NOCACHE)` = true DRAM content
- **Reports ALL mismatches** (not abort-on-first) — builds bad-region map for `buf_placement_bad`
- **Canary sweep of bc/ring pages** — the observed flip was in 0x9FE00000, which 24MB buffer doesn't cover
- **WDT2 kicked per pattern** — 24MB × 4 patterns uncached takes seconds; the WDT2 window is 58.6 s, so ample margin

## Build
```bash
cd kexec
source ../qnx-env.sh
./build.sh
# or directly:
arm-unknown-nto-qnx6.6.0eabi-gcc -O1 -marm -o memtest memtest.c
```

Output: `memtest` (ELF 32-bit ARM, dynamically linked, ~12KB)

## Usage
```bash
# On device (pinned to CPU0):
on -C 0 /tmp/memtest [rounds]
```
- `rounds` (default 2): repeat full sweep to catch intermittent flips
- Each round = 24MB buffer sweep + 4 canary pages

## bc Ladder (Memtest)
| bc[1] | Step | Meaning |
|-------|------|---------|
| 60 | ARMED | bc initialized, WDT2 kicked |
| 61 | PLACED | 24MB buffer placed, bc[3]=phys nonce |
| 62 | P0 | Pattern 0xA5A5A5A5 done |
| 63 | P1 | Pattern 0x5A5A5A5A done |
| 64 | P2 | Pattern 0xFFFFFFFF done |
| 65 | P3 | Pattern 0x00000000 done |
| 66 | CANARY | bc/ring pages done |
| 67 | CLEAN | Zero errors — verdict CLEAN |

`bc[2]` = cumulative error count

## Sweep Algorithm

### 24MB Buffer (per round)
```c
patterns = { 0xA5A5A5A5, 0x5A5A5A5A, 0xFFFFFFFF, 0x00000000 }
for each pattern:
    wdt2_kick()
    // Write pass
    for each word in buffer: nv[wi] = pattern
    // Read/verify pass
    for each word in buffer:
        got = nv[wi]
        if (got != pattern):
            errs++
            record: PA = phys + wi*4, want, got, xor, popcount(xor)
            per-64MB region bucket++
    bc_write(step_after[pattern])
    bc[2] = errs
```

### Canary Pages (once per run)
4 pages × 4KB each:
- 0x88000000 (bc mirror + DEBUG_LL ring)
- 0x90000000 (bc primary)
- 0x94000000 (bc mirror)
- 0x9FE00000 (bc mirror + abort trap vectors + DEBUG_LL ring) — **observed flip here**

After each page: `bc_arm(STEP_CANARY)` to restore bc headers.

## Output Format

```
memtest: bc armed
buffer v=0x... phys=a4000000 size=1800000 contig=1800000
round 1/2: 24MB sweep @a4000000
  r1 buf pat A5A5A5A5: 0 errs (total 0)
  r1 buf pat 5A5A5A5A: 0 errs (total 0)
  r1 buf pat FFFFFFFF: 0 errs (total 0)
  r1 buf pat 00000000: 0 errs (total 0)
  r1 buf per-64MB region errs: [none]
  ...
canary page 88000000
  r1 canary pat A5A5A5A5: 0 errs (total 0)
  ...
canary page 9fe00000
  r1 canary pat FFFFFFFF: 1 errs (total 1)
    ERR r1 canary pat=FFFFFFFF pa=9fe00004 got=7FFEFFFF xor=80010001 bits=2
  ...
VERDICT: DRAM ERRORS total=3 — blacklist bad regions in buf_placement_bad or switch units
```

**Error detail per mismatch**:
- `pa` — physical address of the 32-bit word
- `want` — expected pattern
- `got` — actual readback
- `xor` — want ^ got (isolates flipped bits)
- `bits` — popcount(xor) (single-bit vs multi-bit corruption)

**Per-region summary**: 64MB buckets (16 regions across 1GB) showing error concentration.

## Exit Codes
- `0` — CLEAN (zero errors across all rounds + canary)
- `2` — DRAM ERRORS (errors found; see output for locations)

## Interpreting Results

| Result | Action |
|--------|--------|
| CLEAN (0 errors) | DRAM not the culprit; add heartbeat bc-writes in payload setup to pin crash site, resume M=1 investigation |
| Errors in 24MB buffer only | Map bad 64MB regions; add to `buf_placement_bad` protected list; retest |
| Errors in canary pages (esp 0x9FE00000) | **Confirms prime suspect** — the bc/ring region is marginal; switch to second 64GB unit for kernel runs |
| Errors everywhere | Unit severely degraded; swap hardware |
| Intermittent (errors in round 2 but not 1) | Marginal cells; soak test with more rounds (10+) |

## Integration with buf_placement_bad

Add bad regions to the protected list in `qnx2linux.c`:

```c
static const uint64_t prot[] = {
    0x88000000ull, 0x90000000ull, 0x94000000ull, 0x9FE00000ull,
    // Add bad 64MB regions here, e.g.:
    0xA4000000ull,  // if region 0xA4–0xA7 bad
};
```

The guard logic rejects any buffer overlapping `prot[i] ± 4MB`.

## Why Not Abort on First Error?

The original `do_t3` memtest (lines 608–632) aborts on first mismatch. That's wrong for diagnosis — we need the **full error map** to:
1. Identify whether errors are clustered (bad region) or scattered (systemic)
2. Build `buf_placement_bad` blacklist
3. Decide whether to blacklist or swap hardware

## Deployment
```bash
# From host (in kexec/):
scp -i ../rsa memtest root@169.254.0.1:/tmp/
ssh -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memtest 2"
```
No reboot expected (no jump). If payload crashes → procnto dies → WDT2 reboot (bc pages survive).

## Timing Estimate
- 24MB = 6M words
- 4 patterns × (write + read) = 8 passes = 48M uncached accesses
- Uncached DRAM ~ 60–100 ns/access → 3–5 seconds per round
- 2 rounds + canary ≈ 10–15 seconds total
- Well within WDT2 15s window (kicked per pattern)