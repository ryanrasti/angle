#!/usr/bin/env bash
# Runs all baseline groups with prefix $1 (e.g. b0) on build dir $OUT.
cd /home/ryan/src/angle
export OUT=${OUT:-es3}
p=$1
out/runtests.sh angle_end2end_tests '*/ES3_WebGPU*' ${p}_e2e_es3 > out/results/${p}_e2e_es3.summary 2>&1
while read name filt; do
  out/runtests.sh angle_deqp_gles3_tests "$filt" ${p}_deqp3_$name --use-angle=webgpu > out/results/${p}_deqp3_$name.summary 2>&1
done < out/deqp_groups.txt
out/runtests.sh angle_end2end_tests '*/ES2_WebGPU*' ${p}_e2e_es2 > out/results/${p}_e2e_es2.summary 2>&1
out/runtests.sh angle_deqp_gles2_tests 'dEQP-GLES2.functional.*' ${p}_deqp2 --use-angle=webgpu > out/results/${p}_deqp2.summary 2>&1
echo all done > out/results/${p}_DONE
