/* smctest.c — test whether QNX 6.6 emulates userland SMC (undef) instructions.
 * If yes: r12 = TI monitor API number, r0.. = args, return in r0/r1.
 * Tests:
 *   0x103 = read AUX_CORE_BOOT_0/1 (expect r0=boot0, r1=boot1)
 *   0x106 = read WKG_CONTROL_x for CPU id in r0 (expect 0x700 for CPU1)
 * Both are read-only monitor services (TRM Tables 27-63 / 27-66).
 * build: arm-unknown-nto-qnx6.6.0eabi-gcc -o smctest smctest.c
 */
#include <stdio.h>
#include <stdint.h>

__asm__(".arch armv7-a\n.arch_extension sec\n");

static inline uint32_t smc_call(uint32_t fnid, uint32_t a, uint32_t b,
                                uint32_t *out1)
{
    uint32_t ret;
    __asm__ volatile(
        "dsb                 \n"
        "mov r0, %1          \n"
        "mov r1, %2          \n"
        "mov r12, %3         \n"
        "smc #0              \n"
        "mov %0, r0          \n"
        : "=r"(ret)
        : "r"(a), "r"(b), "r"(fnid)
        : "r0", "r1", "r12", "memory");
    if (out1) {
        /* r1 may hold a second return value (0x103); re-read is tricky in the
         * same asm block — the caller re-runs with out1 handling if needed */
        *out1 = 0;
    }
    return ret;
}

/* variant returning r0 and r1 */
static inline uint32_t smc_call2(uint32_t fnid, uint32_t a,
                                 uint32_t *out0, uint32_t *out1)
{
    uint32_t r0, r1;
    __asm__ volatile(
        "dsb                 \n"
        "mov r0, %2          \n"
        "mov r12, %3         \n"
        "smc #0              \n"
        "mov %0, r0          \n"
        "mov %1, r1          \n"
        : "=r"(r0), "=r"(r1)
        : "r"(a), "r"(fnid)
        : "memory");
    *out0 = r0;
    *out1 = r1;
    return 0;
}

int main(void)
{
    uint32_t v0, v1;

    printf("test 0x103 (read AUX_CORE_BOOT_0/1)...\n");
    fflush(stdout);
    smc_call2(0x103, 0, &v0, &v1);
    printf("  r0=%08x r1=%08x  (WUGEN mirror: 6 / 40304000)\n", v0, v1);

    printf("test 0x106 (read WKG_CONTROL_1, cpu=1)...\n");
    fflush(stdout);
    v0 = smc_call(0x106, 1, 0, 0);
    printf("  r0=%08x  (WUGEN live: 00000700)\n", v0);

    printf("survived — QNX emulates userland SMC\n");
    return 0;
}
