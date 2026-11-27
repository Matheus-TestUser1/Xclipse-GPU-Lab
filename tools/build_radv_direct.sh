#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
: "${ANDROID_NDK_HOME:?set ANDROID_NDK_HOME}"
: "${MESA_SRC:?set MESA_SRC to the radv-xclipse source checkout}"
: "${GLSLC:=glslc}"
out="${1:-$root/build-radv-direct}"
mkdir -p "$out"
for x in spirv-as spirv-dis spirv-val; do command -v "$x" >/dev/null; done
python3 "$root/tools/generate_compute.py" "$GLSLC" "$root/app/src/main/cpp/probe.comp" "$out/compute_generated.inc"
host="${NDK_HOST_TAG:-linux-x86_64}"
cxx="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$host/bin/aarch64-linux-android34-clang++"
"$cxx" -std=c++17 -O2 -static-libstdc++ -I"$MESA_SRC/include" -I"$MESA_SRC/include/android_stub" -I"$out" "$root/tools/radv_direct.cpp" -ldl -o "$out/radv-direct"
echo "$out/radv-direct"
