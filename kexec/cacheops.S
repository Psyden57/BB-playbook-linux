@ cacheops.S — privileged CP15 helpers (thread must be in System mode via
@ ThreadCtl(_NTO_TCTL_IO_PRIV); these instructions are illegal in user mode).
        .syntax unified
        .arch armv7-a
        .arch_extension sec
        .arm
        .text
        .globl clean_inval_l1_all

@ void clean_inval_l1_all(void)
@ Clean+invalidate the entire L1 data cache by set/way, invalidate I-cache+BTB.
@ Corrupts r0-r7.
clean_inval_l1_all:
        push    {r4, r5, r6, lr}
        mov     r0, #0
        mcr     p15, 2, r0, c0, c0, 0       @ CSSELR = 0 (L1 data)
        dsb
        mrc     p15, 1, r0, c0, c0, 0       @ CCSIDR
        ldr     r6, =0x3ff
        lsr     r3, r0, #3
        and     r3, r3, r6                  @ associativity - 1
        add     r3, r3, #1                  @ associativity
        lsr     r5, r0, #13
        ldr     r6, =0x7fff
        and     r5, r5, r6                  @ numsets - 1
        add     r5, r5, #1                  @ numsets
        sub     r6, r3, #1
        clz     r4, r6                      @ way shift  = clz(assoc-1) = 32-log2(assoc)
        sub     r7, r5, #1
        clz     r7, r7
        rsb     r7, r7, #32                 @ log2(numsets)
        sub     r2, r4, r7                  @ set shift  = way shift - log2(numsets)
        mov     r7, #0                      @ way counter
wayloop:
        mov     r6, #0                      @ set counter
setloop:
        orr     r0, r7, r7, lsl r4          @ way field
        orr     r0, r0, r6, lsl r2          @ set field
        mcr     p15, 0, r0, c7, c14, 2      @ DCCISW: clean+invalidate by set/way
        add     r6, r6, #1
        cmp     r6, r5
        blt     setloop
        add     r7, r7, #1
        cmp     r7, r3
        blt     wayloop
        dsb
        mov     r0, #0
        mcr     p15, 0, r0, c7, c5, 0       @ I-cache invalidate all (ICIALLU)
        mcr     p15, 0, r0, c7, c5, 6       @ branch predictor invalidate all
        dsb
        pop     {r4, r5, r6, lr}
        bx      lr

@ uint32_t mon_call_full(r0, r1, r2, r3, r6, r12) — SMC #0 with full register
@ shape (mirrors the SAR blob / devpm conventions). Returns r0.
        .globl mon_call_full
mon_call_full:
        push    {r4, r5, r7, lr}
        mov     r4, r0
        mov     r5, r1
        mov     r7, r2
        mov     lr, r3              @ r3 arg
        mov     r0, r4              @ r0
        mov     r1, r5              @ r1
        mov     r2, r7              @ r2
        mov     r3, lr              @ r3
        mov     r6, #0xFF           @ monitor task tag (as used by blob/devpm)
        mov     ip, #0
        dsb
        mcr     p15, 0, r6, c7, c5, 6
        smc     #0
        dmb
        pop     {r4, r5, r7, lr}
        bx      lr

@ uint32_t ppa_call(r0=idx, r1=process, r2=flag, r3=pargs_phys) — the PPA
@ service shape (devpm-omap4 be04-era + sleep44xx.S: r12=0 secure task call,
@ r6=0xFF new-task tag, BPISI, SMC #0). Returns r0. Unlike mon_call_full this
@ really zeroes r12 (mainline mon_call_full leaves r12 = caller garbage).
@ PPA indices: 0x21 SVC_0, 0x23 L2_POR, 0x25 CPU_ACTRL_SMP, 0x26/0x27
@ (devpm's suspend pair, semantics unknown — the --ppa probe's targets).
        .globl ppa_call
ppa_call:
        push    {r4, r5, r7, lr}
        mov     r4, r0                  @ idx
        mov     r5, r1                  @ process
        mov     r7, r2                  @ flag
        @ r3 = pargs (phys) stays live
        mov     r0, r4
        mov     r1, r5
        mov     r2, r7
        mov     r6, #0xFF
        mov     ip, #0                  @ ip == r12: the smc3 shape needs r12=0
        dsb
        mcr     p15, 0, r6, c7, c5, 6   @ BPIALL (devpm does this pre-SMC)
        smc     #0
        dmb
        pop     {r4, r5, r7, lr}
        bx      lr

@ uint32_t read_ccsidr_decode(uint32_t *assoc_out, uint32_t *sets_out, uint32_t *line_out)
@ r0=assoc_out, r1=sets_out, r2=line_out. Returns raw CCSIDR.
        .globl read_ccsidr_decode
read_ccsidr_decode:
        push    {r4, r5, r6, lr}
        mov     r4, r0
        mov     r5, r1
        mov     r6, r2
        mov     r0, #0
        mcr     p15, 2, r0, c0, c0, 0       @ CSSELR = 0 (L1 data)
        dsb
        mrc     p15, 1, r0, c0, c0, 0       @ CCSIDR
        ldr     ip, =0x3ff
        lsr     r3, r0, #3
        and     r3, r3, ip
        add     r3, r3, #1
        str     r3, [r4]                    @ assoc
        ldr     ip, =0x7fff
        lsr     r3, r0, #13
        and     r3, r3, ip
        add     r3, r3, #1
        str     r3, [r5]                    @ numsets
        and     r3, r0, #7
        add     r3, r3, #2
        mov     r2, #1
        lsl     r2, r2, r3
        str     r2, [r6]                    @ line bytes
        pop     {r4, r5, r6, lr}
        bx      lr
