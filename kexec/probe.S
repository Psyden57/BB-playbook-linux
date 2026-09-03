@ probe.S — kernel-entry probe: mimics the kernel's earliest boot steps and
@ breadcrumbs + LED-toggle each one, then chains into the real kernel.
@
@ Entered at probe_start: MMU off, r0=0, r1=0xffffffff (machine), r2=DTB phys,
@ r9 = probe entry (from the stub). Params block (written by the payload):
@   0x40309800: [0] = kernel entry phys, [1] = DTB phys
@
@ bc words at 0x90000000: [1]=step [2]=dtb size [6]=dtb magic [7]=PL310 id
@ Steps: 50 entered · 501 EN survived · 51 DTB magic ok · 52 DTB size read ·
@        53 MMU on · 54 MMU off · 55 SVC mode · 56 PL310 readable ·
@        57 jumping to kernel
@
@ LED: the payload sets the FAN5702 to BLUE via QNX's /dev/i2c3 before the
@ jump. I2C4 is clock-gated at idle (QNX driver gates per-transaction via
@ the trustzone monitor; direct access = data abort; PRCM writes are NS-
@ firewalled), so the probe can't change colors. It toggles the EN pin
@ (GPIO1_13) instead: low = chip reset (LED off), high = registers default
@ (LED lights, not blue). The user counts blue->off->default->off->default->
@ off as the stages pass.  bc sub-steps 501/502* remain the ground truth.
@   (502 no longer used)

        .syntax unified
        .arm
        .text
        .globl probe_start

