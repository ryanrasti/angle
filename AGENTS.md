# Notes for agents: ES 3 on ANGLE's WebGPU backend

(This file is for our fork, ryanrasti/angle, only: it lives on the `wgpu-es3-local` branch and never goes upstream.)

## Why this exists

The project this serves is linux-workers (`/home/ryan/src/linux-workers`):
Linux compiled to wasm32, running as a JS host's guest on Node and in the
browser, with userspace cross-compiled from nixpkgs. Guest programs already
get the host's WebGPU: a C library implementing the standard `webgpu.h`
(webgpu-native header at a1dc774) whose calls are wasm imports to Dawn on
Node or `navigator.gpu` in the browser, waiting through a kernel device,
`/dev/webgpu` (branch `refactor/webgpu`, worktree
`/home/ryan/src/linux-workers/.claude/worktrees/webgpu`,
`exopkgs/exobox-webgpu/`).

The plan for OpenGL in the guest is ANGLE's WebGPU backend
(`src/libANGLE/renderer/wgpu`) on top of that layer: GLES + EGL on both
hosts. It fits: plain C `webgpu.h` through a `DawnProcTable` passed as an EGL
attribute (`EGL_PLATFORM_ANGLE_DAWN_PROC_TABLE_ANGLE`), every wait an
`instanceWaitAny(..., -1)`, no threads, no optional features. WebKit already
builds it without Dawn (WebKit PR #74812). But upstream calls it an
in-progress ES 2.0 backend, and the targets we care about (mutter/Cogl,
GTK4's GL renderer, SDL2 apps) need ES 3.

**The task here: write tests for the ES 3 functionality those consumers need,
then implement it in the WebGPU backend.** Native x86_64 Linux against Dawn
is the development loop; the wasm32 port is a separate, later step.

## Known gaps (from reading dbd678c22)

- The WGSL translator doesn't output user uniform blocks
  (`OutputUniformBlocks.cpp` TODO, anglebug 376553328).
- `gl_FragData`, and array-typed shader inputs and outputs: UNIMPLEMENTED.
- Transform feedback (`TransformFeedbackWgpu.cpp`) and queries
  (`QueryWgpu.cpp`) are stubs; queries return 0. Transform-feedback caps are 0.
- Indirect and multi-draw: UNIMPLEMENTED. Pixmap surfaces: UNIMPLEMENTED.
- The backend reports a max version of 3.2 (`wgpu_utils.cpp`) while these
  silently do nothing: keep that in mind when reading test results.
- dEQP-GLES2 expectations have 98 WGPU entries (many blanket FAILs);
  dEQP-GLES3 has none. ES3 end2end tests are enabled for only 6 files.

Transform feedback is low priority: WebGPU has no equivalent (it would need
emulation with compute), so implement it only if a consumer needs it, and
otherwise write down the design.

## Workflow

1. **The needs list.** Survey what mutter/Cogl, GTK4's GSK GL renderer and
   typical SDL2/GLES3 apps use from ES 3 (read their sources). Rank the gaps
   above by it. Write the list, with sources, to `out/needs.md` (`out/` is
   ignored).
2. **Tests first.** Prefer enabling ANGLE's existing end2end tests
   (UniformBufferTest, DrawBuffersTest, QueryTest, ...) and the relevant
   dEQP-GLES3 groups for the WGPU backend. Record a baseline pass/fail per
   feature before changing anything (`out/baseline.md`). Write new tests
   only where existing ones don't cover what the consumers do.
3. **Implement** feature by feature in rank order, rerunning that feature's
   tests and the ES2 suite (no regressions) each time. One commit per
   feature, in ANGLE's style (its commit message format, `git cl format`'s
   formatting), as if for upstream. Never push or upload to Gerrit.

## The checkout and build

- `/home/ryan/src/angle`: a shallow clone at upstream `dbd678c22`
  (2026-10-26), on local branch `wgpu-es3`. Dependencies were fetched by hand,
  shallow, at DEPS's revisions (no gclient), so `gclient sync` isn't
  available: fetch a missing dependency the same way, at its DEPS revision.
- Local stubs, not to be committed: `build/config/gclient_args.gni`
  (hand-written), `third_party/catapult/tracing/BUILD.gn` (perf-test data
  only).
- `source out/env.sh` first: nix's clang 23 laid out as Chromium's build
  expects (`out/llvm23`), gn, ninja, X11 headers and libraries, and the nix
  vulkan-loader on `LD_LIBRARY_PATH` (Dawn finds Vulkan only through it,
  set before the process starts).
- `out/es3/args.gn`: release with asserts, WebGPU and null backends, end2end
  and dEQP tests, Dawn on Vulkan through the system loader. `gn gen out/es3`
  works (950 targets). `out/wgpu` is an earlier backend-only config.
- Build: `ninja -C out/es3 -j8 angle_end2end_tests` (and
  `angle_deqp_gles3_tests`, `angle_deqp_gles2_tests`). The first build is long:
  run it in the background and check its log.
- Run the WebGPU backend's tests with a filter, e.g.
  `out/es3/angle_end2end_tests --gtest_filter='UniformBufferTest.*/ES3_WebGPU*'`.
  They open X11 windows: `DISPLAY=:0` is available. The machine has Intel
  (Xe-LPG, Mesa) and NVIDIA GPUs; note which one Dawn picked.

## Rules

- Keep iteration fast: short timeouts, one test filter at a time, fail fast.
  Long builds go in the background with progress checks, not long waits.
- The machine is shared with other builds: `-j8`, not more.
- Never `rm -rf *` or other glob deletes; delete only explicit paths.
- Don't touch `/home/ryan/src/linux-workers` (other work is in progress
  there); read it freely.
