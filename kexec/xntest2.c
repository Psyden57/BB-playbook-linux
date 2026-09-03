/* xntest2.c — does msync-based cache maintenance fix data-page execution
 * after an L1 flush? Probes (separate runs via argv):
 *   1: memcpy code -> call                                  (control, works?)
 *   2: memcpy code -> clean_inval_l1_all() -> call          (expected crash)
 *   3: memcpy code -> msync(MS_INVALIDATE|MS_CACHE_ONLY) -> call
 *   4: memcpy code -> msync(MS_INVALIDATE_ICACHE|MS_CACHE_ONLY) -> call
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

__asm__(".arch armv7-a\n.arch_extension sec\n");

extern void clean_inval_l1_all(void);

static const uint32_t bxlr = 0xE12FFF1Eu;

typedef void (*fn_t)(void);

int main(int argc, char **argv)
{
    int mode = (argc > 1) ? atoi(argv[1]) : 1;
    setvbuf(stdout, NULL, _IONBF, 0);

    if (ThreadCtl(_NTO_TCTL_IO_PRIV, 0) == -1) perror("IO_PRIV");
    printf("mode %d: mapping\n", mode);

    uint32_t *pg = mmap(0, 0x1000, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, 0);
    if (pg == MAP_FAILED) { perror("mmap"); return 1; }
    memcpy(pg, &bxlr, 4);
    printf("mapped+copied\n");

    if (mode == 2) {
        printf("flushing L1 (cp15)...\n");
        clean_inval_l1_all();
        printf("flushed\n");
    } else if (mode == 3) {
        printf("msync INVALIDATE|CACHE_ONLY...\n");
        if (msync(pg, 0x1000, MS_INVALIDATE | MS_CACHE_ONLY) == -1)
            perror("msync");
        printf("msync done\n");
    } else if (mode == 4) {
        printf("msync INVALIDATE_ICACHE|CACHE_ONLY...\n");
        if (msync(pg, 0x1000, MS_INVALIDATE_ICACHE | MS_CACHE_ONLY) == -1)
            perror("msync");
        printf("msync done\n");
    }

    printf("calling...\n");
    ((fn_t)pg)();
    printf("EXEC OK — mode %d works\n", mode);
    return 0;
}
