#!/bin/bash
# mkkernel.sh — package the built kernel for the T3 jump.
# Usage: mkkernel.sh [zImage|Image]   (default zImage)
# Produces the kernel image with the winchester DTB appended (CONFIG_ARM_APPENDED_DTB
# finds it at image end; the payload ALSO passes r2=DTB phys — belt and braces).
# Zero padding before the DTB is harmless: entry scans use their own symbols.
set -e
LINUX=${LINUX:-/home/psyden/kernel/linux}
OUT=${OUT:-/home/psyden/playbook-dev/kexec/kernel}
KIMG=${1:-zImage}
DTB=arch/arm/boot/dts/ti/omap/omap4-winchester.dtb

cd "$LINUX"
mkdir -p "$OUT"
zsize=$(wc -c < "arch/arm/boot/$KIMG")
pad=$(( (8 - zsize % 8) % 8 ))
cp "arch/arm/boot/$KIMG" "$OUT/$KIMG"
[ "$pad" -ne 0 ] && dd if=/dev/zero bs=1 count="$pad" >> "$OUT/$KIMG" 2>/dev/null
cat "$DTB" >> "$OUT/$KIMG"
cp "$DTB" "$OUT/"
echo "packed: $OUT/$KIMG ($(wc -c < "$OUT/$KIMG") B = $KIMG ${zsize}B + pad ${pad} + DTB $(wc -c < "$DTB")B)"
