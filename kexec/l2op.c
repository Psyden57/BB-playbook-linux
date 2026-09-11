/* l2op.c — single PL310 line-op probe, one op per process (session 10).
 * usage: l2op <op> <pa-hex>   op = 0x768 | 0x770 | 0x7F0
 * Writes the PA to the chosen PL310 register + bounded sync + NS readback
 * of the ctrl/aux block (reads proven safe). If the register write is
 * NS-filtered, this process dies (SIGBUS/SIGSEGV) and the next one runs.
 * build: arm-unknown-nto-qnx6.6.0eabi-gcc -o l2op l2op.c
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

#define PL310_PA 0x48242000ULL

int main(int argc, char **argv)
{
    volatile uint32_t *pl310;
    uint32_t op, pa;
    unsigned n;

    if (argc != 3) { fprintf(stderr, "usage: %s <op-hex> <pa-hex>\n", argv[0]); return 2; }
    op = (uint32_t)strtoul(argv[1], NULL, 16);
    pa = (uint32_t)strtoul(argv[2], NULL, 16);

    setvbuf(stdout, NULL, _IONBF, 0);
    ThreadCtl(_NTO_TCTL_IO_PRIV, 0);
    pl310 = mmap_device_memory(0, 0x1000, PROT_READ | PROT_WRITE | PROT_NOCACHE,
                               MAP_SHARED | MAP_PHYS, PL310_PA);
    if (pl310 == MAP_FAILED) { perror("pl310"); return 1; }

    printf("l2op: writing %08x to PL310+%#x (pa=%08x)...\n", pa, op, pa);
    pl310[op / 4] = pa;
    n = 1000000;
    pl310[0x730 / 4] = 0;
    while ((pl310[0x730 / 4] & 1) && --n)
        ;
    printf("l2op: survived; sync %s after %u polls; ctrl=%08x aux=%08x tag=%08x data=%08x\n",
           n ? "clear" : "TIMEOUT", 1000000 - n,
           pl310[0x100 / 4], pl310[0x104 / 4],
           pl310[0x108 / 4], pl310[0x10C / 4]);
    return 0;
}
