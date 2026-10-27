//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferWgpu.h:
//    Defines the class interface for BufferWgpu, implementing BufferImpl.
//

#ifndef LIBANGLE_RENDERER_WGPU_BUFFERWGPU_H_
#define LIBANGLE_RENDERER_WGPU_BUFFERWGPU_H_

#include "common/MemoryBuffer.h"
#include "libANGLE/renderer/BufferImpl.h"

#include "libANGLE/renderer/wgpu/wgpu_helpers.h"

#include <webgpu/webgpu.h>

namespace rx
{

class BufferWgpu : public BufferImpl
{
  public:
    BufferWgpu(const gl::BufferState &state);
    ~BufferWgpu() override;

    angle::Result setData(const gl::Context *context,
                          gl::BufferBinding target,
                          const void *data,
                          size_t size,
                          gl::BufferUsage usage,
                          BufferFeedback *feedback,
                          gl::ZeroFillRequired zeroFillRequired) override;
    angle::Result setSubData(const gl::Context *context,
                             gl::BufferBinding target,
                             const void *data,
                             size_t size,
                             size_t offset,
                             BufferFeedback *feedback) override;
    angle::Result copySubData(const gl::Context *context,
                              BufferImpl *source,
                              GLintptr sourceOffset,
                              GLintptr destOffset,
                              GLsizeiptr size,
                              BufferFeedback *feedback) override;
    angle::Result map(const gl::Context *context,
                      GLenum access,
                      void **mapPtr,
                      BufferFeedback *feedback) override;
    angle::Result mapRange(const gl::Context *context,
                           size_t offset,
                           size_t length,
                           GLbitfield access,
                           void **mapPtr,
                           BufferFeedback *feedback) override;
    angle::Result unmap(const gl::Context *context,
                        GLboolean *result,
                        BufferFeedback *feedback) override;

    angle::Result getIndexRange(const gl::Context *context,
                                gl::DrawElementsType type,
                                size_t offset,
                                size_t count,
                                bool primitiveRestartEnabled,
                                gl::IndexRange *outRange) override;

    webgpu::BufferHelper &getBuffer() { return mBuffer; }

  private:
    // Writes `size` bytes of `data` at `offset`, ordered after the commands already recorded that
    // use the buffer.
    angle::Result writeData(ContextWgpu *contextWgpu,
                            const uint8_t *data,
                            size_t offset,
                            size_t size);
    // Reads `size` bytes at `offset` into `dataOut`, waiting for the commands that use the buffer.
    angle::Result readData(ContextWgpu *contextWgpu, size_t offset, size_t size, uint8_t *dataOut);

    webgpu::BufferHelper mBuffer;

    // While the buffer is mapped with glMapBufferRange, the application reads and writes this CPU
    // copy of the mapped range, which is written back to the buffer when it is unmapped.
    angle::MemoryBuffer mMapShadow;
    size_t mMapOffset     = 0;
    GLbitfield mMapAccess = 0;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_BUFFERWGPU_H_
