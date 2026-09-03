/* stub.S — kexec jump stub, placed in IRAM @ 0x40304000, executed by CPU1
 * released from the OMAP4 ROM pen (SVC mode, MMU off, caches off).
 *
 * Param block at 0x40304f00 (written by payload before release):
 *   +0  entry   physical address of target blob
 *   +4  bc      physical address of breadcrumb block
 *   +8  flags   bit0 = assert CPU0 reset before jump
 *               bit1 = disable GIC distributor before jump
 *
 * Breadcrumb block: [+0]=magic [+4]=step [+8]=count [+12]=maxcount
 *                   [+16]=cpu0-alive counter [+20]=magic2
 */
        .syntax unified
        .arm
        .text
        .globl _start

_start:
        ldr     r8, =0x40304f00     @ param block
        ldr     r9, [r8]            @ entry
        ldr     r10, [r8, #4]       @ bc
        ldr     r11, [r8, #8]       @ flags

        mov     r0, #1
        str     r0, [r10, #4]       @ bc.step = 1 (stub running on CPU1)

        tst     r11, #1             @ flag: reset CPU0?
        beq     1f
        ldr     r1, =0x4824340c     @ RM_PDA_CPU0_RSTCTRL
        mov     r0, #1
        str     r0, [r1]            @ hold CPU0 in warm reset
1:
        tst     r11, #2             @ flag: disable GICD?
        beq     2f
        ldr     r1, =0x48241000     @ GICD_CTLR
        mov     r0, #0
        str     r0, [r1]
2:
        mov     r0, #2
        str     r0, [r10, #4]       @ bc.step = 2 (jumping)

        bx      r9                  @ jump to target (SVC, MMU off)
