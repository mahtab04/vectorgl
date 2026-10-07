// ============================================================================
// gl_handle.hpp — RAII wrappers for OpenGL resource handles
// ============================================================================
// Internal header. Prevents resource leaks and double-frees by tying
// GL object lifetime to C++ object lifetime.
// ============================================================================

#pragma once

#include <glad/gl.h>

#include <cstdint>
#include <type_traits>
#include <utility>

namespace vectorgl::detail
{

// ============================================================================
// Generic RAII handle for a single GL object
// ============================================================================
// Tag types select the correct GL create/destroy functions at compile time.

struct GLBufferTag
{
};
struct GLVAOTag
{
};
struct GLTextureTag
{
};
struct GLFramebufferTag
{
};
struct GLProgramTag
{
};
struct GLShaderTag
{
};

template <typename Tag> class GLHandle
{
public:
    GLHandle() = default;
    ~GLHandle()
    {
        reset();
    }

    GLHandle(const GLHandle&) = delete;
    GLHandle& operator=(const GLHandle&) = delete;

    GLHandle(GLHandle&& other) noexcept : id_(other.id_)
    {
        other.id_ = 0;
    }
    GLHandle& operator=(GLHandle&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            id_ = other.id_;
            other.id_ = 0;
        }
        return *this;
    }

    /// Create a new GL object
    void create()
    {
        reset();
        if constexpr (std::is_same_v<Tag, GLBufferTag>)
        {
            glGenBuffers(1, &id_);
        }
        else if constexpr (std::is_same_v<Tag, GLVAOTag>)
        {
            glGenVertexArrays(1, &id_);
        }
        else if constexpr (std::is_same_v<Tag, GLTextureTag>)
        {
            glGenTextures(1, &id_);
        }
        else if constexpr (std::is_same_v<Tag, GLFramebufferTag>)
        {
            glGenFramebuffers(1, &id_);
        }
        // GLProgramTag: use createProgram() or adopt()
    }

    /// Destroy the owned GL object
    void reset()
    {
        if (id_ == 0)
            return;
        if constexpr (std::is_same_v<Tag, GLBufferTag>)
        {
            glDeleteBuffers(1, &id_);
        }
        else if constexpr (std::is_same_v<Tag, GLVAOTag>)
        {
            glDeleteVertexArrays(1, &id_);
        }
        else if constexpr (std::is_same_v<Tag, GLTextureTag>)
        {
            glDeleteTextures(1, &id_);
        }
        else if constexpr (std::is_same_v<Tag, GLFramebufferTag>)
        {
            glDeleteFramebuffers(1, &id_);
        }
        else if constexpr (std::is_same_v<Tag, GLProgramTag>)
        {
            glDeleteProgram(id_);
        }
        else if constexpr (std::is_same_v<Tag, GLShaderTag>)
        {
            glDeleteShader(id_);
        }
        id_ = 0;
    }

    /// Take ownership of an externally-created handle
    void adopt(uint32_t externalId)
    {
        reset();
        id_ = externalId;
    }

    /// Release ownership without destroying (caller takes over)
    uint32_t release()
    {
        uint32_t tmp = id_;
        id_ = 0;
        return tmp;
    }

    [[nodiscard]] uint32_t get() const
    {
        return id_;
    }
    [[nodiscard]] explicit operator bool() const
    {
        return id_ != 0;
    }
    [[nodiscard]] operator uint32_t() const
    {
        return id_;
    }

private:
    uint32_t id_ = 0;
};

// Convenient type aliases
using GLBuffer = GLHandle<GLBufferTag>;
using GLVAO = GLHandle<GLVAOTag>;
using GLTexture = GLHandle<GLTextureTag>;
using GLFramebuffer = GLHandle<GLFramebufferTag>;
using GLProgram = GLHandle<GLProgramTag>;
using GLShader = GLHandle<GLShaderTag>;

} // namespace vectorgl::detail
