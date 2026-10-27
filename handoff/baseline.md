# Baseline vs. current pass rates (WebGPU backend, ES3)

- **Machine:** Intel Xe-LPG (Mesa ANV), headless Xvfb `:97`, software present.
- **Baseline:** upstream dbd678c22 plus the local test-enable hack, run from `out/es3`.
- **Current:** `out/cur`, a snapshot taken after the "buffer mapping" commit. Swizzle and later work are not included (that is why `swizzle` is unchanged).
- Raw results are in `out/results/b1_*` and `out/results/c1_*`.
- `out/summarize.py` prints totals. `out/compare.py A.json B.json -f` lists regressions and remaining failures.

| group | baseline pass | current pass | total |
|---|---|---|---|
| end2end */ES3_WebGPU (run, non-skipped) | 1474 / 3238 (271 timeouts) | 1656 / 3144 (13 timeouts) | 5062 |
| dEQP-GLES3 shaders.linkage | 31 | 91 | 119 |
| dEQP-GLES3 fragment_out | 197 | 415 | 941 |
| dEQP-GLES3 ubo | 1 | 1198 | 2358 |
| dEQP-GLES3 buffer | 1 | 572 | 696 |
| dEQP-GLES3 instanced | 1 | 9 | 46 |
| dEQP-GLES3 fence_sync | 2 | 12 | 12 |
| dEQP-GLES3 samplers | 1 | 15 | 43 |
| dEQP-GLES3 fbo.blit | 231 | 421 | 642 |
| dEQP-GLES3 occlusion_query | 1 | 3 | 67 |
| dEQP-GLES3 transform_feedback | 159 | 323 | 1321 |
| dEQP-GLES3 vertex_arrays | 1 | 1 | 1006 (all crash: translator assert, see AGENTS.md) |
| dEQP-GLES3 draw | 1 | 1 | 324 (same crash) |
| dEQP-GLES3 shaders.* | 2134 | (current run not finished) | 21244 |
| end2end */ES2_WebGPU | 1135 | 1136 (one new test), no regressions | 3639 (2504 skipped by expectations) |
| dEQP-GLES2 functional | 17160 | 17160, no regressions | 17160 |

Targeted runs on the dev build:

| suite | baseline | current |
|---|---|---|
| UniformBufferTest | 0/41 | 33/33 non-skipped; 8 skipped (row-major, block arrays) |
| buffer-related end2end | 85/138 | 123/138 |
| SwizzleTest | 0/16 | 9/16 |
| dEQP binary_operator shifts | 1/1009 | 841/1009 |
