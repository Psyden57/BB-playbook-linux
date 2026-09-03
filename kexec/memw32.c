/* memw32.c — single 32-bit physical register write (PlayBook kexec work).
 * usage: memw32 <physaddr-hex> <value-hex> [mask-hex]
 *   with mask: writes (old & ~mask) | (value & mask)  [read-modify-write]
 * Same rules as memdump3: one specific address, PROT_NOCACHE, address must be
 * proven live/clocked (pidin mem of a driver that maps it, or TRM + live module).
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

int main(int argc, char **argv)
{
    uint64_t addr;
    uint32_t val, mask = 0xFFFFFFFFu, old, new;
    volatile uint32_t *p;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <physaddr-hex> <value-hex> [mask-hex]\n", argv[0]);
        return 1;
    }
    addr = strtoull(argv[1], NULL, 16);
    val  = (uint32_t)strtoul(argv[2], NULL, 16);
    if (argc > 3) mask = (uint32_t)strtoul(argv[3], NULL, 16);

    ThreadCtl(_NTO_TCTL_IO, 0);
    p = mmap_device_memory(0, 4, PROT_READ | PROT_WRITE | PROT_NOCACHE,
                           MAP_SHARED | MAP_PHYS, addr);
    if (p == MAP_FAILED) { perror("mmap_device_memory"); return 1; }

    old = *p;
    new = (old & ~mask) | (val & mask);
    *p = new;
    printf("0x%08llx: 0x%08x -> 0x%08x (readback 0x%08x)\n",
           (unsigned long long)addr, old, new, *p);
    munmap_device_memory((void *)p, 4);
    return 0;
}
