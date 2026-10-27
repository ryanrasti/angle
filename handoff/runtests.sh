#!/usr/bin/env bash
# Usage: out/runtests.sh <binary> <gtest filter> <result name> [extra args]
# Runs a filter in bot mode (crash-isolated batches) and prints a pass/fail summary.
set -u
cd /home/ryan/src/angle
source out/env.sh
# Never open windows on the user's display: run on a private Xvfb with a minimal window manager.
# Mesa's Intel Vulkan driver renders on the GPU and presents through software (Xvfb has no DRI3).
out/headless.sh
export DISPLAY=:97 XAUTHORITY=/dev/null MESA_VK_WSI_DEBUG=sw
export VK_DRIVER_FILES=/run/opengl-driver/share/vulkan/icd.d/intel_icd.x86_64.json
bin=$1; filter=$2; name=$3; shift 3
json=out/results/$name.json
timeout 3600 out/${OUT:-es3}/$bin --bot-mode --gtest_filter="$filter" \
  --isolated-script-test-output=$json --batch-timeout=300 --max-failures=1000000 --test-timeout=60 --max-processes=${PROCS:-8} "$@" \
  > out/results/$name.log 2>&1
python3 out/summarize.py $json
