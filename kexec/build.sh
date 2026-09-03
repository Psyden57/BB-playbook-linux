#!/bin/bash
# build.sh — build QNX-side kexec tools (run after sourcing qnx-env.sh)
set -e
cd "$(dirname "$0")"
. ../qnx-env.sh
AS=arm-unknown-nto-qnx6.6.0eabi-as
LD=arm-unknown-nto-qnx6.6.0eabi-ld
OC=arm-unknown-nto-qnx6.6.0eabi-objcopy
CC=arm-unknown-nto-qnx6.6.0eabi-gcc

$AS -o hello.o hello.S
$LD -Ttext=0x40305000 -e hello_start -o hello.elf hello.o
$OC -O binary hello.elf hello.bin

$AS -o stub.o stub.S
$LD -Ttext=0x40304000 -e _start -o stub.elf stub.o
$OC -O binary stub.elf stub.bin
$AS -o stub2.o stub2.S
$LD -Ttext=0x40308000 -e _start -o stub2.elf stub2.o
$OC -O binary stub2.elf stub2.bin
$AS -o stub3.o stub3.S
$AS -o probe.o probe.S
$LD -Ttext=0x40309000 -e probe_start -o probe.elf probe.o
$OC -O binary probe.elf probe.bin

$CC -O1 -marm -o qnx2linux qnx2linux.c cacheops.S stub3.o
$CC -O1 -marm -o memtest memtest.c
$CC -O1 -o memdump3 memdump3.c
$CC -O1 -o memw32 memw32.c
$CC -O1 -o secure-probe secure-probe.c
$CC -O1 -o scatter scatter.c
$CC -O1 -o i2c-hammer i2c-hammer.c

echo "built: hello.bin ($(wc -c < hello.bin) B), stub.bin ($(wc -c < stub.bin) B), qnx2linux"
arm-unknown-nto-qnx6.6.0eabi-objdump -d hello.elf | head -40