- Nix: `ryan-desktop` (32 cores, usually idle) is configured as a remote
  builder; `--max-jobs 0` sends a heavy `nix build` there. ANGLE's own
  ninja build is local.

## Done means

The needs list; per-feature baseline and current pass rates; one commit per
feature; what's left, and how far this is from what mutter and GTK4 need.

## Handoff: state as of 2026-10-27 (for a fresh agent)

### Commits so far (branch `wgpu-es3`, on top of dbd678c22, not pushed to Gerrit)

1. `WebGPU: Remove include of libc++'s internal __config header` (needed to build with libstdc++)
2. `WebGPU: Set MAX_COMBINED_SHADER_OUTPUT_RESOURCES`: it was 0, so every program
   with a fragment output failed to link on the 3.2 context. Biggest single win.
3. `WebGPU: Fix packing of vertex formats in the pipeline desc` (5 bits were too few)
4. `WebGPU: Handle renderbuffers with 0 samples`
5. `WebGPU: Support arrays, interpolation and gl_FragData in shader I/O`. Array
   varyings are split into `_elemN` members at their own locations. Also adds
   `@interpolate` for flat/centroid/etc., `gl_FragData`, and widens fragment outputs to vec4.
6. `WebGPU: Support uniform blocks`. UBOs live in bind group 3, with the binding
   filled in at link time from a `@binding(@@@@@@Name@@)` marker. Also adds the
   missing `linkResources()` call in ProgramWgpu, and gives all buffers a usage superset.
7. `WebGPU: Fix shifts by signed and scalar amounts`
8. `WebGPU: Implement buffer mapping, copies and ordered updates`. A per-context
   submission serial plus buffer marking makes writes to buffers used by
   unsubmitted commands go through a staging copy in the encoder. mapRange uses a
   CPU shadow copy. copySubData is implemented.
9. `WebGPU: Support texture swizzle with texture-component-swizzle`, an optional
   Dawn/WebGPU feature used only when the adapter has it. It also fixes
   luminance/alpha format sampling.

Each commit message has before/after numbers. `out/baseline.md` has the full
baseline vs. current table; `out/needs.md` has the consumer survey and ranking.

### Branches (fork: github.com/ryanrasti/angle, remote `github`)

- `wgpu-es3`: only the upstream-style commits above. New feature commits go
  here.
- `wgpu-es3-local`: `wgpu-es3` plus "LOCAL ONLY" commits at the tip, which hold
  this file, `handoff/` (copies of the `out/` docs and scripts) and the
  test-enable hack. Rebase it onto `wgpu-es3` after adding commits there. Never
  upstream it.

The test-enable hack instantiates the ES3 end2end suites for WebGPU:
- It adds `ES3_WEBGPU()` to `ANGLE_ALL_TEST_PLATFORMS_ES3` in
  angle_test_instantiate.h.
- It removes the explicit `ES3_WEBGPU()` from the files that already list it, to
  avoid duplicate test names: BlitFramebufferANGLETest, ClearTest,
  DepthStencilTest, LineLoopTest, MipmapTest and UniformBufferTest.

When a commit on `wgpu-es3` enables another suite for WebGPU upstream-style,
remove that explicit `ES3_WEBGPU()` again in the local commit.

