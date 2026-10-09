#include "vectorgl/renderer.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "vectorgl/detail/gl_handle.hpp"
#include "vectorgl/detail/shader_utils.hpp"
#include "vectorgl/font.hpp"
#include "vectorgl/image.hpp"
#include "vectorgl/paint.hpp"

#ifndef VECTORGL_SHADER_DIR
#define VECTORGL_SHADER_DIR "."
#endif

namespace vectorgl
{

namespace
{

struct TransformComponents
{
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float rotation = 0.0f;
};

TransformComponents decomposeTransform(const Mat3x3& transform)
{
    TransformComponents result;
    result.scaleX = std::hypot(transform.m[0], transform.m[1]);
    result.scaleY = std::hypot(transform.m[3], transform.m[4]);
    result.rotation = std::atan2(transform.m[1], transform.m[0]);
    return result;
}

struct SDFInstance
{
    float posX, posY;
    float sizeX, sizeY;
    std::array<float, 4> cornerRadii{};
    float fillR, fillG, fillB, fillA;
    float strokeR, strokeG, strokeB, strokeA;
    float strokeWidth;
    float rotation;
    float opacity;
    float shapeType;
};

uint32_t buildShaderProgram(const char* vertexFile, const char* fragmentFile, const char* label)
{
    const std::string vertSource = detail::loadShaderSource(VECTORGL_SHADER_DIR, vertexFile);
    const std::string fragSource = detail::loadShaderSource(VECTORGL_SHADER_DIR, fragmentFile);
    return detail::buildShaderProgramFromSource(vertSource, fragSource, vertexFile, fragmentFile, label);
}

std::vector<Vec2> triangulatePolygon(const std::vector<Vec2>& polygon)
{
    std::vector<Vec2> result;
    if (polygon.size() < 3)
        return result;

    std::vector<Vec2> pts(polygon.begin(), polygon.end());
    if (pts.size() > 1 && std::abs(pts.front().x - pts.back().x) < 1e-5f &&
        std::abs(pts.front().y - pts.back().y) < 1e-5f)
    {
        pts.pop_back();
    }
    if (pts.size() < 3)
        return result;

    float area = 0;
    for (size_t i = 0; i < pts.size(); ++i)
    {
        const auto& a = pts[i];
        const auto& b = pts[(i + 1) % pts.size()];
        area += (b.x - a.x) * (b.y + a.y);
    }
    if (area > 0)
        std::reverse(pts.begin(), pts.end());

    std::vector<int> indices(pts.size());
    for (size_t i = 0; i < pts.size(); ++i)
    {
        indices[i] = static_cast<int>(i);
    }

    auto cross = [](Vec2 o, Vec2 a, Vec2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };
    auto pointInTriangle = [&](Vec2 p, Vec2 a, Vec2 b, Vec2 c)
    {
        float d1 = cross(p, a, b);
        float d2 = cross(p, b, c);
        float d3 = cross(p, c, a);
        return !((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0));
    };

    int n = static_cast<int>(indices.size());
    int failCount = 0;
    int current = 0;
    while (n > 2 && failCount < n)
    {
        int prev = (current - 1 + n) % n;
        int next = (current + 1) % n;
        Vec2 a = pts[indices[prev]];
        Vec2 b = pts[indices[current]];
        Vec2 c = pts[indices[next]];
        if (cross(a, b, c) > 0)
        {
            bool ear = true;
            for (int j = 0; j < n; ++j)
            {
                if (j == prev || j == current || j == next)
                    continue;
                if (pointInTriangle(pts[indices[j]], a, b, c))
                {
                    ear = false;
                    break;
                }
            }
            if (ear)
            {
                result.push_back(a);
                result.push_back(b);
                result.push_back(c);
                indices.erase(indices.begin() + current);
                --n;
                failCount = 0;
                if (current >= n)
                    current = 0;
                continue;
            }
        }
        ++failCount;
        current = (current + 1) % n;
    }
    return result;
}

void configureEffectTexture(uint32_t texture, int width, int height)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void validateFramebuffer(const char* label)
{
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        throw std::runtime_error(std::string(label) + " framebuffer is incomplete");
    }
}

#ifndef NDEBUG
const char* debugSeverityName(GLenum severity)
{
    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:
        return "high";
    case GL_DEBUG_SEVERITY_MEDIUM:
        return "medium";
    case GL_DEBUG_SEVERITY_LOW:
        return "low";
    default:
        return "notification";
    }
}

void GLAD_API_PTR openGLDebugCallback(GLenum, GLenum type, GLuint id, GLenum severity, GLsizei, const GLchar* message,
                                      const void*)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
        return;

    std::cerr << "[VectorGL/OpenGL] severity=" << debugSeverityName(severity) << " type=0x" << std::hex << type
              << " id=" << std::dec << id << ": " << (message ? message : "(no message)") << '\n';
}

void enableOpenGLDebugOutput()
{
    if (glad_glDebugMessageCallback == nullptr)
        return;

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(openGLDebugCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
}
#endif

} // namespace

class Renderer::Impl
{
public:
    detail::GLProgram sdfProgram_;
    detail::GLProgram pathProgram_;
    detail::GLProgram texturedProgram_;
    detail::GLProgram blurProgram_;

    detail::GLVAO sdfVAO_;
    detail::GLBuffer sdfVBO_;
    detail::GLBuffer sdfInstanceVBO_;
    std::vector<SDFInstance> sdfBatch_;
    std::vector<float> glyphBatch_;
    uint32_t glyphTexture_ = 0;
    bool glyphSdf_ = true;
    std::shared_ptr<Font> glyphFontOwner_;

    detail::GLVAO pathVAO_;
    detail::GLBuffer pathVBO_;

    detail::GLVAO texVAO_;
    detail::GLBuffer texVBO_;
    detail::GLVAO blurVAO_;
    detail::GLBuffer blurVBO_;

    detail::GLFramebuffer effectFBO_;
    detail::GLTexture effectTexture_;
    detail::GLFramebuffer effectFBO2_;
    detail::GLTexture effectTexture2_;
    int effectW_ = 0;
    int effectH_ = 0;
    bool effectCaptured_ = false;
    bool initialized_ = false;
    bool frameActive_ = false;
    bool effectPassActive_ = false;

    int fbWidth_ = 0;
    int fbHeight_ = 0;
    int clipX_ = 0;
    int clipY_ = 0;
    int clipWidth_ = 0;
    int clipHeight_ = 0;
    bool clipEnabled_ = false;
    std::vector<Renderer::RoundedClip> roundedClips_;

