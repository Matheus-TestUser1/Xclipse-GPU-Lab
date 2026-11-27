#!/usr/bin/env bash
set -euo pipefail
if [[ $# != 3 ]]; then echo "Usage: bash tools/run_radv_direct.sh <radv-direct> <driver.so> <suite>" >&2; exit 2; fi
case "$3" in smoke|fma|hp|operands|roles|special|bench|dynamic) ;; *) exit 2;; esac
dir="/data/local/tmp/xclipse-gpulab-direct"
out="radv-direct-$3-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$out"
sha256sum "$1" "$2" > "$out/host-sha256.txt"
adb shell "mkdir -p $dir"
adb push "$1" "$dir/radv-direct"
adb push "$2" "$dir/vulkan.radeon.so"
adb shell "chmod 700 $dir/radv-direct"
adb shell "sha256sum $dir/radv-direct $dir/vulkan.radeon.so" > "$out/device-sha256.txt"
adb shell getprop ro.build.fingerprint > "$out/build.txt"
adb shell uname -a > "$out/kernel.txt"
adb shell 'cat /sys/kernel/gpu/gpu_info 2>&1' > "$out/gpu-before.txt" || true
set +e
adb shell "cd $dir && LD_LIBRARY_PATH=$dir MESA_SHADER_CACHE_DISABLE=1 RADV_XCLIPSE_COOPMAT_ALU=1 ./radv-direct $dir/vulkan.radeon.so $3" > "$out/report.txt" 2>&1
rc=$?
set -e
echo "$rc" > "$out/exit.txt"
adb shell 'cat /sys/kernel/gpu/gpu_info 2>&1' > "$out/gpu-after.txt" || true
echo "$out (exit $rc)"
exit "$rc"
