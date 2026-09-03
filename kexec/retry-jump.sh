#!/bin/bash
# retry-jump.sh — repeat the jump until the monitor 0x102 L2-disable SMC
# succeeds (mirror0 marker 71), up to MAX attempts. Each failed attempt
# costs one WDT2 reboot (~2-3 min): mirror0=70-without-71 = SMC hung.
# Success signature: mirror0=71 (SMC returned). Then the kernel ladder in
# the primary bc decides the rest (the device may NOT come back if the
# kernel boots — the loop detects that too and stops).
set -u
cd "$(dirname "$0")"
MAX=${1:-6}
for n in $(seq 1 "$MAX"); do
    echo "================ ATTEMPT $n/$MAX ================"
    ./jump.sh zImage 2>&1 | grep -vE "^== waiting for device ==$"
    # jump.sh already printed the readbacks; parse mirror0 step from the
    # last run's output is fragile — read the device directly if it is up.
    m0=$(timeout 6 ssh -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa \
        -o PubkeyAcceptedKeyTypes=+ssh-rsa -o MACs=+hmac-sha1 -o ConnectTimeout=6 \
        -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 94000000 0x8" 2>/dev/null \
        | awk '/94000004/ {print $2}')
    p1=$(timeout 6 ssh -o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa \
        -o PubkeyAcceptedKeyTypes=+ssh-rsa -o MACs=+hmac-sha1 -o ConnectTimeout=6 \
        -i ../rsa root@169.254.0.1 "on -C 0 /tmp/memdump3 90000004 0x4" 2>/dev/null \
        | awk '/90000004/ {print $2}')
    echo ">>> attempt $n: mirror0=$m0 primary_bc1=$p1"
    case "$m0" in
        00000047)   # 71 decimal
            echo ">>> SMC RETURNED (mirror0=71). L2 disabled. Kernel ladder:"
            echo ">>> primary bc[1]=$p1  (>=0x64=100 means the kernel ran;"
            echo ">>> 0x8f=143/0x88=136/0x82=130/0x6e=110/0x6f=111 = ladder zones)"
            exit 0 ;;
        00000046)   # 70 decimal
            echo ">>> SMC hung again (70 without 71) — retrying." ;;
        "")
            echo ">>> device unreachable post-run — if it stays down, the kernel"
            echo ">>> may be RUNNING (check: no WDT2 reboot = success)."
            read -r -p "Press enter for next attempt, ctrl-c to stop..." _ ;;
        *)
            echo ">>> unexpected mirror0=$m0 — inspect manually." ;;
    esac
    sleep 5
done
echo ">>> $MAX attempts exhausted."
exit 1
