@ hello.S — T2 target blob: runs on CPU1 (SVC, MMU off) after the stub jump.
@ Writes breadcrumbs, streams "KEXEC-HELLO" on UART3 (may not be externally
@ visible — breadcrumbs are the real readout), then after maxcount messages
@ parks itself in its own reset pen (held until CPU0 releases it).
@
@ Breadcrumb block: [+0]=magic [+4]=step [+8]=count [+12]=maxcount

        .syntax unified
        .arm
        .text
        .globl hello_start

hello_start:
        ldr     r4, =0x9fe00000     @ breadcrumb block (survives reboot)
        mov     r0, #3
        str     r0, [r4, #4]        @ bc.step = 3 (target running)

        ldr     r5, =0x48020000     @ UART3 (OMAP4, 115200 8N1, QNX-owned)
        adr     r6, msg
        mov     r7, #0              @ msg index

charloop:
        ldr     r0, [r5, #0x14]     @ UART_LSR
        tst     r0, #0x20           @ THR empty?
        beq     charloop
        ldrb    r0, [r6, r7]
        cmp     r0, #0
        bne     sendit

        @ end of message: bump counter, check limit
        mov     r7, #0
        ldr     r0, [r4, #8]
        add     r0, r0, #1
        str     r0, [r4, #8]        @ bc.count++
        ldr     r1, [r4, #12]       @ maxcount
        cmp     r0, r1
        blt     charloop

        @ done: hold BOTH cores -> nobody services WDT2 -> warm reset in <=15s
        @ (warm reset preserves DRAM = breadcrumbs survive; avoids the TWL6030
        @ PMIC watchdog's 127s POWER-OFF)
        mov     r0, #4
        str     r0, [r4, #4]        @ bc.step = 4 (done, forcing reset)
        ldr     r1, =0x4824380c     @ RM_PDA_CPU1_RSTCTRL (self, CPU1)
        mov     r0, #1
        str     r0, [r1]
        ldr     r1, =0x4824340c     @ RM_PDA_CPU0_RSTCTRL
        mov     r0, #1
        str     r0, [r1]            @ CPU0 holds itself too
spin:   b       spin                @ WDT2 fires ~15s -> warm reset

sendit:
        strb    r0, [r5]            @ UART_THR
        add     r7, r7, #1
        b       charloop

msg:    .ascii  "KEXEC-HELLO\r\n"
        .byte   0
        .align  2
