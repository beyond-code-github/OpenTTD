#!/usr/bin/env bash
# usage: runjt.sh <binary> <name> <filter> ; per-case processes like tools/junction/jt
bin=$1; name=$2; filter=$3; S=$(dirname "$0"); d=$S/$name.d; rm -rf $d; mkdir -p $d
cd /home/pete/OpenTTD/.claude/o4c2/build-rel
"$bin" "$filter" --list-test-names-only 2>/dev/null | sed '/^$/d' > $d/names
sed -e 's/[][\\,"]/\\&/g' $d/names > $d/specs
export bin d
start=$(date +%s%N)
n=0; while IFS= read -r spec; do n=$((n+1)); printf "%s\0%s\0" "$n" "$spec"; done < $d/specs | xargs -0 -n 2 -P 14 sh -c '"$bin" "$2" --durations yes > "$d/$1.log" 2>&1 || echo "$1" >> "$d/failed"' _
echo "wall $(( ($(date +%s%N)-start)/1000000 )) ms cases $(wc -l < $d/names) failed $( [ -s $d/failed ] && wc -l < $d/failed || echo 0)"
cat $d/[0-9]*.log | grep -E '^[0-9]+\.[0-9]+ s: ' | sort -rn | head -8