    int32_t sdfLoc_viewSize_ = -1;
    int32_t pathLoc_viewSize_ = -1;
    int32_t pathLoc_paintType_ = -1;
    int32_t pathLoc_gradStart_ = -1;
    int32_t pathLoc_gradEnd_ = -1;
    int32_t pathLoc_gradInnerR_ = -1;
    int32_t pathLoc_gradOuterR_ = -1;
    int32_t pathLoc_gradStopCount_ = -1;
    int32_t pathLoc_gradPositions_ = -1;
    int32_t pathLoc_gradColors_ = -1;
    int32_t texLoc_viewSize_ = -1;
    int32_t texLoc_texture_ = -1;
    int32_t texLoc_sdf_ = -1;
    int32_t texLoc_effect_ = -1;
    int32_t blurLoc_radius_ = -1;
    int32_t blurLoc_direction_ = -1;
    int32_t blurLoc_texture_ = -1;

    void requireInitialized(const char* operation) const
    {
        if (!initialized_)
            throw std::logic_error(std::string("Renderer::") + operation + " requires init() first");
    }

    void requireFrame(const char* operation) const
    {
        requireInitialized(operation);
        if (!frameActive_)
            throw std::logic_error(std::string("Renderer::") + operation +
                                   " must be called between beginFrame() and endFrame()");
    }

    void applyClip() const
    {
        if (!clipEnabled_)
            glDisable(GL_SCISSOR_TEST);
        else
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(clipX_, fbHeight_ - (clipY_ + clipHeight_), clipWidth_, clipHeight_);
        }

        if (roundedClips_.empty())
            glDisable(GL_STENCIL_TEST);
        else
        {
            glEnable(GL_STENCIL_TEST);
            glStencilMask(0x00);
            glStencilFunc(GL_EQUAL, static_cast<GLint>(roundedClips_.size()), 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        }
    }

