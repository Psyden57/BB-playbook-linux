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
        @ (NOTE: no literals in the cont — the memcpy copies [cont_start,
        @ cont_end) only, so an =literal pool after cont_end is never copied
        @ and the ldr would read leftover IRAM junk. Bit us once.)
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
