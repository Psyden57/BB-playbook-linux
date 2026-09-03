/* secure-probe.c — /dev/trustzone raw SMC passthrough probe (PlayBook, QNX 6.6)
 *
 * Uses the raw SMC devctl of trustzone-omap4 (RE: PLAYBOOK-REFERENCE §1.10):
 *   dcmd 0xC0280501, 40-byte payload:
 *     {u32 result_out; u32 fnid; u32 a1; u32 a2; u32 n; u32 w[4]; u32 pad[2];}
 *   driver: r0=fnid r1=a1 r2=a2 r3=phys{u32 n; u32 w[n]} -> smc 1, result=r0 back.
 * Known-good reference calls (libsecure_dispatcher RE):
 *   0x12 rng_hwRNGen : a1=0 a2=4 n=2 w={len, outptr}
 *   0x31 kds_select_kek: a1=1 a2=0 n=0
 * Monitor return codes (TI API, mainline omap-secure.h):
 *   0x00 OK · 0x01 FAIL · 0xFFFFFFFE NS2S conversion error · 0xFFFFFFFF unknown service
 *
 * Guardrails: only 0x12 (rng) and 0x103 (auxcoreboot0 read, TI SMC#0 API number
 * probed via r0 to test RIM dispatcher routing) run without --force.
 *
 * usage:
 *   secure-probe rng [len]
 *   secure-probe call <fnid-hex> <a1> <a2> <n> [w0..w3]   (n<=4; --force for unknowns)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <devctl.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

#define TZ_RAW_SMC_DCMD 0xC0280501

struct tz_smc {
    uint32_t result;
    uint32_t fnid;
    uint32_t a1;
    uint32_t a2;
    uint32_t n;
    uint32_t w[4];
    uint32_t pad[2];
};

static const char *retname(uint32_t r)
{
    switch (r) {
    case 0x00000000: return "OK";
    case 0x00000001: return "FAIL";
    case 0xFFFFFFFE: return "NS2S_CONVERSION_ERROR";
    case 0xFFFFFFFF: return "SERVICE_UNKNOWN";
    default:         return "";
    }
}

static int tz_open(void)
{
    int fd = open("/dev/trustzone", O_RDWR);
    if (fd < 0) { perror("open(/dev/trustzone)"); return -1; }
    return fd;
}

/* returns 0 on devctl success (check s.result for monitor status) */
static int tz_call(int fd, struct tz_smc *s)
{
    int rc = devctl(fd, TZ_RAW_SMC_DCMD, s, sizeof *s, NULL);
    if (rc != EOK) {
        fprintf(stderr, "devctl: rc=%d errno=%d (%s)\n", rc, errno, strerror(errno));
        return -1;
    }
    printf("smc(0x%x, a1=0x%x, a2=0x%x, n=%u, w=[%08x %08x %08x %08x]) -> result=0x%08x %s\n",
           s->fnid, s->a1, s->a2, s->n,
           s->w[0], s->w[1], s->w[2], s->w[3],
           s->result, retname(s->result));
    return 0;
}

