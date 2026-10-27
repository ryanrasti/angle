# ES 3 needs of mutter/Cogl, GTK4 and SDL, ranked for ANGLE's WebGPU backend

Sources: mutter 50.4, GTK 4.22.4, SDL 3.4.16 + sdl2-compat 2.32.72 (nixpkgs
sources). Paths are relative to each source root. "Inferred" marks claims from
common practice rather than read source.

## Context-version facts that shape everything

- ANGLE gives a 3.2 context even when ES 2 is requested
  (`EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE` defaults true,
  `src/libANGLE/Context.cpp`), and the wgpu backend's max is 3.2
  (`wgpu_utils.cpp:421`). So:
  - Cogl (asks for ES 2, GLSL ES 1.00 only) turns on its ES 3 paths: blit,
    sampler objects, glMapBufferRange, RG/RGB10_A2, and at >= 3.2 FP16/FP32
    render targets *without* checking extensions
    (`cogl/cogl/driver/gl/gles2/cogl-driver-gles2.c:802-897`).
  - GTK compiles `#version 320 es` and assumes RGBA16F/32F renderable and
    blendable from the version alone (`gdk/gdkglcontext.c:1715-1724`).
- GTK needs ES >= 3.0 for its GL renderer (`gsk/gpu/gskgldevice.c:254-260`);
  otherwise falls back to Cairo. Cogl works on ES 2. SDL's 2D renderer is pure
  ES 2 (`src/render/opengles2/SDL_render_gles2.c`).

## Ranking

| # | Feature | mutter/Cogl | GTK4 GSK | SDL renderer | typical GLES3 apps (inferred) |
|---|---|---|---|---|---|
| 1 | **Array-typed varyings** (and fragment output arrays) | **hard**: `varying vec4 _cogl_tex_coord[N]` in every textured pipeline (`cogl/cogl/driver/gl/cogl-pipeline-vertend-glsl.c:285`) | **hard**: `#define RoundedRect vec4[3]` / `Rect vec4[1]` passed as flat varyings in all 21 shaders (`gsk/gpu/shaders/roundedrect.glsl:7`, `rect.glsl:6`, `gskgpuborder.glsl:17`) | - | common |
| 2 | **Uniform blocks (UBOs)** | - | **hard**: `layout(std140) uniform PushConstants { mat4; mat3x4; vec2; }` (`gsk/gpu/shaders/common-gl.glsl:3-13`), `glBindBufferRange(GL_UNIFORM_BUFFER, 0, ...)` (`gsk/gpu/gskglbuffer.c:66-76`) | - | very common |
| 3 | **Buffer objects done right**: mapRange, copySubData, usage independent of first target, correct ordering of BufferSubData vs. recorded draws | used at ES3: glMapBufferRange read+write (`cogl/cogl/driver/gl/cogl-buffer-impl-gl.c:258-299`, journal `cogl-journal.c:643`) | glBufferSubData of UBO/vertex data per frame (`gskglbuffer.c`); mapRange only with EXT_buffer_storage | - | very common |
| 4 | Other GTK hard requirements (check, not known gaps): instancing + divisor, `glVertexAttribIPointer`, flat int/uint varyings, `gl_VertexID`, sampler objects, VAOs, `glFenceSync` + zero-timeout `glClientWaitSync` poll + `glWaitSync` | sampler objects at ES3 (`cogl-pipeline-gl.c:493`) | **hard** (`gskgpushaderop.c:228`, `generate-glsl-interface.py:75-95,576`, `common-gl.glsl:20`, `gskglframe.c:46-107`) | - | common |
| 5 | glBlitFramebuffer, texture swizzle, sRGB textures, RGBA16F/32F render+blend, external textures (samplerExternalOES + texelFetch/textureSize) | blit (`cogl-framebuffer.c:1539`), FP16 at 3.2 | blit default (`gskgpublitop.c:173`), swizzle (`gskglimage.c:261-273`), SRGB8_A8, float RTs, external for dma-buf (`common-gl.glsl:65-87`) | external optional | common |
| 6 | `gl_FragData` / MRT | - (`glDrawBuffers(1,{GL_BACK})` only) | - (single output; dual-source only with EXT_blend_func_extended) | - | common |
| 7 | Queries (occlusion, timer) | - (`COGL_FEATURE_ID_TIMESTAMP_QUERY` declared, never used) | - | - | optional (profilers) |
| 8 | Indirect / multi-draw | - | - | - | rare (ES 3.1) |
| 9 | Transform feedback | - | - | - | rare |
| 10 | Pixmap surfaces | - (no eglCreatePixmapSurface) | - | - (window + pbuffer only, `src/video/SDL_egl.c:470,1372`) | essentially never |

Notes:
- 1 and 6 share machinery: splitting an array in/out into N `@location`s gives
  `gl_FragData` almost for free.
- 3 is not in AGENTS.md's gap list but found by reading `BufferWgpu.cpp`:
  `map`/`mapRange`/`copySubData` are no-ops, WebGPU usage is fixed by the first
  bind target, and `setSubData` on a non-mappable buffer uses
  `queueWriteBuffer`, which lands before already-recorded-but-unsubmitted draws.
- Cogl's GLSL is ES 1.00 so it needs no ES 3 shader features beyond what
  ES 2 needs, except the array varyings.
- Missing from the backend's extension list and relevant: BGRA8888 (GTK then
  uses swizzle), EXT_color_buffer_(half_)float, EGL dma-buf import. Reporting
  ES 3.0 rather than 3.2 would make both consumers check extensions instead of
  assuming.
- None of the three consumers uses transform feedback, queries, indirect draws
  or pixmap surfaces.
