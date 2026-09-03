@ stub2.S — kexec continuation stub, linked at IRAM 0x40308000, entered at
@ that EXACT physical address (the 0x403 section is identity-mapped in the
@ flat table). Reached via a 4-word TTBR0-switch trampoline in the jump buffer.
@ Registers on entry: r9 = target entry (physical), r11 = breadcrumbs phys.
        .syntax unified
        .arm
        .text
        .globl _start

_start:
        mov     r0, #21
        str     r0, [r11, #4]               @ bc.step = 21 (identity reached)
        dsb
        mov     r0, #0
        mcr     p15, 0, r0, c8, c7, 0       @ TLBIALL (drop stale QNX entries)
        dsb
        isb
        mrc     p15, 0, r0, c1, c0, 0
        bic     r0, r0, #(1 << 0)           @ M
        bic     r0, r0, #(1 << 2)           @ C
        bic     r0, r0, #(1 << 12)          @ I
        mcr     p15, 0, r0, c1, c0, 0
        dsb
        isb
        mov     r0, #23
        str     r0, [r11, #4]               @ bc.step = 23 (MMU off)
        dsb
        mov     r0, #0                      @ ARM Linux boot convention
        mvn     r1, #0                      @ r1 = 0xffffffff
        mov     r2, #0                      @ r2 = DTB (none for T2)
        bx      r9                          @ jump to target
