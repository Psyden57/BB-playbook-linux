/* scuprobe.c — session 11 (W-72): can the NS side disable the MPCore SCU?
 *
 * The W-72 A/B proved the TLBIALL mcr (c8,c7,0) itself wedges the machine
 * post-jump (bc[1] passed the whole CMA block with the TLBIALL skipped,
 * and early_fixmap_shutdown's clear_fixmap then wedged at 126->125).
 * Model: QNX leaves the SCU (0x48240000) enabled; CPU1 sits in warm reset
 * (its SCU slave port dead); the A9's TLB maintenance ops are routed
 * through the SCU regardless of ACTLR.SMP/FW (proven: all three ACTLR
 * states wedged) -> every broadcast waits for the held CPU1 forever.
 * stub3.S has known this since session 1: "no TLBIALL here (wedged with
 * CPU1 in reset; redundant)".
 *
 * THE TEST: read the SCU CTRL (expect enabled), try the NS write to clear
 * the enable bit, re-read, restore. Outcomes:
 *   - write survives + re-read shows disabled -> the payload can disable
 *     the SCU pre-jump; the kernel's TLB ops become pure-local.
 *   - write = SIGBUS (the secure-filter class) -> the SCU write is closed
 *     from NS; look for a monitor service instead.
 *   - the box freezes on the write -> ALSO diagnostic (the SCU-write wedge
 *     class); WDT2 recovers, evidence intact.
 *
 * The SCU is RE-ENABLED at the end to keep live QNX healthy (its own
 * coherence depends on it while it runs).
 *
 * build: arm-unknown-nto-qnx6.6.0eabi-gcc -O1 -o scuprobe scuprobe.c
 */
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

#define SCU_PA 0x48240000ULL

int main(void)
{
    volatile uint32_t *scu;
    uint32_t ctrl, cfg, pwrs, before;

    setvbuf(stdout, NULL, _IONBF, 0);
    ThreadCtl(_NTO_TCTL_IO_PRIV, 0);
    scu = mmap_device_memory(0, 0x1000, PROT_READ | PROT_WRITE | PROT_NOCACHE,
                             MAP_SHARED | MAP_PHYS, SCU_PA);
    if (scu == MAP_FAILED) { perror("scu"); return 1; }

    ctrl = scu[0x00 / 4];           /* SCU_CTRL: bit0 = enable */
    cfg  = scu[0x04 / 4];           /* SCU_CFG */
    pwrs = scu[0x08 / 4];           /* SCU_CPU_PWR_STATUS */
    printf("SCU_CTRL   = %08x (enable=%u)\n", ctrl, ctrl & 1);
    printf("SCU_CFG    = %08x\n", cfg);
    printf("SCU_PWRSTS = %08x (cpu0=%u cpu1=%u)\n",
           pwrs, pwrs & 1, (pwrs >> 1) & 1);

    before = ctrl;
    printf("step: writing CTRL=%08x (clear enable bit)\n", before & ~1u);
    scu[0x00 / 4] = before & ~1u;

    ctrl = scu[0x00 / 4];
    printf("post-write SCU_CTRL = %08x -> %s\n", ctrl,
           (ctrl & 1) ? "STILL ENABLED (write filtered/no-op?)" :
                        "DISABLED — NS SCU WRITE WORKS");
    if (!(ctrl & 1)) {
        printf("step: TLB-op smoke test while the SCU is off (local TLBIALL)\n");
        __asm__ volatile("mcr p15, 0, %0, c8, c7, 0" :: "r"(0) : "memory");
        __asm__ volatile("dsb sy" ::: "memory");
        __asm__ volatile("isb" ::: "memory");
        printf("survived the local TLBIALL + dsb + isb under SCU=off\n");
        printf("step: re-enabling the SCU (restore %08x)\n", before);
        scu[0x00 / 4] = before;
        ctrl = scu[0x00 / 4];
        printf("restored SCU_CTRL = %08x\n", ctrl);
    }
    printf("SCUPROBE DONE (box alive)\n");
    return 0;
}
