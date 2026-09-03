/* memdump3.c — safe per-address physical memory dumps for the PlayBook kexec work.
 * v3 rules (PLAYBOOK-REFERENCE §10.2/2b):
 *  - one specific physical address per invocation (no bus sweeping, ever)
 *  - mmap_device_memory with PROT_READ|PROT_NOCACHE
 *  - caller must verify the address is live/clocked (pidin mem of a driver
 *    that maps it, or TRM + always-on domain) BEFORE running this
 * usage: memdump3 <physaddr-hex> <size-hex> [outfile]
 *        size rounded up to 4; printed as u32 words with offsets.
 * QNX 6.6, build with arm-unknown-nto-qnx6.6.0eabi-gcc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

int main(int argc, char **argv)
{
    uint64_t addr, size;
    FILE *out = stdout;
    volatile uint32_t *p;
    unsigned i;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <physaddr-hex> <size-hex> [outfile]\n", argv[0]);
        return 1;
    }
    addr = strtoull(argv[1], NULL, 16);
    size = strtoull(argv[2], NULL, 16);
    size = (size + 3u) & ~3ull;
    if (size == 0 || size > 0x10000) {
        fprintf(stderr, "size 1..0x10000 please\n");
        return 1;
    }
    if (argc > 3) {
        out = fopen(argv[3], "w");
        if (!out) { perror("fopen"); return 1; }
    }
    /* IO privilege: required by mmap_device_memory. Try TCTL_IO; some 6.6
     * configurations reject it with EINVAL for sshd sessions, in which case
     * fall through and let mmap_device_memory fail if rights are missing. */
    ThreadCtl(_NTO_TCTL_IO, 0);
    p = mmap_device_memory(0, size, PROT_READ | PROT_NOCACHE,
                           MAP_SHARED | MAP_PHYS, addr);
    if (p == MAP_FAILED) {
        perror("mmap_device_memory");
        return 1;
    }
    for (i = 0; i < size / 4; i++) {
        if (out == stdout)
            fprintf(out, "%08llx: %08x\n",
                    (unsigned long long)(addr + i * 4), p[i]);
        else
            fprintf(out, "%08x", p[i]);
    }
    munmap_device_memory((void *)p, size);
    if (out != stdout) fclose(out);
    return 0;
}
