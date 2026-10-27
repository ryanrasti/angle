//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// BufferWgpu.cpp:
//    Implements the class methods for BufferWgpu.
//

#include "libANGLE/renderer/wgpu/BufferWgpu.h"
#include "common/unsafe_buffers.h"

#include "common/debug.h"
#include "common/mathutil.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{
namespace
{
// Based on a buffer binding target, compute the default wgpu usage flags. More can be added if the
// buffer is used in new ways.
WGPUBufferUsage GetDefaultWGPUBufferUsageForBinding(gl::BufferBinding binding)
{
    switch (binding)
    {
        // A GL buffer can be bound to any target after it is created, so buffers created through
        // these targets get all the usages that these targets need. Mappable WebGPU buffers can't
        // have any other usage than copies, so pixel pack/unpack buffers aren't mappable either;
        // reading and writing them goes through staging buffers like for other buffers.
        case gl::BufferBinding::Array:
        case gl::BufferBinding::ElementArray:
        case gl::BufferBinding::Uniform:
        case gl::BufferBinding::CopyRead:
        case gl::BufferBinding::CopyWrite:
        case gl::BufferBinding::PixelPack:
        case gl::BufferBinding::PixelUnpack:
            return WGPUBufferUsage_Vertex | WGPUBufferUsage_Index | WGPUBufferUsage_Uniform |
                   WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst;

        case gl::BufferBinding::ShaderStorage:
        case gl::BufferBinding::Texture:
        case gl::BufferBinding::TransformFeedback:
        case gl::BufferBinding::DispatchIndirect:
        case gl::BufferBinding::DrawIndirect:
        case gl::BufferBinding::AtomicCounter:
            UNIMPLEMENTED();
            return WGPUBufferUsage_None;

        default:
            UNREACHABLE();
            return WGPUBufferUsage_None;
    }
}

}  // namespace

BufferWgpu::BufferWgpu(const gl::BufferState &state) : BufferImpl(state) {}

BufferWgpu::~BufferWgpu() {}

angle::Result BufferWgpu::setData(const gl::Context *context,
                                  gl::BufferBinding target,
                                  const void *data,
                                  size_t size,
                                  gl::BufferUsage usage,
                                  BufferFeedback *feedback,
                                  gl::ZeroFillRequired zeroFillRequired)
{
    ContextWgpu *contextWgpu = webgpu::GetImpl(context);
    const DawnProcTable *wgpu   = webgpu::GetProcs(contextWgpu);
    webgpu::DeviceHandle device = webgpu::GetDevice(context);

    const void *dataForImpl = data;
    if (zeroFillRequired == gl::ZeroFillRequired::Yes)
    {
        const angle::MemoryBuffer *scratchBuffer = nullptr;
        ANGLE_CHECK_GL_ALLOC(
            contextWgpu, context->getZeroFilledBuffer(static_cast<size_t>(size), &scratchBuffer));
        dataForImpl = scratchBuffer->data();
    }

    bool hasData = dataForImpl && size > 0;

    // Allocate a new buffer if the current one is invalid, the size is different, or the current
    // buffer cannot be mapped for writing when data needs to be uploaded.
    if (!mBuffer.valid() || mBuffer.requestedSize() != size ||
        (hasData && !mBuffer.canMapForWrite()))
    {
        // Allocate a new buffer
        ANGLE_TRY(mBuffer.initBuffer(wgpu, device, size,
                                     GetDefaultWGPUBufferUsageForBinding(target),
                                     webgpu::MapAtCreation::Yes));
    }

    if (hasData)
    {
        ASSERT(mBuffer.canMapForWrite());

        if (!mBuffer.getMappedState().has_value())
        {
            ANGLE_TRY(mBuffer.mapImmediate(contextWgpu, WGPUMapMode_Write, 0, size));
        }

        uint8_t *mappedData = mBuffer.getMapWritePointer(0, size);
        ANGLE_UNSAFE_TODO(memcpy(mappedData, dataForImpl, size));
    }

    return angle::Result::Continue;
}

angle::Result BufferWgpu::setSubData(const gl::Context *context,
                                     gl::BufferBinding target,
                                     const void *data,
                                     size_t size,
                                     size_t offset,
                                     BufferFeedback *feedback)
{
    ASSERT(mBuffer.valid());
    return writeData(webgpu::GetImpl(context), static_cast<const uint8_t *>(data), offset, size);
}