int main(int argc, char **argv)
{
    int fd, force = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: %s rng [len] | call [--force] <fnid-hex> <a1> <a2> <n> [w0..w3]\n", argv[0]);
        return 1;
    }
    fd = tz_open();
    if (fd < 0) return 1;

    if (!strcmp(argv[1], "rng")) {
        unsigned len = (argc > 2) ? (unsigned)strtoul(argv[2], 0, 0) : 16;
        uint8_t *buf;
        off64_t phys = 0;
        size_t contig = 0;
        struct tz_smc s;
        unsigned i;

        if (len > 256) len = 256;
        /* monitor works on PHYSICAL addresses (libsecure_dispatcher uses
         * mem_offset64) — allocate pinned contiguous memory and translate */
        buf = mmap(0, 4096, PROT_READ | PROT_WRITE,
                   MAP_ANON | MAP_PHYS | MAP_SHARED, NOFD, 0);
        if (buf == MAP_FAILED) { perror("mmap(MAP_ANON|MAP_PHYS)"); return 1; }
        if (mem_offset64(buf, NOFD, 4096, &phys, &contig) == -1) {
            perror("mem_offset64");
            return 1;
        }
        memset(&s, 0, sizeof s);
        s.fnid = 0x12;
        s.a1 = 0;
        s.a2 = 4;
        s.n = 2;
        s.w[0] = (uint32_t)phys;
        s.w[1] = len;
        if (tz_call(fd, &s) != 0) return 1;
        printf("buffer v=%p phys=%llx (%llu contiguous):", (void *)buf,
               (unsigned long long)phys, (unsigned long long)contig);
        for (i = 0; i < (len > 32 ? 32 : len); i++) printf(" %02x", buf[i]);
        printf("%s\n", len > 32 ? " ..." : "");
        if (s.result == 0) {
            int changed = 0;
            for (i = 0; i < len; i++) if (buf[i] != 0) changed = 1;
            printf("verdict: RNG %s\n", changed
                   ? "FILLED (passthrough + physical-pointer path WORKS)"
                   : "returned OK but buffer untouched (try count-first param order)");
        }
    } else if (!strcmp(argv[1], "call")) {
        int ai = 2;
        struct tz_smc s;
        unsigned i;

        memset(&s, 0, sizeof s);
        if (ai < argc && !strcmp(argv[ai], "--force")) { force = 1; ai++; }
        if (argc - ai < 4) { fprintf(stderr, "call <fnid-hex> <a1> <a2> <n> [w0..w3]\n"); return 1; }
        s.fnid = (uint32_t)strtoul(argv[ai], 0, 0);
        s.a1   = (uint32_t)strtoul(argv[ai + 1], 0, 0);
        s.a2   = (uint32_t)strtoul(argv[ai + 2], 0, 0);
        s.n    = (uint32_t)strtoul(argv[ai + 3], 0, 0);
        for (i = 0; i < 4 && argc - ai - 4 > (int)i; i++)
            s.w[i] = (uint32_t)strtoul(argv[ai + 4 + i], 0, 0);
        if (s.n > 4) { fprintf(stderr, "n>4 not supported by driver payload\n"); return 1; }
        /* guardrail: unknown services need --force (read-only probes 0x103 exempt) */
        if (!force && s.fnid != 0x12 && s.fnid != 0x103 && s.fnid != 0x31) {
            fprintf(stderr, "refusing unverified service 0x%x without --force\n", s.fnid);
            return 1;
        }
        return (tz_call(fd, &s) == 0) ? 0 : 1;

    } else if (!strcmp(argv[1], "sweep")) {
        /* probe a list of service ids with EMPTY param lists (count=0).
         * skips known side-effect-heavy ids unless --force given. */
        unsigned i, id;
        int force = (argc > 2 && !strcmp(argv[2], "--force"));
        unsigned skip[] = {0x1a, 0x1b, 0x1c, 0x1d, 0x21, 0x23, 0x102, 0x108, 0x109, 0x113};
        unsigned j, dangerous;
        struct tz_smc s;
        for (i = 2; i < (unsigned)argc; i++) {
            if (!strcmp(argv[i], "--force")) continue;
            id = (unsigned)strtoul(argv[i], 0, 0);
            dangerous = 0;
            for (j = 0; j < sizeof skip / sizeof skip[0]; j++)
                if (id == skip[j]) dangerous = 1;
            if (dangerous && !force) {
                printf("0x%03x: SKIPPED (side-effect risk)\n", id);
                continue;
            }
            memset(&s, 0, sizeof s);
            s.fnid = id;
            s.a1 = 0;
            s.a2 = 4;
            s.n = 0;
            if (devctl(fd, TZ_RAW_SMC_DCMD, &s, sizeof s, NULL) != EOK) {
                printf("0x%03x: devctl error\n", id);
                continue;
            }
            printf("0x%03x: 0x%08x %s\n", id, s.result, retname(s.result));
        }
        return 0;
    } else {
        fprintf(stderr, "unknown command %s\n", argv[1]);
        return 1;
    }
    return 0;
}