Pushing: a plain `git push github ...` from this checkout stalls. It is a shallow,
partial clone, so `pack-objects` lazily fetches blobs from googlesource and
GitHub drops the idle connection. Push from a helper repo instead:
`git init`, then `git fetch --depth=1 github <base sha>` (a complete clone of the
base), then `git fetch /home/ryan/src/angle wgpu-es3:wgpu-es3
wgpu-es3-local:wgpu-es3-local`, then `git push github wgpu-es3 wgpu-es3-local`.
The commit hashes stay identical.

### Environment notes (out/ is gitignored; copies of its scripts and docs are in `handoff/`)

- `out/env.sh` (a copy is in `handoff/`) was extended:
  - libxcb, pciutils and gcc-lib paths added
  - `NIX_CC_WRAPPER_TARGET_HOST_x86_64_unknown_linux_gnu=1`, without which the
    nix clang wrapper ignores `NIX_CFLAGS_COMPILE`/`NIX_LDFLAGS`
- Dependencies fetched by hand at their DEPS revisions:
  - `third_party/re2/src`
  - `third_party/dawn/tools/golang/linux-amd64`: a symlink to nix go's
    `share/go`, used by Dawn's `generate-sources-gn.py`
- `gn gen` needs `--script-executable=$(which python3)`, because
  third_party/cpython3 is missing.
- Build directories:
  - `out/es3`: the baseline build. Leave it untouched.
  - `out/dev`: the working build.
  - `out/cur`: a snapshot used by the full "current" run.
- The user allows `ninja -j24`.
- **Never let tests open windows on the user's display.**
  - `out/runtests.sh <binary> <filter> <name> [args]` runs tests in bot mode on a
    private `Xvfb :97` with twm (started by `out/headless.sh`, auto-placement via
    `out/twmrc`).
  - It forces Mesa ANV with `MESA_VK_WSI_DEBUG=sw`; Xvfb has no DRI3, and NVIDIA
    can't present to it.
  - Set `OUT=dev` to pick a build dir and `PROCS=N` for parallelism.
  - It uses `--max-failures=1000000`. Without it, the runner stops early and
    writes no results JSON.
  - `out/summarize.py X.json [-v]` summarizes a run; `out/compare.py base.json
    new.json [-f]` lists regressions.
  - `out/baseline_all.sh <prefix>` runs every group in `out/deqp_groups.txt`.
- dEQP filters look like `dEQP-GLES3.functional.ubo.*`, plus `--use-angle=webgpu`.
  Run a single dEQP test from a scratch directory to get its own `TestResults.qpa`.
- Never `pkill -f <pattern>` or `pgrep -f` with a pattern that appears in your own
  command line: it kills or matches your own shell. Use `pgrep -x`, or explicit PIDs.

### Formatting and commit style

- `git cl` isn't available. Use nix `clang-tools`:
  `git clang-format --binary <clang-tools>/bin/clang-format -f HEAD -- src/...`.
  clang-format 21 instead of Chromium's; close enough.
- Commit messages: `WebGPU: <Title>`, a body, `Bug: angleproject:42267100` (or
  376553328 for UBOs), then a `Co-Authored-By` line. No Change-Id; there is no
  Gerrit hook.

### What's next (ranked for mutter/GTK)

1. **dEQP vertex_arrays/draw all crash**: the translator asserts in `visitSymbol`
   (TranslatorWGSL.cpp:368) on a builtin that isn't rewritten as pipeline I/O.
   This was being investigated: run
   `dEQP-GLES3.functional.vertex_arrays.multiple_attributes.attribute_count.2`.
   It affects many dEQP groups.
2. Integer vertex attributes from client-side arrays
   (`calculateStagingBufferSize` asserts `destTypeSize > 0`), and default integer
   attribute values. GTK uses VBOs, which already work.
3. Integer render targets read back wrong (dEQP fragment_out int/uint image diffs).
4. Report ES 3.0 instead of 3.2? Mutter and GTK both assume float render targets
   (and GTK blending on them) from 3.2 without checking extensions. ES 3.1/3.2
   features are stubbed (compute, SSBO, images, indirect).
5. Row-major matrices in UBOs (needs a WGSL-capable RewriteRowMajorMatrices; the
   existing one is Apple-GLSL only). Arrays of uniform blocks (split into
   per-element vars; GLSL requires constant indices). Together these are about
   1100 of the dEQP ubo failures.
6. Struct varyings crash (dEQP linkage.varying.struct.*). Interpolation must match
   across stages: a centroid mismatch fails WebGPU validation.
7. Compound assignment used as an expression (e.g. `a = (b <<= c)`) fails in the
   WGSL output.
8. Low priority, unused by the consumers: queries, transform feedback, indirect
   draws, pixmap surfaces. Also PBOs: `readPixels` into a pack buffer is
   UNREACHABLE.
