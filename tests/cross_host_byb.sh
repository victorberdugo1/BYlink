#!/bin/sh
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
work=${TMPDIR:-/tmp}/bylink-cross-host
rm -rf "$work"
mkdir -p "$work"
sources=$(find "$root/compiler" -name '*.cpp' ! -path '*/driver/main.cpp' ! -path '*/driver/pipeline.cpp' -o -name 'main.cpp' -path '*/driver/*' -o -name 'pipeline.cpp' -path '*/driver/*' | sort)

digest() {
  runner=$1
  bylc=$2
  for program in "$root"/examples/*.by "$root"/bench/corpus/*.by; do
    for level in -O0 -O2; do
      $runner "$bylc" "$program" $level --emit-byb -o "$work/out.byb"
      printf '%s %s %s\n' "$(basename "$program")" "$level" "$(sha256sum < "$work/out.byb" | cut -d' ' -f1)"
    done
  done
}

g++ -std=c++20 -O1 -I"$root/compiler" $sources -o "$work/bylc-native"
digest "" "$work/bylc-native" > "$work/native.txt"

status=0
for host in "aarch64-linux-gnu qemu-aarch64" "s390x-linux-gnu qemu-s390x" "riscv64-linux-gnu qemu-riscv64"; do
  set -- $host
  command -v "$1-g++" >/dev/null && command -v "$2" >/dev/null || { echo "SKIPPED $1: $1-g++ or $2 not installed"; continue; }
  "$1-g++" -std=c++20 -O1 -static -I"$root/compiler" $sources -o "$work/bylc-$1"
  digest "$2" "$work/bylc-$1" > "$work/$1.txt"
  if cmp -s "$work/native.txt" "$work/$1.txt"; then
    echo "IDENTICAL $1 ($(wc -l < "$work/native.txt") encodings, executed under $2)"
  else
    echo "DIFFERENT $1"
    status=1
  fi
done
exit $status
