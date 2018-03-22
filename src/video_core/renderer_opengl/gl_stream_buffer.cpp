// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <deque>
#include <vector>
#include "common/alignment.h"
#include "common/assert.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_stream_buffer.h"

OGLStreamBuffer::OGLStreamBuffer(GLenum target, GLsizeiptr size, bool coherent) {
    gl_target = target;
    buffer_size = size;
    map_flags = GL_MAP_WRITE_BIT;

    gl_buffer.Create();

    glBindBuffer(gl_target, gl_buffer.handle);

    if (GLAD_GL_ARB_buffer_storage) {
        map_flags |= GL_MAP_PERSISTENT_BIT;
        if (coherent) {
            map_flags |= GL_MAP_COHERENT_BIT;
        }
        glBufferStorage(gl_target, buffer_size, nullptr, map_flags);

        if (!coherent) {
            map_flags |= GL_MAP_FLUSH_EXPLICIT_BIT;
        }
        mapped_ptr = reinterpret_cast<u8*>(glMapBufferRange(gl_target, 0, buffer_size, map_flags));
    } else {
        map_flags |= GL_MAP_FLUSH_EXPLICIT_BIT;
        glBufferData(gl_target, buffer_size, nullptr, GL_STREAM_DRAW);
    }
}

OGLStreamBuffer::~OGLStreamBuffer() {
    if (map_flags & GL_MAP_PERSISTENT_BIT) {
        glBindBuffer(gl_target, gl_buffer.handle);
        glUnmapBuffer(gl_target);
    }
    gl_buffer.Release();
}

GLuint OGLStreamBuffer::GetHandle() const {
    return gl_buffer.handle;
}

GLsizeiptr OGLStreamBuffer::GetSize() const {
    return buffer_size;
}

std::tuple<u8*, GLintptr, bool> OGLStreamBuffer::Map(GLsizeiptr size, GLintptr alignment) {
    ASSERT(size <= buffer_size);
    ASSERT(alignment <= buffer_size);
    mapped_size = size;

    if (alignment > 0) {
        buffer_pos = Common::AlignUp<size_t>(buffer_pos, alignment);
    }

    GLbitfield flags = map_flags;
    if (buffer_pos + size > buffer_size) {
        buffer_pos = 0;
        flags |= GL_MAP_INVALIDATE_BUFFER_BIT;

        if (flags & GL_MAP_PERSISTENT_BIT) {
            glUnmapBuffer(gl_target);
        }
    } else if (!(flags & GL_MAP_PERSISTENT_BIT)) {
        flags |= GL_MAP_UNSYNCHRONIZED_BIT;
    }

    if (flags & GL_MAP_INVALIDATE_BUFFER_BIT || !(flags & GL_MAP_PERSISTENT_BIT)) {
        mapped_ptr = reinterpret_cast<u8*>(
            glMapBufferRange(gl_target, buffer_pos, buffer_size - buffer_pos, flags));
        mapped_offset = buffer_pos;
    }

    return std::make_tuple(mapped_ptr + buffer_pos - mapped_offset, buffer_pos,
                           flags & GL_MAP_INVALIDATE_BUFFER_BIT);
}

void OGLStreamBuffer::Unmap(GLsizeiptr size) {
    ASSERT(size <= mapped_size);

    if (!(map_flags & GL_MAP_COHERENT_BIT)) {
        glFlushMappedBufferRange(gl_target, buffer_pos - mapped_offset, size);
    }

    if (!(map_flags & GL_MAP_PERSISTENT_BIT)) {
        glUnmapBuffer(gl_target);
    }

    buffer_pos += size;
}