angle::Result BufferWgpu::copySubData(const gl::Context *context,
                                      BufferImpl *source,
                                      GLintptr sourceOffset,
                                      GLintptr destOffset,
                                      GLsizeiptr size,
                                      BufferFeedback *feedback)
{
    ContextWgpu *contextWgpu = webgpu::GetImpl(context);
    BufferWgpu *sourceWgpu   = GetAs<BufferWgpu>(source);
    if (size == 0)
    {
        return angle::Result::Continue;
    }

    // WebGPU can copy between different buffers with 4-byte aligned offsets and sizes. Otherwise
    // go through the CPU.
    const size_t kAlignment = webgpu::kBufferCopyToBufferAlignment;
    if (sourceWgpu == this || sourceOffset % kAlignment != 0 || destOffset % kAlignment != 0 ||
        size % kAlignment != 0)
    {
        std::vector<uint8_t> data(size);
        ANGLE_TRY(sourceWgpu->readData(contextWgpu, sourceOffset, size, data.data()));
        return writeData(contextWgpu, data.data(), destOffset, size);
    }

    webgpu::BufferHelper &sourceBuffer = sourceWgpu->getBuffer();
    if (sourceBuffer.getMappedState())
    {
        ANGLE_TRY(sourceBuffer.unmap());
    }
    if (mBuffer.getMappedState())
    {
        ANGLE_TRY(mBuffer.unmap());
    }

    webgpu::CommandEncoderHandle commandEncoder;
    ANGLE_TRY(contextWgpu->getCurrentCommandEncoder(webgpu::RenderPassClosureReason::BufferCopy,
                                                    &commandEncoder));
    const DawnProcTable *wgpu = webgpu::GetProcs(contextWgpu);
    wgpu->commandEncoderCopyBufferToBuffer(commandEncoder.get(), sourceBuffer.getBuffer().get(),
                                           sourceOffset, mBuffer.getBuffer().get(), destOffset,
                                           size);
    sourceBuffer.markUsedByPendingCommands(contextWgpu->getPendingCommandsSerial());
    mBuffer.markUsedByPendingCommands(contextWgpu->getPendingCommandsSerial());

    return angle::Result::Continue;
}

angle::Result BufferWgpu::map(const gl::Context *context,
                              GLenum access,
                              void **mapPtr,
                              BufferFeedback *feedback)
{
    // glMapBufferOES only supports GL_WRITE_ONLY_OES, which preserves the buffer contents.
    ASSERT(access == GL_WRITE_ONLY_OES);
    return mapRange(context, 0, static_cast<size_t>(mState.getSize()), GL_MAP_WRITE_BIT, mapPtr,
                    feedback);
}

angle::Result BufferWgpu::mapRange(const gl::Context *context,
                                   size_t offset,
                                   size_t length,
                                   GLbitfield access,
                                   void **mapPtr,
                                   BufferFeedback *feedback)
{
    ContextWgpu *contextWgpu = webgpu::GetImpl(context);

    // WebGPU buffers can only be mapped when the GPU isn't using them, and mappable buffers can't
    // be used for anything but copies, so the application gets a CPU copy of the range instead.
    // It needs the current contents unless they are invalidated and not read.
    ANGLE_CHECK_GL_ALLOC(contextWgpu, mMapShadow.resize(length));
    const bool needsContents =
        (access & GL_MAP_READ_BIT) != 0 ||
        (access & (GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)) == 0;
    if (needsContents && length > 0)
    {
        ANGLE_TRY(readData(contextWgpu, offset, length, mMapShadow.data()));
    }

    mMapOffset = offset;
    mMapAccess = access;
    *mapPtr    = mMapShadow.data();
    return angle::Result::Continue;
}

angle::Result BufferWgpu::unmap(const gl::Context *context,
                                GLboolean *result,
                                BufferFeedback *feedback)
{
    // With GL_MAP_FLUSH_EXPLICIT_BIT, the parts of the range that were not flushed are undefined,
    // so writing back the whole range is fine.
    if ((mMapAccess & GL_MAP_WRITE_BIT) != 0 && mMapShadow.size() > 0)
    {
        ANGLE_TRY(
            writeData(webgpu::GetImpl(context), mMapShadow.data(), mMapOffset, mMapShadow.size()));
    }
    mMapAccess = 0;
    *result    = GL_TRUE;
    return angle::Result::Continue;
}

