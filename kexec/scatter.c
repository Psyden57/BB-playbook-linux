/* scatter.c — mark candidate DRAM pages, then read them back after a
 * full qnx2linux jump + WDT2 reset + QNX reboot cycle.
 *
 *   scatter write   : write magic+index to each candidate page
 *   scatter read    : report which pages survived intact
 *
 * Churn is nondeterministic per QNX boot (observed: 0x55555555 memory-test
 * patterns, zeroed pages, QNX code pages). This finds the survivors so the
 * kernel DEBUG_LL ring can be parked at a reliable address.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

#define MAGIC       0x5CA5CA5Cu
#define MAGIC2      0x11223344u

static const uint32_t pages[] = {
    0x85000000, 0x87000000, 0x89000000, 0x8C000000,
    0x90000000, 0x94000000, 0x98000000, 0x9C000000,
    0x9F000000, 0x9FE00000, 0xA0000000, 0xA4000000,
    0xA8000000, 0xAC000000, 0xB0000000, 0xB4000000,
    0xB8000000, 0xBC000000, 0xBE000000, 0xBF000000,
};
#define NPAGES ((int)(sizeof(pages) / sizeof(pages[0])))

int main(int argc, char **argv)
{
    int i, mode = (argc > 1 && !strcmp(argv[1], "read")) ? 1 : 0;

    if (ThreadCtl(_NTO_TCTL_IO_PRIV, 0) == -1)
        perror("IO_PRIV");

    for (i = 0; i < NPAGES; i++) {
        volatile uint32_t *p = mmap_device_memory(0, 0x1000,
                PROT_READ | PROT_WRITE | PROT_NOCACHE,
                MAP_SHARED | MAP_PHYS, pages[i]);
        if (p == MAP_FAILED) { perror("mmap"); continue; }
        if (!mode) {
            p[0] = MAGIC;
            p[1] = (uint32_t)i;
            p[2] = MAGIC2;
        } else {
            printf("%08x: %08x %08x %08x  %s\n", pages[i], p[0], p[1], p[2],
                   (p[0] == MAGIC && p[2] == MAGIC2 &&
                    p[1] == (uint32_t)i) ? "SURVIVED" : "churned");
        }
        munmap((void *)p, 0x1000);
    }
    return 0;
}
