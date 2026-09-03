#!/bin/bash
# qnx-disasm.sh <qnx-arm-elf> <outfile.dis>
# QNX binaries are section-stripped; extract every executable LOAD segment
# and disassemble it raw with correct VMA. Literal pools are inline.
set -e
IN="$1"; OUT="$2"
readelf=$READELF; objdump=$OBJDUMP
[ -n "$readelf" ] || readelf=$(dirname $(which gcc 2>/dev/null) 2>/dev/null)
# default to SDP tools
PREFIX=${QNX_OBJDUMP_PREFIX:-arm-unknown-nto-qnx6.6.0eabi}
readelf=${READELF:-${PREFIX}-readelf}
objdump=${OBJDUMP:-${PREFIX}-objdump}
: > "$OUT"
$readelf -l "$IN" 2>/dev/null | awk '
/LOAD/ { print $1, $2, $3, $5 }' | while read type off vma fsz; do
    flags=$($readelf -l "$IN" 2>/dev/null | grep -A0 "LOAD.*0x$(printf %x $off)" >/dev/null && echo)
    # only disassemble segments that are executable: check flags via full listing
    line=$($readelf -l "$IN" 2>/dev/null | grep "LOAD" | grep -i " 0x$(printf '%08x' $((off))) " | head -1)
    case "$line" in
      *"R E"*|*"E "*|*"RWE"*) ;;
      *) continue;;
    esac
    tmp=$(mktemp)
    dd if="$IN" of="$tmp" bs=1 skip=$((off)) count=$((fsz)) 2>/dev/null
    echo "===== LOAD off=0x$(printf %x $off) vma=0x$(printf %x $vma) size=0x$(printf %x $fsz) =====" >> "$OUT"
    $objdump -D -b binary -m arm --adjust-vma=$((vma)) "$tmp" >> "$OUT" 2>/dev/null
    rm -f "$tmp"
done
echo "wrote $OUT ($(wc -l < "$OUT") lines)"
