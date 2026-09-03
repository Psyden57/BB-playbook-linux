#!/bin/bash
# jump.sh — deploy everything, run the probe jump, wait for reboot, read back.
# usage: ./jump.sh [zImage|Image]     (default Image)
set -e
cd "$(dirname "$0")"
KIMG=${1:-Image}
SSHARGS="-o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o MACs=+hmac-sha1 -o ConnectTimeout=8 -i ../rsa"
R="ssh $SSHARGS root@169.254.0.1"

echo "== waiting for device =="
for i in $(seq 1 60); do
    timeout 6 ssh $SSHARGS root@169.254.0.1 "echo up" >/dev/null 2>&1 && break
    sleep 3
done

echo "== deploying =="
scp $SSHARGS qnx2linux probe.bin memdump3 "kernel/$KIMG" kernel/omap4-winchester.dtb \
    root@169.254.0.1:/tmp/ >/dev/null

echo "== quiescing display stack (splash/backlight/screen) =="
# The boot animation actively writes RAM (splash/screen/GPU); buffer placement
# cannot avoid regions QNX may be writing (framebuffer pages move). Kill the
# writers before the jump; DISPC keeps scanout-reading the last frame, which
# is harmless. Everything restarts on the WDT2 reboot.
timeout 30 ssh $SSHARGS root@169.254.0.1 \
    "slay splash 2>/dev/null; slay backlight_win 2>/dev/null; slay screen 2>/dev/null; sleep 1; echo display quiesced" 2>&1 | tail -2 || true

echo "== jumping (LED: blue -> off -> default -> off -> default -> off) =="
# Launch the payload DETACHED: a setup stall (bc 39-41 eMMC/sync, seen
# 2026-09-01) must never get the payload SIGHUP-killed by an SSH timeout —
# that cost us a run. Output goes to /tmp/jump.log; we then poll the bc page
# directly to watch the ladder live until the jump (SSH dies at the CPU1 hold).
PAYLOAD_MODE=${PAYLOAD_MODE:---probe}
timeout 20 ssh $SSHARGS root@169.254.0.1 \
    "on -C 0 /tmp/qnx2linux $PAYLOAD_MODE /tmp/$KIMG /tmp/omap4-winchester.dtb /tmp/probe.bin >/tmp/jump.log 2>&1 &" 2>&1 || true

echo "== payload launched detached; polling bc ladder =="
prev=""
miss=0
for i in $(seq 1 36); do
    sleep 5
    cur=$(timeout 6 ssh $SSHARGS root@169.254.0.1 \
        "on -C 0 /tmp/memdump3 90000000 0x40" 2>/dev/null | tr '\n' ' ')
    if [ -z "$cur" ]; then
        miss=$((miss+1))
        # transient SSH failures under memtest load are normal; only a
        # sustained outage means the machine jumped or died
        [ $miss -ge 4 ] && { echo "== ssh gone (jumped or device died) @ t=$((i*5))s =="; break; }
        continue
    fi
    miss=0
    step=$(echo "$cur" | awk '{print $2}')
    [ "$step" != "$prev" ] && { echo "--- t=$((i*5))s  bc[1]=$step ---"; echo "$cur"; prev="$step"; }
done || true

echo "== payload console log (if reachable) =="
timeout 10 ssh $SSHARGS root@169.254.0.1 "cat /tmp/jump.log" 2>&1 || true

echo "== waiting for reboot =="
for i in $(seq 1 80); do
    timeout 6 ssh $SSHARGS root@169.254.0.1 "echo up" >/dev/null 2>&1 && break
    sleep 3
done

echo "== breadcrumbs =="
scp $SSHARGS memdump3 root@169.254.0.1:/tmp/ >/dev/null
ssh $SSHARGS root@169.254.0.1 "on -C 0 /tmp/memdump3 90000000 0x40" 2>&1
echo "== mirror0 0x94000000 (enter_stub pre/post-SMC markers 70/71) =="
ssh $SSHARGS root@169.254.0.1 "on -C 0 /tmp/memdump3 94000000 0x8" 2>&1
echo "== mirror1 0x88000000 =="
ssh $SSHARGS root@169.254.0.1 "on -C 0 /tmp/memdump3 88000000 0x8" 2>&1
echo "== mirror2 0x9FE00000 (trap vectors; abort flag @ +0xA0) =="
ssh $SSHARGS root@169.254.0.1 "on -C 0 /tmp/memdump3 9fe00000 0x8; on -C 0 /tmp/memdump3 9fe000a0 0x4" 2>&1

echo "== ring1 (chars @0x88000100, count @0x88000080) =="
ssh $SSHARGS root@169.254.0.1 "on -C 0 /tmp/memdump3 88000080 0x20" 2>&1