probe_start:
        ldr     r4, =0x90000000         @ bc block (identity-mapped, phys)
        mov     r0, #50
        str     r0, [r4, #4]            @ 50: entered
        bl      led_off
        ldr     r0, =501
        str     r0, [r4, #4]            @ 501: EN write survived (LED: blue->off)
        str     r2, [r4, #0x10]         @ bc[4] = r2 (DTB ptr as seen by probe)
        ldr     r0, =502
        str     r0, [r4, #4]            @ 502: about to read DTB magic
        @ r2 from the register chain has arrived as garbage twice (r2=4) —
        @ bypass it: the params block holds the authoritative DTB address
        ldr     r2, =0x40309800
        ldr     r2, [r2, #4]            @ r2 = params[1] = DTB phys
        ldr     r5, [r2]
        ldr     r0, =503
        str     r0, [r4, #4]            @ 503: DTB read survived
        str     r5, [r4, #0x18]         @ bc[6] = raw magic read (even if bad)
        ldr     r6, =0xedfe0dd0
        cmp     r5, r6
        bne     probe_dead
        ldr     r0, =504
        str     r0, [r4, #4]            @ 504: magic ok

        @ --- DTB sanity (r2) ---
        mov     r0, #51
        str     r0, [r4, #4]
        ldr     r0, [r2, #4]            @ totalsize
        str     r0, [r4, #8]            @ bc[2] = dtb totalsize
        mov     r0, #52
        str     r0, [r4, #4]
        bl      led_on                  @ LED: off -> default (DTB ok)

        @ --- MMU on with the payload's flat table (IRAM 0x40304000) ---
        mov     r0, #0
        mcr     p15, 0, r0, c2, c0, 2   @ TTBCR = 0
        mvn     r0, #0
        mcr     p15, 0, r0, c3, c0, 0   @ DACR = manager
        ldr     r0, =0x40304000
        mcr     p15, 0, r0, c2, c0, 0   @ TTBR0 = flat table
        mcr     p15, 0, r0, c8, c7, 0   @ TLBIALL
        dsb
        isb
        mrc     p15, 0, r0, c1, c0, 0
        orr     r0, r0, #1              @ M
        mcr     p15, 0, r0, c1, c0, 0
        dsb
        isb
        mov     r0, #53
        str     r0, [r4, #4]            @ 53: MMU on, still alive
        bl      led_off                 @ LED: default -> off (MMU ok)

        @ --- MMU off again ---
        mrc     p15, 0, r0, c1, c0, 0
        bic     r0, r0, #1
        mcr     p15, 0, r0, c1, c0, 0
        dsb
        isb
        mov     r0, #54
        str     r0, [r4, #4]

        @ --- SVC mode (kernel enters safe_svcmode_maskall) ---
        cps     #0x13
        mov     r0, #55
        str     r0, [r4, #4]

        @ --- PL310 accessibility (cache-id reg) ---
        ldr     r1, =0x48242000
        ldr     r5, [r1]                @ cache-id — data abort here = PL310 gated
        str     r5, [r4, #0x1c]         @ bc[7] = PL310 cache-id
        @ L2-disable verification (the cont ran monitor SMC 0x102 CTRL:=0):
        @ bc[8] = control (expect 0), bc[9] = data latency (stale 0x111).
        @ These are the only reads — never write PL310 config from NS.
        ldr     r2, [r1, #0x100]        @ control
        ldr     r0, =0x90000020
        str     r2, [r0]                @ bc[8]
        ldr     r2, [r1, #0x10c]        @ data latency
        ldr     r0, =0x90000024
        str     r2, [r0]                @ bc[9]
        mov     r0, #56
        str     r0, [r4, #4]
        bl      led_on                  @ LED: off -> default (SVC+PL310 ok)

        @ --- UART3 / ring / push tests (MMU off, SVC) ---
        ldr     r1, =0x48020000
        mov     r0, #'X'
        strb    r0, [r1]                @ UART3 write — abort here = UART3 dead
        ldr     r0, =57
        str     r0, [r4, #4]            @ 57: UART3 write survived
        ldr     r1, =0x94000080
        ldr     r2, [r1]                @ ring3 count
        str     r2, [r4, #0x34]         @ 0x90000034 = ring3 count as seen
        add     r2, r2, #1
        str     r2, [r1]                @ ring3 count++
        mov     r0, #'Z'
        strb    r0, [r1, r2]            @ ring3 char at slot (count)
        ldr     r0, =58
        str     r0, [r4, #4]            @ 58: ring3 read/write survived
        push    {r0, r1}
        pop     {r0, r1}
        ldr     r0, =59
        str     r0, [r4, #4]            @ 59: push/pop survived

        @ --- long-loop diagnostic: sustained cacheable WB traffic -------
        @ Separates machine-vs-kernel: with the L2 ON (QNX config) the
        @ kernel's early C is the first sustained cacheable WB traffic —
        @ if the MACHINE wedges under the same traffic bare-metal, the
        @ L2/interconnect-state theory is confirmed without any monitor
        @ call. Writes go through the payload-built CACHEABLE alias
        @ tt[0x700] = VA 0x70000000 -> PA 0xB0000000 (high DRAM, away
        @ from kernel/DTB/bc/rings). Markers: bc[1]=80 start, bc[2]=pass
        @ counter (CIPA-flushed per pass), bc[1]=81 survived.
        mov     r0, #80
        str     r0, [r4, #4]            @ bc = 80: long-loop start
        mrc     p15, 0, r0, c1, c0, 0
        orr     r0, r0, #1              @ M (flat table still in TTBR0)
        orr     r0, r0, #4              @ C: data accesses cacheable
        mcr     p15, 0, r0, c1, c0, 0
        dsb
        isb
        ldr     r6, =0x70000000         @ cacheable alias base
        mov     r7, #0                  @ pass counter
        ldr     r8, =0x12345678         @ data pattern
1000:   mov     r2, r6                  @ cursor
        mov     r1, #0x100000           @ 1 MB per pass
1010:   str     r8, [r2], #4
        subs    r1, r1, #4
        bne     1010b
        @ per-pass durability: bc[2] = pass count, then flush the bc line
        @ by PA (PL310 by-PA 0x768 + sync — foreground-safe from NS)
        str     r7, [r4, #8]            @ bc[2] = pass counter
        dsb
        mov     r0, #0x90000000         @ bc line PA
        ldr     r3, =0x48242768
        str     r0, [r3]
        ldr     r3, =0x48242730
        mov     r0, #0
        str     r0, [r3]
1100:   ldr     r0, [r3]
        tst     r0, #1
        bne     1100b
        add     r7, r7, #1
        cmp     r7, #1000               @ 1000 passes = 1 GB (short: the
        blo     1000b                   @ L2-on machine is proven flaky —
                                        @ just a sanity marker now)
        mrc     p15, 0, r0, c1, c0, 0
        bic     r0, r0, #1              @ MMU off again for the kernel
        bic     r0, r0, #4
        mcr     p15, 0, r0, c1, c0, 0
        dsb
        isb
        mov     r0, #81
        str     r0, [r4, #4]            @ bc = 81: long-loop SURVIVED

        @ --- chain into the real kernel ---
        ldr     r9, =0x40309800
        ldr     r9, [r9]                @ kernel entry phys
        ldr     r5, [r9]                @ first kernel instruction word
        str     r5, [r4, #0x14]         @ bc[5] = kernel[0] (expect bl stub)
        mov     r0, #60
        str     r0, [r4, #4]
        bl      led_off                 @ LED: default -> off (entering kernel)
        mov     r0, #0                  @ ARM Linux boot convention
        mvn     r1, #0                  @ r1 = 0xffffffff
        ldr     r2, =0x40309800
        ldr     r2, [r2, #4]            @ r2 = DTB phys (params[1])
        bx      r9                      @ -> kernel

probe_dead:
        mov     r0, #0
        str     r0, [r4, #0x18]         @ bc[6] = 0 (bad dtb magic)
        bl      led_on                  @ LED: default (dead early, parking)
        ldr     r1, =0x4824380c         @ hold both cores -> WDT2 reset
        ldr     r2, =0x4824340c
        mov     r0, #1
        str     r0, [r1]
        str     r0, [r2]
1:      b       1b

@ ---------------------------------------------------------------------------
@ FAN5702 EN pin = GPIO1_13 (GPIO1 @ 0x4A310000, WKUP domain, NS-writable —
@ proven live). EN low = chip hard reset (LED off); EN high = registers at
@ default (LED lights). Colors need I2C4, which is clock-gated at idle:
@ QNX's i2c driver gates per-transaction through the trustzone monitor and
@ PRCM CLKCTRL writes abort from NS — see LED-RE.md + SESSION-HANDOFF 5C.
@ ---------------------------------------------------------------------------

@ led_off — EN (GPIO1_13) low. Clobbers r0-r1.
led_off:
        movw    r0, #0x0190
        movt    r0, #0x4a31             @ GPIO1 CLEARDATAOUT
        mov     r1, #0x2000
        str     r1, [r0]
        bx      lr

@ led_on — EN (GPIO1_13) high. Clobbers r0-r1.
led_on:
        movw    r0, #0x0194
        movt    r0, #0x4a31             @ GPIO1 SETDATAOUT
        mov     r1, #0x2000
        str     r1, [r0]
        bx      lr
