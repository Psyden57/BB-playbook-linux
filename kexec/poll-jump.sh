#!/bin/bash
# poll-jump.sh — the W-97 live-observation instrument: wraps jump.sh with a
# FAST host-side state poller during the payload window, then defers the
# post-reboot readback to jump.sh's own flow.
#
# usage (from kexec/):  PAYLOAD_MODE=--l2on ./poll-jump.sh zImage
#
# What it settles (the W-97 check list, one run):
#   (alpha) the DEPLOYED artifact sizes on-device (ls -l /tmp/zImage + the
#           DTB) — expect zImage = 5,225,409 (the on-disk #159 pack);
#           5,226,177 = the #155 shape = an artifact-chain bug.
#   (beta)  bc[2] LIVE (= the payload's own zlen write) — the same
#           discriminator from the payload's hand.
#   (omega) the sweep heartbeat slots (bc[29]/bc[30] @ +0x74/+0x78) and the
#           sweep-survived marker (bc[1] = 36) vs the LED magenta —
#           sweep-rate vs skip, directly timestamped.
#   (WDT2)  the watchdog down-counter (4a31402c) each poll — the true jump
#           moment and the remaining window (the off->red 68 s question).
#   (gamma) post-run, on the readback logs: bc[14] (the FDT fixed-map VA,
#           low-20 = the on-disk-pack shape) vs the ring's r[0] (0xee9a8-class
#           = the #155-shape in W-96 run 2) — agreement = deploy-shape,
#           disagreement = a relocation step between the views.
#
# Notes:
# - Polls every 3 s (read-only memdump3 through SSH — established safe
#   during the payload era; the W-91 record polluted ITS own readback with
#   polls, not the boot).
# - Lease: 90 polls (~270 s) or 4 consecutive SSH misses (dark = jumped or
#   died). After dark, jump.sh's reboot-wait + readback continue (bg).
# - /tmp/wiped per reboot: ls -l runs ONCE per invocation, early (pass 1).
# - Never sends anything QNX-mutating (no slay/devctl here — jump.sh's own
#   quiesce block handles that BEFORE the payload starts).
set -u
cd "$(dirname "$0")"
KIMG=${1:-zImage}
SSHARGS="-o StrictHostKeyChecking=no -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o MACs=+hmac-sha1 -o ConnectTimeout=6 -i ../rsa"
R="root@169.254.0.1"
T0=$(date +%s)
LIVELOG="${LIVELOG:-/home/psyden/agent-runs/w97-live-$(date +%Y%m%d-%H%M%S).log}"

echo "== poll-jump: live log = $LIVELOG"
echo "== launching jump.sh ($KIMG) in the background =="
PAYLOAD_MODE=${PAYLOAD_MODE:---l2on} ./jump.sh "$KIMG" 2>&1 &
JUMP_PID=$!

# ---- the live poller -------------------------------------------------
dark=0; pass=0
while [ $pass -lt 90 ]; do
    pass=$((pass+1))
    sleep 3
    now=$(( $(date +%s) - T0 ))
    if [ $pass -eq 1 ]; then
        timeout 8 ssh $SSHARGS $R "ls -l /tmp/zImage /tmp/omap4-winchester.dtb /tmp/qnx2linux" >> "$LIVELOG" 2>&1
        echo "[alpha] deployed-file sizes captured (pass 1)" | tee -a "$LIVELOG"
    fi
    cur=$(timeout 8 ssh $SSHARGS $R \
        "on -C 0 /tmp/memdump3 90000000 0x80; on -C 0 /tmp/memdump3 4a31402c 4" \
        2>/dev/null | tr '\n' ' ')
    if [ -z "$cur" ]; then
        dark=$((dark+1))
        echo "[${now}s] miss $dark/4" | tee -a "$LIVELOG"
        if [ $dark -ge 4 ]; then
            echo "== DARK at t=${now}s — jumped or died; live poll ends ==" | tee -a "$LIVELOG"
            break
        fi
        continue
    fi
    dark=0
    echo "[${now}s] $cur" | tee -a "$LIVELOG"
done

# ---- hand the tail back to jump.sh's readback (it is still running) ----
wait $JUMP_PID
echo "== poll-jump done: $LIVELOG + the jump log = this run's record =="
