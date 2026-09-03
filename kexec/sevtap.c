/* sevtap.c — execute SEV from userland (unprivileged hint instruction on ARMv7).
 * If CPU1 is sitting in the ROM pen (WFE), this wakes it; the pen then evaluates
 * AUX_CORE_BOOT_0/1 (which the T2 payload already set to stub/0x6).
 */
#include <stdio.h>

int main(void)
{
    __asm__ volatile(
        "dsb \n"
        "sev \n"
        "dsb \n"
        ::: "memory");
    printf("SEV sent\n");
    return 0;
}