angle::Result BufferWgpu::writeData(ContextWgpu *contextWgpu,
                                    const uint8_t *data,
                                    size_t offset,
                                    size_t size)
{
    if (size == 0)
    {
        return angle::Result::Continue;
    }

    if (mBuffer.isMappedForWrite())
    {
        // The buffer is still mapped at creation, so the GPU hasn't used it yet.
        uint8_t *mappedData = mBuffer.getMapWritePointer(offset, size);
        ANGLE_UNSAFE_TODO(memcpy(mappedData, data, size));
        return angle::Result::Continue;
    }

    // WebGPU requires buffer writes to be 4-byte aligned. Extend unaligned writes with the
    // current contents of the buffer.
    const size_t kAlignment   = webgpu::kBufferCopyToBufferAlignment;
    const size_t alignedStart = rx::roundDownPow2(offset, kAlignment);
    const size_t alignedEnd   = rx::roundUpPow2(offset + size, kAlignment);
    const size_t alignedSize  = alignedEnd - alignedStart;
    ASSERT(alignedEnd <= mBuffer.actualSize());

    std::vector<uint8_t> alignedData;
    if (alignedStart != offset || alignedSize != size)
    {
        alignedData.resize(alignedSize);
        ANGLE_TRY(readData(contextWgpu, alignedStart, alignedSize, alignedData.data()));
        ANGLE_UNSAFE_TODO(memcpy(alignedData.data() + (offset - alignedStart), data, size));
        data = alignedData.data();
    }

    const DawnProcTable *wgpu = webgpu::GetProcs(contextWgpu);
    if (!mBuffer.isUsedByPendingCommands(contextWgpu->getPendingCommandsSerial()))
    {
        // No recorded command uses the buffer, so writing it from the queue, which happens before
        // the recorded commands are submitted, is fine.
        wgpu->queueWriteBuffer(contextWgpu->getQueue().get(), mBuffer.getBuffer().get(),
                               alignedStart, data, alignedSize);
        return angle::Result::Continue;
    }

    // Otherwise the write must be ordered after the recorded commands, so upload the data to a
    // staging buffer and copy it in the command encoder.
    webgpu::BufferHelper stagingBuffer;
    ANGLE_TRY(stagingBuffer.initBuffer(wgpu, contextWgpu->getDevice(), alignedSize,
                                       WGPUBufferUsage_CopySrc, webgpu::MapAtCreation::Yes));
    ANGLE_UNSAFE_TODO(memcpy(stagingBuffer.getMapWritePointer(0, alignedSize), data, alignedSize));
    ANGLE_TRY(stagingBuffer.unmap());

    webgpu::CommandEncoderHandle commandEncoder;
    ANGLE_TRY(contextWgpu->getCurrentCommandEncoder(webgpu::RenderPassClosureReason::BufferUpdate,
                                                    &commandEncoder));
    wgpu->commandEncoderCopyBufferToBuffer(commandEncoder.get(), stagingBuffer.getBuffer().get(), 0,
                                           mBuffer.getBuffer().get(), alignedStart, alignedSize);

    return angle::Result::Continue;
}

angle::Result BufferWgpu::readData(ContextWgpu *contextWgpu,
                                   size_t offset,
                                   size_t size,
                                   uint8_t *dataOut)
{
    if (mBuffer.isMappedForRead())
    {
        // The buffer is still mapped at creation, so the GPU hasn't used it yet.
        ANGLE_UNSAFE_TODO(memcpy(dataOut, mBuffer.getMapReadPointer(offset, size), size));
        return angle::Result::Continue;
    }

    webgpu::BufferReadback readback;
    ANGLE_TRY(mBuffer.readDataImmediate(
        contextWgpu, offset, size, webgpu::RenderPassClosureReason::BufferReadback, &readback));
    ANGLE_UNSAFE_TODO(memcpy(dataOut, readback.data, size));
    return angle::Result::Continue;
}

angle::Result BufferWgpu::getIndexRange(const gl::Context *context,
                                        gl::DrawElementsType type,
                                        size_t offset,
                                        size_t count,
                                        bool primitiveRestartEnabled,
                                        gl::IndexRange *outRange)
{
    ContextWgpu *contextWgpu = webgpu::GetImpl(context);

    const GLuint typeBytes = gl::GetDrawElementsTypeSize(type);

    webgpu::BufferReadback readback;
    ANGLE_TRY(mBuffer.readDataImmediate(contextWgpu, offset, count * typeBytes,
                                        webgpu::RenderPassClosureReason::IndexRangeReadback,
                                        &readback));
    *outRange = gl::ComputeIndexRange(type, readback.data, count, primitiveRestartEnabled);

    return angle::Result::Continue;
}

}  // namespace rx