    void initSDF()
    {
        sdfProgram_.adopt(buildShaderProgram("sdf.vert", "sdf.frag", "sdf"));

        const float quadVerts[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};

        sdfVAO_.create();
        sdfVBO_.create();
        sdfInstanceVBO_.create();

        glBindVertexArray(sdfVAO_);

        glBindBuffer(GL_ARRAY_BUFFER, sdfVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

        glBindBuffer(GL_ARRAY_BUFFER, sdfInstanceVBO_);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW);

        const GLsizei stride = static_cast<GLsizei>(sizeof(SDFInstance));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SDFInstance, posX)));
        glVertexAttribDivisor(1, 1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SDFInstance, sizeX)));
        glVertexAttribDivisor(2, 1);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(offsetof(SDFInstance, cornerRadii)));
        glVertexAttribDivisor(3, 1);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SDFInstance, fillR)));
        glVertexAttribDivisor(4, 1);
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(offsetof(SDFInstance, strokeR)));
        glVertexAttribDivisor(5, 1);
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void*>(offsetof(SDFInstance, strokeWidth)));
        glVertexAttribDivisor(6, 1);

        glBindVertexArray(0);
    }

    void initPath()
    {
        pathProgram_.adopt(buildShaderProgram("path.vert", "path.frag", "path"));

        pathVAO_.create();
        pathVBO_.create();
        glBindVertexArray(pathVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, pathVBO_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
        glBindVertexArray(0);
    }

    void initTextured()
    {
        texturedProgram_.adopt(buildShaderProgram("textured.vert", "textured.frag", "textured"));

        texVAO_.create();
        texVBO_.create();
        glBindVertexArray(texVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, texVBO_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(4 * sizeof(float)));
        glBindVertexArray(0);
    }

    void initEffects()
    {
        blurProgram_.adopt(buildShaderProgram("blur.vert", "blur.frag", "blur"));
        blurVAO_.create();
        blurVBO_.create();
        glBindVertexArray(blurVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, blurVBO_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    void ensureEffectFBOs(int width, int height)
    {
        if (effectW_ == width && effectH_ == height && effectFBO_)
            return;

        effectW_ = width;
        effectH_ = height;

        effectFBO_.create();
        effectTexture_.create();
        configureEffectTexture(effectTexture_, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, effectFBO_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, effectTexture_, 0);
        validateFramebuffer("primary effect");

        effectFBO2_.create();
        effectTexture2_.create();
        configureEffectTexture(effectTexture2_, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, effectFBO2_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, effectTexture2_, 0);
        validateFramebuffer("secondary effect");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void init()
    {
        if (initialized_)
            throw std::logic_error("Renderer::init() cannot be called more than once without destroy()");
        if (glad_glGetString == nullptr)
            throw std::runtime_error("Renderer::init() requires GLAD to be loaded first");
        if (glGetString(GL_VERSION) == nullptr)
            throw std::runtime_error("Renderer::init() requires a current OpenGL context");

#ifndef NDEBUG
        enableOpenGLDebugOutput();
#endif

        try
        {
            initSDF();
            initPath();
            initTextured();
            initEffects();
        }
        catch (...)
        {
            destroy();
            throw;
        }

        sdfLoc_viewSize_ = glGetUniformLocation(sdfProgram_, "uViewSize");
        pathLoc_viewSize_ = glGetUniformLocation(pathProgram_, "uViewSize");
        pathLoc_paintType_ = glGetUniformLocation(pathProgram_, "uPaintType");
        pathLoc_gradStart_ = glGetUniformLocation(pathProgram_, "uGradStart");
        pathLoc_gradEnd_ = glGetUniformLocation(pathProgram_, "uGradEnd");
        pathLoc_gradInnerR_ = glGetUniformLocation(pathProgram_, "uGradInnerR");
        pathLoc_gradOuterR_ = glGetUniformLocation(pathProgram_, "uGradOuterR");
        pathLoc_gradStopCount_ = glGetUniformLocation(pathProgram_, "uGradStopCount");
        pathLoc_gradPositions_ = glGetUniformLocation(pathProgram_, "uGradPositions[0]");
        pathLoc_gradColors_ = glGetUniformLocation(pathProgram_, "uGradColors[0]");
        texLoc_viewSize_ = glGetUniformLocation(texturedProgram_, "uViewSize");
        texLoc_texture_ = glGetUniformLocation(texturedProgram_, "uTexture");
        texLoc_sdf_ = glGetUniformLocation(texturedProgram_, "uSDF");
        texLoc_effect_ = glGetUniformLocation(texturedProgram_, "uEffectTexture");
        blurLoc_radius_ = glGetUniformLocation(blurProgram_, "uRadius");
        blurLoc_direction_ = glGetUniformLocation(blurProgram_, "uDirection");
        blurLoc_texture_ = glGetUniformLocation(blurProgram_, "uTexture");
        initialized_ = true;
    }

    void destroy()
    {
        glyphBatch_.clear();
        glyphTexture_ = 0;
        glyphFontOwner_.reset();
        sdfVBO_.reset();
        sdfInstanceVBO_.reset();
        sdfVAO_.reset();
        pathVBO_.reset();
        pathVAO_.reset();
        texVBO_.reset();
        texVAO_.reset();
        blurVBO_.reset();
        blurVAO_.reset();
        sdfProgram_.reset();
        pathProgram_.reset();
        texturedProgram_.reset();
        blurProgram_.reset();
        effectFBO_.reset();
        effectTexture_.reset();
        effectFBO2_.reset();
        effectTexture2_.reset();
        effectW_ = 0;
        effectH_ = 0;
        effectCaptured_ = false;
        effectPassActive_ = false;
        frameActive_ = false;
        initialized_ = false;
        fbWidth_ = 0;
        fbHeight_ = 0;
        clipEnabled_ = false;
        clipX_ = 0;
        clipY_ = 0;
        clipWidth_ = 0;
        clipHeight_ = 0;
        roundedClips_.clear();
        sdfBatch_.clear();
    }

    void beginFrame(int fbWidth, int fbHeight)
    {
        requireInitialized("beginFrame()");
        if (frameActive_)
            throw std::logic_error("Renderer::beginFrame() cannot be nested");
        if (fbWidth <= 0 || fbHeight <= 0)
            throw std::invalid_argument("Renderer::beginFrame() requires positive framebuffer dimensions");

        frameActive_ = true;
        fbWidth_ = fbWidth;
        fbHeight_ = fbHeight;
        glViewport(0, 0, fbWidth, fbHeight);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        clipEnabled_ = false;
        roundedClips_.clear();
        glClearStencil(0);
        glStencilMask(0xFF);
        glClear(GL_STENCIL_BUFFER_BIT);
        sdfBatch_.clear();
    }

    void endFrame()
    {
        requireFrame("endFrame()");
        if (effectPassActive_)
            throw std::logic_error("Renderer::endFrame() cannot end while an effect pass is active");
        flushSDF();
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        clipEnabled_ = false;
        roundedClips_.clear();
        frameActive_ = false;
    }

    void setClipRect(int x, int y, int width, int height)
    {
        requireFrame("setClipRect()");
        if (width < 0 || height < 0)
            throw std::invalid_argument("Renderer::setClipRect() requires non-negative dimensions");

        flushSDF();
        const int left = std::clamp(x, 0, fbWidth_);
        const int top = std::clamp(y, 0, fbHeight_);
        const int right = std::clamp(x + width, 0, fbWidth_);
        const int bottom = std::clamp(y + height, 0, fbHeight_);
        clipX_ = left;
        clipY_ = top;
        clipWidth_ = std::max(0, right - left);
        clipHeight_ = std::max(0, bottom - top);
        clipEnabled_ = true;
        applyClip();
    }

    void clearClip()
    {
        requireFrame("clearClip()");
        flushSDF();
        clipEnabled_ = false;
        glDisable(GL_SCISSOR_TEST);
    }

    void setRoundedClips(const std::vector<Renderer::RoundedClip>& clips)
    {
        requireFrame("setRoundedClips()");
        flushSDF();

        if (!clips.empty())
        {
            constexpr std::size_t kMaximumStencilDepth = 255;
            if (clips.size() > kMaximumStencilDepth)
                throw std::invalid_argument("Renderer::setRoundedClips() exceeds the framebuffer stencil depth");
        }

        roundedClips_ = clips;
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_STENCIL_TEST);
        glStencilMask(0xFF);
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);

        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        NodeStyle maskStyle;
        maskStyle.fillColor = Color::White;
        maskStyle.strokeColor = Color::Transparent;
        maskStyle.strokeWidth = 0.0f;

        for (std::size_t depth = 0; depth < roundedClips_.size(); ++depth)
        {
            const auto& clip = roundedClips_[depth];
            glStencilFunc(GL_EQUAL, static_cast<GLint>(depth), 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
            const std::array<float, 4> radii = {clip.radius, clip.radius, clip.radius, clip.radius};
            drawSDFRoundedRect(clip.position, clip.size, radii, maskStyle, clip.transform);
            flushSDF();
        }

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        applyClip();
    }

    void drawSDFRect(Vec2 pos, Vec2 size, const NodeStyle& style, const Mat3x3& transform)
    {
        flushGlyphs();
        Vec2 center = transform.transformPoint(pos);
        const auto components = decomposeTransform(transform);
        SDFInstance inst{};
        inst.posX = center.x;
        inst.posY = center.y;
        inst.sizeX = size.x * components.scaleX * 0.5f;
        inst.sizeY = size.y * components.scaleY * 0.5f;
        inst.fillR = style.fillColor.r;
        inst.fillG = style.fillColor.g;
        inst.fillB = style.fillColor.b;
        inst.fillA = style.fillColor.a;
        inst.strokeR = style.strokeColor.r;
        inst.strokeG = style.strokeColor.g;
        inst.strokeB = style.strokeColor.b;
        inst.strokeA = style.strokeColor.a;
        inst.strokeWidth = style.strokeWidth * 0.5f * (components.scaleX + components.scaleY);
        inst.rotation = components.rotation;
        inst.opacity = style.opacity;
        inst.shapeType = 0;
        sdfBatch_.push_back(inst);
    }

    void drawSDFCircle(Vec2 center, float radius, const NodeStyle& style, const Mat3x3& transform)
    {
        flushGlyphs();
        Vec2 transformed = transform.transformPoint(center);
        const auto components = decomposeTransform(transform);
        SDFInstance inst{};
        inst.posX = transformed.x;
        inst.posY = transformed.y;
        inst.sizeX = radius * components.scaleX;
        inst.sizeY = radius * components.scaleY;
        inst.fillR = style.fillColor.r;
        inst.fillG = style.fillColor.g;
        inst.fillB = style.fillColor.b;
        inst.fillA = style.fillColor.a;
        inst.strokeR = style.strokeColor.r;
        inst.strokeG = style.strokeColor.g;
        inst.strokeB = style.strokeColor.b;
        inst.strokeA = style.strokeColor.a;
        inst.strokeWidth = style.strokeWidth * 0.5f * (components.scaleX + components.scaleY);
        inst.rotation = components.rotation;
        inst.opacity = style.opacity;
        inst.shapeType = std::abs(components.scaleX - components.scaleY) < 1e-5f ? 1 : 2;
        sdfBatch_.push_back(inst);
    }

    void drawSDFEllipse(Vec2 center, Vec2 radii, const NodeStyle& style, const Mat3x3& transform)
    {
        flushGlyphs();
        Vec2 transformed = transform.transformPoint(center);
        const auto components = decomposeTransform(transform);
        SDFInstance inst{};
        inst.posX = transformed.x;
        inst.posY = transformed.y;
        inst.sizeX = radii.x * components.scaleX;
        inst.sizeY = radii.y * components.scaleY;
        inst.fillR = style.fillColor.r;
        inst.fillG = style.fillColor.g;
        inst.fillB = style.fillColor.b;
        inst.fillA = style.fillColor.a;
        inst.strokeR = style.strokeColor.r;
        inst.strokeG = style.strokeColor.g;
        inst.strokeB = style.strokeColor.b;
        inst.strokeA = style.strokeColor.a;
        inst.strokeWidth = style.strokeWidth * 0.5f * (components.scaleX + components.scaleY);
        inst.rotation = components.rotation;
        inst.opacity = style.opacity;
        inst.shapeType = 2;
        sdfBatch_.push_back(inst);
    }

    void drawSDFRoundedRect(Vec2 pos, Vec2 size, const std::array<float, 4>& cornerRadii, const NodeStyle& style,
                            const Mat3x3& transform)
    {
        flushGlyphs();
        Vec2 center = transform.transformPoint(pos);
        const auto components = decomposeTransform(transform);
        const float radiusScale = 0.5f * (components.scaleX + components.scaleY);
        SDFInstance inst{};
        inst.posX = center.x;
        inst.posY = center.y;
        inst.sizeX = size.x * components.scaleX * 0.5f;
        inst.sizeY = size.y * components.scaleY * 0.5f;
        for (std::size_t i = 0; i < inst.cornerRadii.size(); ++i)
            inst.cornerRadii[i] = cornerRadii[i] * radiusScale;
        inst.fillR = style.fillColor.r;
        inst.fillG = style.fillColor.g;
        inst.fillB = style.fillColor.b;
        inst.fillA = style.fillColor.a;
        inst.strokeR = style.strokeColor.r;
        inst.strokeG = style.strokeColor.g;
        inst.strokeB = style.strokeColor.b;
        inst.strokeA = style.strokeColor.a;
        inst.strokeWidth = style.strokeWidth * radiusScale;
        inst.rotation = components.rotation;
        inst.opacity = style.opacity;
        inst.shapeType = 3;
        sdfBatch_.push_back(inst);
    }

    void flushSDF()
    {
        flushGlyphs();
        if (sdfBatch_.empty())
            return;
        glUseProgram(sdfProgram_);
        glUniform2f(sdfLoc_viewSize_, static_cast<float>(fbWidth_), static_cast<float>(fbHeight_));
        glBindVertexArray(sdfVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, sdfInstanceVBO_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sdfBatch_.size() * sizeof(SDFInstance)), sdfBatch_.data(),
                     GL_STREAM_DRAW);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(sdfBatch_.size()));
        glBindVertexArray(0);
        sdfBatch_.clear();
    }

    void fillPath(const std::vector<std::vector<Vec2>>& subPaths, Color color, const Mat3x3& transform)
    {
        flushSDF();
        std::vector<float> buffer;
        for (const auto& subPath : subPaths)
        {
            auto triangles = triangulatePolygon(subPath);
            for (const auto& vertex : triangles)
            {
                Vec2 transformed = transform.transformPoint(vertex);
                buffer.insert(buffer.end(), {transformed.x, transformed.y, color.r, color.g, color.b, color.a, 1.0f});
            }
        }

        // Compute fringe in world-space that maps to ~1 screen pixel
        float transformScale = std::sqrt(transform.m[0] * transform.m[0] + transform.m[3] * transform.m[3]);
        float fringe = (transformScale > 1e-4f) ? (1.0f / transformScale) : 1.0f;
        for (const auto& subPath : subPaths)
        {
            for (size_t i = 0; i + 1 < subPath.size(); ++i)
            {
                Vec2 a = subPath[i];
                Vec2 b = subPath[i + 1];
                Vec2 direction = (b - a).normalized();
                Vec2 normal = direction.perp() * fringe;
                Vec2 a0 = transform.transformPoint(a);
                Vec2 b0 = transform.transformPoint(b);
                Vec2 a1 = transform.transformPoint(a + normal);
                Vec2 b1 = transform.transformPoint(b + normal);
                buffer.insert(buffer.end(), {a0.x, a0.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {b0.x, b0.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {a1.x, a1.y, color.r, color.g, color.b, color.a, 0.0f});
                buffer.insert(buffer.end(), {b0.x, b0.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {b1.x, b1.y, color.r, color.g, color.b, color.a, 0.0f});
                buffer.insert(buffer.end(), {a1.x, a1.y, color.r, color.g, color.b, color.a, 0.0f});
            }
        }

        if (buffer.empty())
            return;
        glUseProgram(pathProgram_);
        glUniform2f(pathLoc_viewSize_, static_cast<float>(fbWidth_), static_cast<float>(fbHeight_));
        glUniform1i(pathLoc_paintType_, 0); // solid color
        glBindVertexArray(pathVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, pathVBO_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(buffer.size() * sizeof(float)), buffer.data(),
                     GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(buffer.size() / 7));
        glBindVertexArray(0);
    }

    void fillPathWithPaint(const std::vector<std::vector<Vec2>>& subPaths, const Paint& paint, float opacity,
                           const Mat3x3& transform)
    {
        if (paint.type == PaintType::Solid)
        {
            Color c = paint.color;
            c.a *= opacity;
            fillPath(subPaths, c, transform);
            return;
        }

        flushSDF();

        // Build geometry the same way as fillPath but with white placeholder color
        // (the shader will compute the actual color from gradient uniforms)
        Color placeholder{1.0f, 1.0f, 1.0f, opacity};
        std::vector<float> buffer;
        for (const auto& subPath : subPaths)
        {
            auto triangles = triangulatePolygon(subPath);
            for (const auto& vertex : triangles)
            {
                Vec2 transformed = transform.transformPoint(vertex);
                buffer.insert(buffer.end(), {transformed.x, transformed.y, placeholder.r, placeholder.g, placeholder.b,
                                             placeholder.a, 1.0f});
            }
        }

        float transformScale = std::sqrt(transform.m[0] * transform.m[0] + transform.m[3] * transform.m[3]);
        float fringe = (transformScale > 1e-4f) ? (1.0f / transformScale) : 1.0f;
        for (const auto& subPath : subPaths)
        {
            for (size_t i = 0; i + 1 < subPath.size(); ++i)
            {
                Vec2 a = subPath[i];
                Vec2 b = subPath[i + 1];
                Vec2 direction = (b - a).normalized();
                Vec2 normal = direction.perp() * fringe;
                Vec2 a0 = transform.transformPoint(a);
                Vec2 b0 = transform.transformPoint(b);
                Vec2 a1 = transform.transformPoint(a + normal);
                Vec2 b1 = transform.transformPoint(b + normal);
                buffer.insert(buffer.end(),
                              {a0.x, a0.y, placeholder.r, placeholder.g, placeholder.b, placeholder.a, 1.0f});
                buffer.insert(buffer.end(),
                              {b0.x, b0.y, placeholder.r, placeholder.g, placeholder.b, placeholder.a, 1.0f});
                buffer.insert(buffer.end(),
                              {a1.x, a1.y, placeholder.r, placeholder.g, placeholder.b, placeholder.a, 0.0f});
                buffer.insert(buffer.end(),
                              {b0.x, b0.y, placeholder.r, placeholder.g, placeholder.b, placeholder.a, 1.0f});
                buffer.insert(buffer.end(),
                              {b1.x, b1.y, placeholder.r, placeholder.g, placeholder.b, placeholder.a, 0.0f});
                buffer.insert(buffer.end(),
                              {a1.x, a1.y, placeholder.r, placeholder.g, placeholder.b, placeholder.a, 0.0f});
            }
        }

        if (buffer.empty())
            return;

        glUseProgram(pathProgram_);
        glUniform2f(pathLoc_viewSize_, static_cast<float>(fbWidth_), static_cast<float>(fbHeight_));

        // Set gradient uniforms
        int paintType = (paint.type == PaintType::LinearGradient) ? 1 : 2;
        glUniform1i(pathLoc_paintType_, paintType);

        // Transform gradient coordinates to screen space
        Vec2 gradStart = transform.transformPoint(paint.gradientStart);
        Vec2 gradEnd = transform.transformPoint(paint.gradientEnd);
        glUniform2f(pathLoc_gradStart_, gradStart.x, gradStart.y);
        glUniform2f(pathLoc_gradEnd_, gradEnd.x, gradEnd.y);

        if (paint.type == PaintType::RadialGradient)
        {
            glUniform1f(pathLoc_gradInnerR_, paint.innerRadius * transformScale);
            glUniform1f(pathLoc_gradOuterR_, paint.outerRadius * transformScale);
        }

        int stopCount = std::min(static_cast<int>(paint.stops.size()), 8);
        glUniform1i(pathLoc_gradStopCount_, stopCount);
        float positions[8] = {};
        float colors[32] = {};
        for (int i = 0; i < stopCount; ++i)
        {
            positions[i] = paint.stops[i].position;
            colors[i * 4 + 0] = paint.stops[i].color.r;
            colors[i * 4 + 1] = paint.stops[i].color.g;
            colors[i * 4 + 2] = paint.stops[i].color.b;
            colors[i * 4 + 3] = paint.stops[i].color.a;
        }
        glUniform1fv(pathLoc_gradPositions_, 8, positions);
        glUniform4fv(pathLoc_gradColors_, 8, reinterpret_cast<const float*>(colors));

        glBindVertexArray(pathVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, pathVBO_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(buffer.size() * sizeof(float)), buffer.data(),
                     GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(buffer.size() / 7));
        glBindVertexArray(0);

        // Reset paint type to solid for subsequent draws
        glUniform1i(pathLoc_paintType_, 0);
    }

    void strokePath(const std::vector<std::vector<Vec2>>& subPaths, Color color, float width, const Mat3x3& transform)
    {
        flushSDF();
        const float halfWidth = width * 0.5f;
        // Compute fringe in world-space that maps to ~1 screen pixel
        float transformScale = std::sqrt(transform.m[0] * transform.m[0] + transform.m[3] * transform.m[3]);
        float fringe = (transformScale > 1e-4f) ? (1.0f / transformScale) : 1.0f;
        std::vector<float> buffer;
        for (const auto& subPath : subPaths)
        {
            if (subPath.size() < 2)
                continue;
            for (size_t i = 0; i + 1 < subPath.size(); ++i)
            {
                Vec2 a = subPath[i];
                Vec2 b = subPath[i + 1];
                Vec2 direction = (b - a).normalized();
                Vec2 normal = direction.perp();
                Vec2 i0 = transform.transformPoint(a + normal * halfWidth);
                Vec2 o0 = transform.transformPoint(a - normal * halfWidth);
                Vec2 i1 = transform.transformPoint(b + normal * halfWidth);
                Vec2 o1 = transform.transformPoint(b - normal * halfWidth);
                buffer.insert(buffer.end(), {i0.x, i0.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {o0.x, o0.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {i1.x, i1.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {o0.x, o0.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {o1.x, o1.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {i1.x, i1.y, color.r, color.g, color.b, color.a, 1.0f});

                Vec2 fo0 = transform.transformPoint(a - normal * (halfWidth + fringe));
                Vec2 fo1 = transform.transformPoint(b - normal * (halfWidth + fringe));
                Vec2 fi0 = transform.transformPoint(a + normal * (halfWidth + fringe));
                Vec2 fi1 = transform.transformPoint(b + normal * (halfWidth + fringe));
                buffer.insert(buffer.end(), {o0.x, o0.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {fo0.x, fo0.y, color.r, color.g, color.b, color.a, 0.0f});
                buffer.insert(buffer.end(), {o1.x, o1.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {o1.x, o1.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {fo0.x, fo0.y, color.r, color.g, color.b, color.a, 0.0f});
                buffer.insert(buffer.end(), {fo1.x, fo1.y, color.r, color.g, color.b, color.a, 0.0f});
                buffer.insert(buffer.end(), {i0.x, i0.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {i1.x, i1.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {fi0.x, fi0.y, color.r, color.g, color.b, color.a, 0.0f});
                buffer.insert(buffer.end(), {i1.x, i1.y, color.r, color.g, color.b, color.a, 1.0f});
                buffer.insert(buffer.end(), {fi1.x, fi1.y, color.r, color.g, color.b, color.a, 0.0f});
                buffer.insert(buffer.end(), {fi0.x, fi0.y, color.r, color.g, color.b, color.a, 0.0f});
            }
        }

        if (buffer.empty())
            return;
        glUseProgram(pathProgram_);
        glUniform2f(pathLoc_viewSize_, static_cast<float>(fbWidth_), static_cast<float>(fbHeight_));
        glUniform1i(pathLoc_paintType_, 0); // solid color
        glBindVertexArray(pathVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, pathVBO_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(buffer.size() * sizeof(float)), buffer.data(),
                     GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(buffer.size() / 7));
        glBindVertexArray(0);
    }

    void drawTexturedQuad(float x, float y, float w, float h, uint32_t texture, Color tint, const Mat3x3& transform,
                          bool effectTexture = false)
    {
        flushSDF();
        Vec2 tl = transform.transformPoint({x, y});
        Vec2 tr = transform.transformPoint({x + w, y});
        Vec2 bl = transform.transformPoint({x, y + h});
        Vec2 br = transform.transformPoint({x + w, y + h});
        const float buffer[] = {
            tl.x, tl.y, 0, 0, tint.r, tint.g, tint.b, tint.a, tr.x, tr.y, 1, 0, tint.r, tint.g, tint.b, tint.a,
            bl.x, bl.y, 0, 1, tint.r, tint.g, tint.b, tint.a, tr.x, tr.y, 1, 0, tint.r, tint.g, tint.b, tint.a,
            br.x, br.y, 1, 1, tint.r, tint.g, tint.b, tint.a, bl.x, bl.y, 0, 1, tint.r, tint.g, tint.b, tint.a,
        };
        glUseProgram(texturedProgram_);
        glUniform2f(texLoc_viewSize_, static_cast<float>(fbWidth_), static_cast<float>(fbHeight_));
        glUniform1i(texLoc_sdf_, 0);
        glUniform1i(texLoc_effect_, effectTexture ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(texLoc_texture_, 0);
        glBindVertexArray(texVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, texVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(buffer), buffer, GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    void drawGlyph(float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t texture,
                   Color color, const Mat3x3& transform, bool sdf)
    {
        if (!sdfBatch_.empty())
            flushSDF();
        if (glyphTexture_ != texture || glyphSdf_ != sdf || glyphBatch_.size() >= 1024 * 48)
            flushGlyphs();
        glyphTexture_ = texture;
        glyphSdf_ = sdf;
        Vec2 tl = transform.transformPoint({x, y});
        Vec2 tr = transform.transformPoint({x + w, y});
        Vec2 bl = transform.transformPoint({x, y + h});
        Vec2 br = transform.transformPoint({x + w, y + h});
        const float buffer[] = {
            tl.x,    tl.y,    u0,      v0,      color.r, color.g, color.b, color.a, tr.x,    tr.y,    u1,      v0,
            color.r, color.g, color.b, color.a, bl.x,    bl.y,    u0,      v1,      color.r, color.g, color.b, color.a,
            tr.x,    tr.y,    u1,      v0,      color.r, color.g, color.b, color.a, br.x,    br.y,    u1,      v1,
            color.r, color.g, color.b, color.a, bl.x,    bl.y,    u0,      v1,      color.r, color.g, color.b, color.a,
        };
        glyphBatch_.insert(glyphBatch_.end(), std::begin(buffer), std::end(buffer));
    }

    void drawText(const Font& font, std::string_view text, float x, float y, Color color, const Mat3x3& transform,
                  const TextLayoutOptions& options, bool pixelSnap, TextRenderingMode mode)
    {
        if (!(color.a > 0))
            return;
        const auto layout = font.layoutText(text, options);
        const auto& m = transform.m;
        const bool snap = pixelSnap && std::abs(m[1]) < 1e-5f && std::abs(m[3]) < 1e-5f && std::abs(m[0]) > 1e-5f &&
                          std::abs(m[4]) > 1e-5f;
        const float deviceEm = font.emSize() * m[0];
        const bool bitmap = font.hasBitmapSupport() && mode != TextRenderingMode::Sdf && std::abs(m[1]) < 1e-5f &&
                            std::abs(m[3]) < 1e-5f && m[0] > 1e-5f && m[4] > 1e-5f && std::abs(m[0] - m[4]) < 1e-5f &&
                            std::isfinite(deviceEm) && deviceEm >= 1 &&
                            deviceEm <= (mode == TextRenderingMode::Bitmap ? 64 : 24) &&
                            (mode == TextRenderingMode::Bitmap || pixelSnap);
        const int pixelSize = bitmap ? static_cast<int>(std::round(deviceEm)) : 0;
        size_t index = 0;
        for (const auto& line : layout.lines)
        {
            float offsetX = x, baseline = y + line.y + font.ascent();
            if (snap)
            {
                const auto device = transform.transformPoint({x + line.x, baseline});
                offsetX += (std::round(device.x) - device.x) / m[0];
                baseline += (std::round(device.y) - device.y) / m[4];
            }
            while (index < layout.glyphs.size() && layout.glyphs[index].y == line.y)
            {
                const auto& positioned = layout.glyphs[index++];
                const auto device = transform.transformPoint({offsetX + positioned.x, baseline});
                if (!std::isfinite(device.x) || !std::isfinite(device.y))
                    continue;
                float pixelX = std::floor(device.x);
                int phase = 0;
                if (bitmap && std::isfinite(device.x + device.y))
                {
                    phase = static_cast<int>(std::round((device.x - pixelX) * 4));
                    if (phase == 4)
                    {
                        pixelX += 1;
                        phase = 0;
                    }
                }
                const auto* glyph = bitmap
                                        ? font.getBitmapGlyph(static_cast<int>(positioned.codepoint), pixelSize, phase)
                                        : font.getGlyph(static_cast<int>(positioned.codepoint));
                if (!glyph || glyph->width <= 0 || glyph->height <= 0)
                    continue;
                float gx, gy, gw, gh;
                if (!glyph->sdf)
                {
                    // Draw coverage at exactly one atlas texel per device pixel.
                    // Quarter-pixel raster phases preserve fractional kerning.
                    gx = (pixelX + glyph->xoff - m[6]) / m[0];
                    gy = (std::round(device.y) + glyph->yoff - m[7]) / m[4];
                    gw = glyph->width / m[0];
                    gh = glyph->height / m[4];
                }
                else
                {
                    gx = offsetX + positioned.x + glyph->xoff;
                    gy = baseline + glyph->yoff;
                    gw = glyph->width;
                    gh = glyph->height;
                }
                drawGlyph(gx, gy, gw, gh, glyph->u0, glyph->v0, glyph->u1, glyph->v1, glyph->texture, color, transform,
                          glyph->sdf);
            }
        }
    }

    void flushGlyphs()
    {
        if (glyphBatch_.empty())
            return;
        glUseProgram(texturedProgram_);
        glUniform2f(texLoc_viewSize_, static_cast<float>(fbWidth_), static_cast<float>(fbHeight_));
        glUniform1i(texLoc_sdf_, glyphSdf_ ? 1 : 2);
        glUniform1i(texLoc_effect_, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, glyphTexture_);
        glUniform1i(texLoc_texture_, 0);
        glBindVertexArray(texVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, texVBO_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(glyphBatch_.size() * sizeof(float)), glyphBatch_.data(),
                     GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(glyphBatch_.size() / 8));
        glBindVertexArray(0);
        glyphBatch_.clear();
        glyphTexture_ = 0;
        glyphFontOwner_.reset();
    }

    void beginEffectPass(float x, float y, float w, float h)
    {
        requireFrame("beginEffectPass()");
        if (effectPassActive_)
            throw std::logic_error("Renderer::beginEffectPass() cannot be nested");
        if (w <= 0.0f || h <= 0.0f)
            throw std::invalid_argument("Renderer::beginEffectPass() requires positive dimensions");

        flushSDF();
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        (void)x;
        (void)y;
        ensureEffectFBOs(static_cast<int>(w), static_cast<int>(h));
        glBindFramebuffer(GL_FRAMEBUFFER, effectFBO_);
        glViewport(0, 0, static_cast<int>(w), static_cast<int>(h));
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        effectCaptured_ = true;
        effectPassActive_ = true;
    }

    void endEffectPass()
    {
        requireFrame("endEffectPass()");
        if (!effectPassActive_)
            throw std::logic_error("Renderer::endEffectPass() requires an active effect pass");

        flushSDF();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fbWidth_, fbHeight_);
        effectPassActive_ = false;
        applyClip();
    }

    void blurEffectTexture(float radius)
    {
        if (radius <= 0 || !effectCaptured_)
            return;
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        const float quad[] = {-1, -1, 0, 0, 1, -1, 1, 0, 1, 1, 1, 1, -1, -1, 0, 0, 1, 1, 1, 1, -1, 1, 0, 1};
        // Filtering writes premultiplied samples directly; blending would
        // apply their alpha again and darken each blur pass.
        glDisable(GL_BLEND);
        glViewport(0, 0, effectW_, effectH_);
        glUseProgram(blurProgram_);
        glUniform1f(blurLoc_radius_, radius);
        glBindFramebuffer(GL_FRAMEBUFFER, effectFBO2_);
        glClear(GL_COLOR_BUFFER_BIT);
        glUniform2f(blurLoc_direction_, 1.0f, 0.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, effectTexture_);
        glUniform1i(blurLoc_texture_, 0);
        glBindVertexArray(blurVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, blurVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindFramebuffer(GL_FRAMEBUFFER, effectFBO_);
        glClear(GL_COLOR_BUFFER_BIT);
        glUniform2f(blurLoc_direction_, 0.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, effectTexture2_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_BLEND);
        glViewport(0, 0, fbWidth_, fbHeight_);
        applyClip();
    }

    void compositeEffect(Vec2 offset, Color tint)
    {
        if (!effectCaptured_)
            return;
        drawTexturedQuad(offset.x, offset.y, static_cast<float>(effectW_), static_cast<float>(effectH_), effectTexture_,
                         tint, Mat3x3::identity(), true);
    }

    void applyBlur(float radius)
    {
        requireFrame("applyBlur()");
        if (effectPassActive_)
            throw std::logic_error("Renderer::applyBlur() requires endEffectPass() first");
        blurEffectTexture(radius);
        compositeEffect({0, 0}, Color::White);
    }

    void applyShadow(float blur, Vec2 offset, Color color)
    {
        requireFrame("applyShadow()");
        if (effectPassActive_)
            throw std::logic_error("Renderer::applyShadow() requires endEffectPass() first");
        blurEffectTexture(blur);
        compositeEffect(offset, color);
    }

    void applyGlow(float radius, Color color)
    {
        requireFrame("applyGlow()");
        if (effectPassActive_)
            throw std::logic_error("Renderer::applyGlow() requires endEffectPass() first");
        blurEffectTexture(radius);
        compositeEffect({0, 0}, color);
    }

    void renderNodeContents(Node* node)
    {
        const auto& worldTransform = node->worldTransform();
        const auto& style = node->style();
        Vec2 size = node->size();

        switch (node->type())
        {
        case ShapeType::Text:
        {
            const auto font = node->font();
            if (!font || !(style.opacity > 0) || !(style.fillColor.a > 0))
                break;
            if (glyphFontOwner_ != font)
                flushGlyphs();
            Color color = style.fillColor;
            color.a *= style.opacity;
            drawText(*font, node->text(), 0, 0, color, worldTransform, node->textLayoutOptions(), node->textPixelSnap(),
                     node->textRenderingMode());
            if (!glyphBatch_.empty())
                glyphFontOwner_ = font;
            break;
        }
        case ShapeType::Image:
        {
            const auto image = node->image();
            const auto dimensions = node->imageSize();
            if (image && dimensions.x > 0 && dimensions.y > 0 && std::isfinite(dimensions.x + dimensions.y) &&
                style.opacity > 0)
                drawTexturedQuad(-dimensions.x * 0.5f, -dimensions.y * 0.5f, dimensions.x, dimensions.y,
                                 image->texture(), {1, 1, 1, style.opacity}, worldTransform);
            break;
        }
        case ShapeType::Rect:
            drawSDFRect({0, 0}, size, style, worldTransform);
            break;
        case ShapeType::RoundedRect:
            drawSDFRoundedRect({0, 0}, size, style.cornerRadii, style, worldTransform);
            break;
        case ShapeType::Circle:
            drawSDFCircle({0, 0}, size.x * 0.5f, style, worldTransform);
            break;
        case ShapeType::Ellipse:
            drawSDFEllipse({0, 0}, {size.x * 0.5f, size.y * 0.5f}, style, worldTransform);
            break;
        case ShapeType::Path:
        {
            auto subPaths = node->path().getSubPaths();
            Color fill = style.fillColor, stroke = style.strokeColor;
            fill.a *= style.opacity;
            stroke.a *= style.opacity;
            if (style.fillColor.a > 0)
                fillPath(subPaths, fill, worldTransform);
            if (style.strokeWidth > 0 && style.strokeColor.a > 0)
            {
                strokePath(subPaths, stroke, style.strokeWidth, worldTransform);
            }
            break;
        }
        case ShapeType::Line:
        {
            float halfLength = size.x * 0.5f;
            std::vector<std::vector<Vec2>> paths = {{{-halfLength, 0}, {halfLength, 0}}};
            Color stroke = style.strokeColor;
            stroke.a *= style.opacity;
            strokePath(paths, stroke, style.strokeWidth > 0 ? style.strokeWidth : 1.0f, worldTransform);
            break;
        }
        default:
            break;
        }
    }

    void renderNode(Node* node)
    {
        if (!node || !node->visible())
            return;

        if (!node->hasEffects() || fbWidth_ <= 0 || fbHeight_ <= 0)
        {
            renderNodeContents(node);
            return;
        }

        // Capture in framebuffer coordinates so the same world transform can be
        // used for both the off-screen and final passes.
        beginEffectPass(0, 0, static_cast<float>(fbWidth_), static_cast<float>(fbHeight_));
        renderNodeContents(node);
        flushSDF();
        endEffectPass();

        const auto& effects = node->effects();
        if (effects.hasShadow)
            applyShadow(effects.shadow.blur, effects.shadow.offset, effects.shadow.color);
        if (effects.hasGlow)
            applyGlow(effects.glow.radius, effects.glow.color);
        if (effects.hasBlur)
            applyBlur(effects.blur.radius);
        else
            renderNodeContents(node);
    }
};

Renderer::Renderer() : impl_(std::make_unique<Impl>()) {}
Renderer::~Renderer() = default;
Renderer::Renderer(Renderer&&) noexcept = default;
Renderer& Renderer::operator=(Renderer&&) noexcept = default;

void Renderer::init()
{
    impl_->init();
}

void Renderer::destroy()
{
    impl_->destroy();
}

void Renderer::beginFrame(int fbWidth, int fbHeight)
{
    impl_->beginFrame(fbWidth, fbHeight);
}

void Renderer::endFrame()
{
    impl_->endFrame();
}

void Renderer::setClipRect(int x, int y, int width, int height)
{
    impl_->setClipRect(x, y, width, height);
}

void Renderer::clearClip()
{
    impl_->clearClip();
}

void Renderer::setRoundedClips(const std::vector<RoundedClip>& clips)
{
    impl_->setRoundedClips(clips);
}

void Renderer::drawSDFRect(Vec2 pos, Vec2 size, const NodeStyle& style, const Mat3x3& transform)
{
    impl_->requireFrame("drawSDFRect()");
    impl_->drawSDFRect(pos, size, style, transform);
}

void Renderer::drawSDFCircle(Vec2 center, float radius, const NodeStyle& style, const Mat3x3& transform)
{
    impl_->requireFrame("drawSDFCircle()");
    impl_->drawSDFCircle(center, radius, style, transform);
}

void Renderer::drawSDFEllipse(Vec2 center, Vec2 radii, const NodeStyle& style, const Mat3x3& transform)
{
    impl_->requireFrame("drawSDFEllipse()");
    impl_->drawSDFEllipse(center, radii, style, transform);
}

void Renderer::drawSDFRoundedRect(Vec2 pos, Vec2 size, const std::array<float, 4>& cornerRadii, const NodeStyle& style,
                                  const Mat3x3& transform)
{
    impl_->requireFrame("drawSDFRoundedRect()");
    impl_->drawSDFRoundedRect(pos, size, cornerRadii, style, transform);
}

void Renderer::flushSDF()
{
    impl_->requireFrame("flushSDF()");
    impl_->flushSDF();
}

void Renderer::flush()
{
    impl_->requireFrame("flush()");
    impl_->flushSDF();
}

void Renderer::fillPath(const std::vector<std::vector<Vec2>>& subPaths, Color color, const Mat3x3& transform)
{
    impl_->requireFrame("fillPath()");
    impl_->fillPath(subPaths, color, transform);
}

void Renderer::fillPathWithPaint(const std::vector<std::vector<Vec2>>& subPaths, const Paint& paint, float opacity,
                                 const Mat3x3& transform)
{
    impl_->requireFrame("fillPathWithPaint()");
    impl_->fillPathWithPaint(subPaths, paint, opacity, transform);
}

void Renderer::strokePath(const std::vector<std::vector<Vec2>>& subPaths, Color color, float width,
                          const Mat3x3& transform)
{
    impl_->requireFrame("strokePath()");
    impl_->strokePath(subPaths, color, width, transform);
}

void Renderer::drawTexturedQuad(float x, float y, float w, float h, uint32_t texture, Color tint,
                                const Mat3x3& transform)
{
    impl_->requireFrame("drawTexturedQuad()");
    impl_->drawTexturedQuad(x, y, w, h, texture, tint, transform);
}

void Renderer::drawGlyph(float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t texture,
                         Color color, const Mat3x3& transform, bool sdf)
{
    impl_->requireFrame("drawGlyph()");
    impl_->drawGlyph(x, y, w, h, u0, v0, u1, v1, texture, color, transform, sdf);
}

void Renderer::beginEffectPass(float x, float y, float w, float h)
{
    impl_->beginEffectPass(x, y, w, h);
}

void Renderer::endEffectPass()
{
    impl_->endEffectPass();
}

void Renderer::applyBlur(float radius)
{
    impl_->applyBlur(radius);
}

void Renderer::applyShadow(float blur, Vec2 offset, Color color)
{
    impl_->applyShadow(blur, offset, color);
}

void Renderer::applyGlow(float radius, Color color)
{
    impl_->applyGlow(radius, color);
}

void Renderer::drawText(const Font& font, std::string_view text, float x, float y, Color color, const Mat3x3& transform,
                        const TextLayoutOptions& options, bool pixelSnap, TextRenderingMode mode)
{
    impl_->requireFrame("drawText()");
    impl_->drawText(font, text, x, y, color, transform, options, pixelSnap, mode);
}

void Renderer::renderNode(Node* node)
{
    impl_->requireFrame("renderNode()");
    impl_->renderNode(node);
}

bool Renderer::isInitialized() const noexcept
{
    return impl_ && impl_->initialized_;
}

bool Renderer::isFrameActive() const noexcept
{
    return impl_ && impl_->frameActive_;
}

int Renderer::fbWidth() const
{
    return impl_->fbWidth_;
}

int Renderer::fbHeight() const
{
    return impl_->fbHeight_;
}

} // namespace vectorgl
