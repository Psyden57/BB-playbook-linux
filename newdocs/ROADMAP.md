# Roadmap

Ordered. The first item is the next device run.

## 0. Where session 9 left the boot (2026-09-04)

The pv-stale regime is CURED (build #106: the W-32c direct-store block
in start_kernel, marker 163 — pv correct on every run since, tries=0).
The DMA masters are quiesced (--dmaquiet: devb slain, DISPC killed +
register-verified). The placement guard reserves [0xa0000000,
0xa1000000) for the zreladdr inflation region. The front = **the stale
pgd pair [VA 0xdfc/0xdfd]** (QNX-era content, deterministic) — any
allocation in the top 2MB of the linear map wedges on the walk (the svm
deaths W-25/31/32a/34/36/37 at bc[1]=161).

## 1. Run W-39 (kernel #110, BUILT AND UNRUN — next session's first run)

The 2MB allocator shave (arm_lowmem_limit -= 2MB → 0xbfc00000) + the
svm memset re-armed (markers 167/0x567/151, bc[19] = the readback) + the
pmd pattern dump extended with bc[25] = VA 0xdfa (the previously-
untested middle pair). Interpretation table in
SESSION-HANDOFF/BOOTSTRAP_SESSION_10.md. If the middle pair is healthy →
the memset completes → the boot advances to the 171 region.

## 2. The ladder beyond the remap: 152/153 → 147 → 126 → 125 → 129 → 128
   → the 171 region

The 171 wall (taskstats_init_early → kmem_cache_create) gets its first
zImage-path test with the pv fix in place. Read
newdocs/contradictions/171-wall-analyses.md (the era matrix) before
analyzing.

## 3. Deal with l2x0_of_init before the boot reaches init_IRQ

Patch `l2c_enable` to skip the by-way op (0x7FC) on winchester, or set
`CONFIG_CACHE_L2X0=n` (keep QNX's L2 setup). See KNOWN_ISSUES #3.

## 4. RE SMC 0x112 (the undocumented loader service) + test NS 0x772

0x112: full disassembly of `bootblob_arm9000.bin` (local-only), find
0x112's callers and arguments — if it is an L2 latency service, the
1/1/1 config becomes fixable pre-jump. 0x772 (PL310 invalidate-by-PA,
no clean) from NS via smctest: if it works, the stale-L2-line class
(pv + the pgd pair) gets its REAL cure (invalidate without poisoning
DRAM).

## 5. Rootfs (T4)

## 6. Open questions carried from session 9

- The stale-region width (is it only the last pgd pair? W-39's bc[25]
  answers).
- The W-26 stack-protector catch's mechanism (the boot-stack lines as
  stale-era content — unverified).
- bc[2]=0x3E7's writer (probe+zImage-correlated, head.S-era deaths).
- The MMU-off delivery paradox mechanism (INLINE-don't-call stands).
- The head.S/phys2virt.S comments still narrate the wrong
  dirty-discard theory — a cleanup pass when convenient.
