/* i2c-hammer.c — keep I2C4 (0x48350000, /dev/i2c3) clocked by looping the
 * exact SEND devctl the FAN5702 LED driver uses (see LED-RE.md).
 * QNX gates the i2c module when idle; a gated-module access = data abort for
 * the bare-metal payload. Gating needs code to run — this loop runs until
 * the kexec jump freezes the machine, so the clock stays on across the jump.
 *
 * usage: i2c-hammer [reg_hex [val_hex]]    (default: GENERAL 0x10 = 0x00 off)
 * stop with: kill <pid> / `slay i2c-hammer`
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/iomgr.h>

#define DCMD_I2C_SEND   0x80100505

int main(int argc, char **argv)
{
    uint8_t buf[18];
    uint32_t reg = 0x10, val = 0x00;
    int fd, i;
    unsigned long n = 0;

    if (argc > 1) reg = strtoul(argv[1], NULL, 16);
    if (argc > 2) val = strtoul(argv[2], NULL, 16);

    fd = open("/dev/i2c3", O_RDWR);
    if (fd < 0) { perror("/dev/i2c3"); return 1; }

    memset(buf, 0, sizeof(buf));
    *(uint32_t *)(buf + 0)  = 0x36;         /* slave (7-bit)      */
    *(uint32_t *)(buf + 4)  = 2;            /* len                */
    *(uint32_t *)(buf + 8)  = 2;            /* stop               */
    *(uint32_t *)(buf + 12) = 1;            /* restart            */
    buf[16] = (uint8_t)reg;
    buf[17] = (uint8_t)val;

    for (i = 0; ; i = (i + 1) & 0x3ff) {
        if (devctl(fd, DCMD_I2C_SEND, buf, sizeof(buf), NULL) != 0) {
            perror("devctl");
            return 1;
        }
        n++;
        if ((n & 0x3ff) == 0) {
            fprintf(stderr, "hammer: %lu tx\n", n);
            fflush(stderr);
        }
        /* no delay: keep the module clocked ~continuously */
    }
    return 0;
}
