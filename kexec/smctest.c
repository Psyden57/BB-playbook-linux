/* smctest.c — QNX-side secure-monitor (SMC) experiments, PlayBook kexec work.
 * Run as root. Calls use the proven C-flow mon_call shape (service id in
 * r12, System mode via ThreadCtl(_NTO_TCTL_IO_PRIV)) — the same shape the
 * payload uses for 0x100/0x101/0x102.
 *
 * 2026-09-11 (session 10): the TRM §27.5 Table 27-61 documents
 *   R12 = 0x112 = write PL310 Tag AND Data RAM latency registers
 *   (r0 = tag latency, r1 = data latency)
 * contradicting session-7's "no tag/data-latency service" RE claim. This
 * ladder tests it WITHOUT functional change first:
 *   1. baseline 0x103 (known-good read service) — is the monitor answering
 *      from System mode at all?
 *   2. 0x112 accept-test with the CURRENT values (tag=0x728-read,
 *      data=0x72C-read) — rc tells accept/reject/wedge without changing
 *      any L2 state.
 *   3. 0x112 data-latency change to the SLOWER 0x222 (all fields 2
 *      cycles vs the current 1) + readback verification.
 *   4. restore to the observed values + readback.
 * NS PL310 READS are safe (proven by --l2lat/--ppa); this tool never
 * writes PL310 registers directly.
 * build: arm-unknown-nto-qnx6.6.0eabi-gcc -o smctest smctest.c
 */
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

__asm__(".arch armv7-a\n.arch_extension sec\n");

/* payload's mon_call shape: dsb; smc #0; dmb — service in r12 */
static inline uint32_t mon_call(uint32_t api, uint32_t a0, uint32_t a1)
{
    register uint32_t r0 __asm__("r0") = a0;
    register uint32_t r1 __asm__("r1") = a1;
    register uint32_t r12v __asm__("r12") = api;
    __asm__ volatile("dsb\n\tsmc #0\n\tdmb"
                     : "+r"(r0)
                     : "r"(r1), "r"(r12v)
                     : "memory");
    return r0;
}

static volatile uint32_t *pl310;

static void l2dump(const char *when)
{
    printf("PL310 %s: ctrl=%08x aux=%08x tag=%08x data=%08x prefetch=%08x\n",
           when, pl310[0x100 / 4], pl310[0x104 / 4], pl310[0x108 / 4],
           pl310[0x10C / 4], pl310[0x110 / 4]);
    fflush(stdout);
}

int main(void)
{
    uint32_t r, tag0, data0;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (ThreadCtl(_NTO_TCTL_IO_PRIV, 0) == -1)
        perror("IO_PRIV");

    pl310 = mmap_device_memory(0, 0x1000, PROT_READ | PROT_NOCACHE,
                               MAP_SHARED | MAP_PHYS, 0x48242000ull);
    if (pl310 == MAP_FAILED) {
        perror("mmap_device_memory PL310");
        return 1;
    }

    printf("== baseline 0x103 (read AUX_CORE_BOOT_0/1)...\n");
    r = mon_call(0x103, 0, 0);
    printf("   ret r0=%08x\n", r);

    l2dump("before");
    tag0  = pl310[0x108 / 4];   /* 0x728 */
    data0 = pl310[0x10C / 4];   /* 0x72C */
    printf("observed: tag(0x728)=%08x data(0x72C)=%08x\n", tag0, data0);

    printf("== 0x112 accept-test: set CURRENT values (tag=%08x data=%08x)...\n",
           tag0, data0);
    r = mon_call(0x112, tag0, data0);
    printf("   ret=%08x\n", r);
    l2dump("after accept-test");

    printf("== 0x112 change: data -> 0x222 (slower, all fields 2)...\n");
    r = mon_call(0x112, tag0, 0x222);
    printf("   ret=%08x\n", r);
    l2dump("after change");

    printf("== 0x112 restore: data -> %08x...\n", data0);
    r = mon_call(0x112, tag0, data0);
    printf("   ret=%08x\n", r);
    l2dump("after restore");

    printf("== also: tag -> 0x222, data back to %08x...\n", data0);
    r = mon_call(0x112, 0x222, data0);
    printf("   ret=%08x\n", r);
    l2dump("after tag change");

    printf("== restore tag to %08x...\n", tag0);
    r = mon_call(0x112, tag0, data0);
    printf("   ret=%08x\n", r);
    l2dump("final");

    printf("LATT112 DONE\n");
    return 0;
}
