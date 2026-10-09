@ stub3.S — kexec jump pieces.
@
@ tramp_pos_start: a POSITION-INDEPENDENT naked trampoline that runs from the
@   payload's own .text (always executable — QNX never marks its own text XN,
@   unlike anon/device maps whose XN varies with placement). Entered via a
@   plain blx with registers prepared by enter_stub():
@     r8 = TTBR0 (flat table)   r9 = target entry (physical)
@     r11 = breadcrumb block PHYSICAL (post-switch stores)
@     r12 = continuation physical (IRAM 0x40308040)
@     r7  = DTB physical (0 = none)
@   After the TTBR0 switch the next fetches walk the NEW flat table, so the
@   payload text's 1 MB section is identity-mapped in it by the payload.
@
@ cont_start: the continuation, copied to IRAM 0x40308040 (VA == PA,
@   identity-mapped) — fetched entirely through the flat table, no QNX vaddr
@   involved. Turns the MMU off and jumps to the target with the ARM Linux
@   boot convention.
        .syntax unified
        .arch armv7-a
        .arch_extension sec
        .arm
        .text
        .globl tramp_pos_start
        .globl tramp_pos_end
        .globl cont_start
        .globl cont_end

tramp_pos_start:
        mov     r0, #0
        mcr     p15, 0, r0, c2, c0, 2       @ TTBCR = 0 (all VAs -> TTBR0)
        mvn     r0, #0
        mcr     p15, 0, r0, c3, c0, 0       @ DACR = all manager
        mcr     p15, 0, r8, c2, c0, 0       @ TTBR0 = flat table
        mov     r0, #63
        str     r0, [r11, #4]               @ bc = 63 (PHYS, identity-mapped)
        mov     r0, #64
        str     r0, [r11, #4]               @ bc = 64
        mcr     p15, 0, r12, c8, c7, 1      @ TLBIMVA (continuation VA==PA)
        isb
        bx      r12                         @ -> continuation (VA == PA)
tramp_pos_end:

cont_start:                                 @ PC == PA from here on
        mov     r0, #21
        str     r0, [r11, #4]               @ bc = 21 (identity reached)
        str     r7, [r11, #0x30]            @ 0x90000030 = r7 (DTB chain debug)
        mov     r0, #211
        str     r0, [r11, #4]               @ bc = 211: r7 recorded
        cps     #0x13                       @ SVC: the kernel runs in SVC mode and
        @ R13 is BANKED — setting sp in System mode (as we were) wrote the USER
        @ bank; the kernel then pushed onto QNX's leftover SVC stack -> abort.
        mov     sp, #0x89000000             @ safe scratch stack (DRAM), SVC bank
        mov     r0, #212
        str     r0, [r11, #4]               @ bc = 212: sp set
        dsb                                 @ head.S error paths + DEBUG_LL
        @ (NOTE: literals are only SAFE INSIDE [cont_start, cont_end) — the
        @ memcpy copies that range only, so a pool after cont_end is never
        @ copied and the ldr would read leftover IRAM junk. Bit us once.
        @ The W-104 insert below follows this: its pool rides the .ltorg.)
        dsb
        mov     r0, #213
        str     r0, [r11, #4]               @ bc = 213: pre-TLBIALL
        @ TLBIALL REMOVED: with CPU1 held in reset the broadcast TLBIALL
        @ wedged the cont (bc=213 death, reproducible) — and it is redundant:
        @ the MMU goes off right here and the kernel flushes its own TLB.
        mrc     p15, 0, r0, c1, c0, 0
        bic     r0, r0, #1                  @ M
        bic     r0, r0, #4                  @ C
        bic     r0, r0, #0x1000             @ I
        mcr     p15, 0, r0, c1, c0, 0
        dsb
        isb
        mov     r0, #23
        str     r0, [r11, #4]               @ bc = 23 (MMU off)
        @ NOTE: do NOT SMC here (MMU off). Empirical 2026-09-01: monitor
        @ SMC 0x102 with MMU off NEVER RETURNS (bc=24 never landed; the
        @ identical call with MMU on returns fine, 2 runs). The L2 disable
        @ lives in enter_stub (qnx2linux.c) — MMU on, last cached-context
        @ act before blx into the trampoline.
        @ --- W-104 (session 20): the pre-kernel RESTORE (restore-before-reset).
        @ If params[4] (0x40309810) == 0x4A326B00 (armed by the payload's
        @ --sarrep block), restore CPU1_WAKEUP_NS_PA_ADDR = 0x4A326B00 here —
        @ the restore-before-reset safety net (W-103 r1: a repointed wake path
        @ at reset time stalls the reset cycle). Any other value (0, residue)
        @ skips: normal runs never touch the SAR. MMU is OFF here — plain
        @ physical stores, same class as the bc=23 store above. No SMC.
        ldr     r2, =0x40309800
        ldr     r0, [r2, #0x10]             @ params[4]: the armed restore value
        ldr     r1, =0x4A326B00             @ the canonical armed value
        cmp     r0, r1
        bne     .Lw104_no_restore
        ldr     r1, =0x4A326A08             @ CPU1_WAKEUP_NS_PA_ADDR
        str     r0, [r1]
        dsb
.Lw104_no_restore:
        @ --- W-107 (session 23): THE PARKED-ALIVE RELEASE (jump-integration).
        @ Armed iff params[5] (0x40309814) == 0x5A52F7A7 (the --parkjump C
        @ block stages blob v2 at 0x40309A00 + this arm). MMU is OFF: plain
        @ physical stores, NO SMC (the file's rule). r9/r11 preserved.
        ldr     r2, =0x40309800
        ldr     r0, [r2, #0x14]             @ params[5]: the arm gate
        ldr     r1, =0x5A52F7A7
        cmp     r0, r1
        bne     .Lw107_skip
        ldr     r3, =0x4A326A08             @ CPU1_WAKEUP_NS_PA_ADDR
        ldr     r0, =0x40309A00             @ the parked blob v2
        str     r0, [r3]                    @ REPOINT [A08] -> the blob
        dsb
        ldr     r4, =0x4824380C             @ RSTCTRL_CPU1
        mov     r0, #0
        str     r0, [r4]                    @ RELEASE (1 -> 0)
        dsb
        sev                                 @ the documented wake event
        mov     r2, #0x10000                @ bounded landing poll (sub-ms)
.Lw107_poll:
        ldr     r0, [r11, #0x7C]            @ bc[31] = the blob's marker slot
        ldr     r1, =0x5A52F2F2
        cmp     r0, r1
        beq     .Lw107_landed
        subs    r2, r2, #1
        bne     .Lw107_poll
        @ POLL EXPIRED (no landing): degrade safe — RE-HOLD + restore.
        mov     r0, #1
        str     r0, [r4]                    @ RE-HOLD (today's held shape)
        dsb
        ldr     r0, =0x4A326B00             @ canonical
        str     r0, [r3]                    @ RESTORE [A08]
        dsb
        mov     r0, #215
        str     r0, [r11, #4]               @ bc = 215 (no-landing; re-held)
        b       .Lw107_skip
.Lw107_landed:
        ldr     r0, =0x4A326B00             @ canonical
        str     r0, [r3]                    @ RESTORE [A08] (load-bearing:
        dsb                                 @ any later reset must see this)
        mov     r0, #214
        str     r0, [r11, #4]               @ bc = 214 (PARKED-ALIVE)
.Lw107_skip:
        mov     r0, #0                      @ ARM Linux boot convention
        mvn     r1, #0                      @ r1 = 0xffffffff
        @ r2 = DTB phys from the params block (0x40309800+4) — the register
        @ chain (enter_stub r7 -> cont) proved unreliable (GCC reused r7 as a
        @ local: the probe saw r2=4); the params block is payload-written and
        @ probe-verified. For T2 (no probe) params[1] = 0 = no DTB.
        ldr     r2, =0x40309800
        ldr     r2, [r2, #4]
        bx      r9                          @ jump to target (blob/probe)
        .ltorg                              @ pool MUST be inside [cont_start, cont_end)

cont_end:
