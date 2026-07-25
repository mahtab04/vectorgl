#include "vectorgl/canvas.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace vectorgl
{
namespace
{

uint32_t nextUtf8Codepoint(const std::string& text, std::size_t& offset)
{
    const auto first = static_cast<uint8_t>(text[offset++]);
    if (first < 0x80)
        return first;

    int continuationCount = 0;
    uint32_t codepoint = 0;
    if ((first & 0xE0) == 0xC0)
    {
        continuationCount = 1;
        codepoint = first & 0x1F;
    }
    else if ((first & 0xF0) == 0xE0)
    {
        continuationCount = 2;
        codepoint = first & 0x0F;
    }
    else if ((first & 0xF8) == 0xF0)
    {
        continuationCount = 3;
        codepoint = first & 0x07;
    }
    else
    {
        return 0xFFFD;
    }

    if (offset + static_cast<std::size_t>(continuationCount) > text.size())
    {
        offset = text.size();
        return 0xFFFD;
    }
    for (int i = 0; i < continuationCount; ++i)
    {
        const auto next = static_cast<uint8_t>(text[offset]);
        if ((next & 0xC0) != 0x80)
            return 0xFFFD;
        ++offset;
        codepoint = (codepoint << 6) | (next & 0x3F);
    }
    return codepoint;
}

} // namespace

Canvas::Canvas() = default;
Canvas::~Canvas()
{
    destroy();
}

void Canvas::init()
{
    renderer_.init();
}

void Canvas::destroy()
{
    fonts_.clear();
    activeFont_ = nullptr;
    renderer_.destroy();
}

void Canvas::beginFrame(int fbWidth, int fbHeight)
{
    renderer_.beginFrame(fbWidth, fbHeight);
    currentState_ = State{};
    stateStack_.clear();
}

void Canvas::endFrame()
{
    renderer_.endFrame();
}

void Canvas::save()
{
    stateStack_.push_back(currentState_);
}

void Canvas::restore()
{
    if (!stateStack_.empty())
    {
        currentState_ = stateStack_.back();
        stateStack_.pop_back();
        applyClipState();
    }
}

void Canvas::clipRect(float x, float y, float width, float height)
{
    if (width < 0.0f || height < 0.0f)
        throw std::invalid_argument("Canvas::clipRect() requires non-negative dimensions");

    const Vec2 corners[] = {
        currentState_.transform.transformPoint({x, y}),
        currentState_.transform.transformPoint({x + width, y}),
        currentState_.transform.transformPoint({x, y + height}),
        currentState_.transform.transformPoint({x + width, y + height}),
    };

    float left = corners[0].x;
    float top = corners[0].y;
    float right = corners[0].x;
    float bottom = corners[0].y;
    for (const auto& corner : corners)
    {
        left = std::min(left, corner.x);
        top = std::min(top, corner.y);
        right = std::max(right, corner.x);
        bottom = std::max(bottom, corner.y);
    }

    if (currentState_.clip.enabled)
    {
        left = std::max(left, currentState_.clip.x);
        top = std::max(top, currentState_.clip.y);
        right = std::min(right, currentState_.clip.x + currentState_.clip.width);
        bottom = std::min(bottom, currentState_.clip.y + currentState_.clip.height);
    }

    currentState_.clip = {left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top), true};
    applyClipState();
}

void Canvas::clipRoundedRect(float x, float y, float width, float height, float radius)
{
    if (width < 0.0f || height < 0.0f || radius < 0.0f)
        throw std::invalid_argument("Canvas::clipRoundedRect() requires non-negative dimensions and radius");

    const float clampedRadius = std::min(radius, std::min(width, height) * 0.5f);
    currentState_.roundedClips.push_back(
        {{x + width * 0.5f, y + height * 0.5f}, {width, height}, clampedRadius, currentState_.transform});

    // The transformed bounds are also useful as a coarse scissor. The stencil
    // buffer retains the exact rounded and transformed boundary.
    clipRect(x, y, width, height);
}

void Canvas::resetClip()
{
    currentState_.clip = {};
    currentState_.roundedClips.clear();
    applyClipState();
}

void Canvas::applyClipState()
{
    if (!currentState_.clip.enabled)
        renderer_.clearClip();
    else
    {
        const int left = static_cast<int>(std::floor(currentState_.clip.x));
        const int top = static_cast<int>(std::floor(currentState_.clip.y));
        const int right = static_cast<int>(std::ceil(currentState_.clip.x + currentState_.clip.width));
        const int bottom = static_cast<int>(std::ceil(currentState_.clip.y + currentState_.clip.height));
        renderer_.setClipRect(left, top, std::max(0, right - left), std::max(0, bottom - top));
    }
    renderer_.setRoundedClips(currentState_.roundedClips);
}

void Canvas::translate(float x, float y)
{
    currentState_.transform = currentState_.transform * Mat3x3::translation(x, y);
}

void Canvas::rotate(float angle)
{
    currentState_.transform = currentState_.transform * Mat3x3::rotation(angle);
}

void Canvas::scale(float x, float y)
{
    currentState_.transform = currentState_.transform * Mat3x3::scaling(x, y);
}

void Canvas::setFillColor(Color c)
{
    currentState_.fillColor = c;
}
void Canvas::setStrokeColor(Color c)
{
    currentState_.strokeColor = c;
}
void Canvas::setLineWidth(float w)
{
    currentState_.lineWidth = w;
}

bool Canvas::setFont(const std::string& fontPath, float size)
{
    for (auto& fe : fonts_)
    {
        if (fe.path == fontPath && fe.size == size)
        {
            activeFont_ = &fe.font;
            return true;
        }
    }
    FontEntry entry{fontPath, size, {}};
    if (!entry.font.load(fontPath, size))
        return false;
    fonts_.push_back(std::move(entry));
    activeFont_ = &fonts_.back().font;
    return true;
}

// ============================================================
// SDF-Accelerated Shape Drawing
// ============================================================

void Canvas::fillRect(float x, float y, float w, float h)
{
    NodeStyle style;
    style.fillColor = currentState_.fillColor;
    style.strokeWidth = 0;
    style.opacity = 1.0f;
    Vec2 center = {x + w * 0.5f, y + h * 0.5f};
    renderer_.drawSDFRect(center, {w, h}, style, currentState_.transform);
}

void Canvas::strokeRect(float x, float y, float w, float h)
{
    NodeStyle style;
    style.fillColor = Color{0, 0, 0, 0};
    style.strokeColor = currentState_.strokeColor;
    style.strokeWidth = currentState_.lineWidth;
    style.opacity = 1.0f;
    Vec2 center = {x + w * 0.5f, y + h * 0.5f};
    renderer_.drawSDFRect(center, {w, h}, style, currentState_.transform);
}

void Canvas::fillCircle(float cx, float cy, float r)
{
    NodeStyle style;
    style.fillColor = currentState_.fillColor;
    style.strokeWidth = 0;
    style.opacity = 1.0f;
    renderer_.drawSDFCircle({cx, cy}, r, style, currentState_.transform);
}

void Canvas::strokeCircle(float cx, float cy, float r)
{
    NodeStyle style;
    style.fillColor = Color{0, 0, 0, 0};
    style.strokeColor = currentState_.strokeColor;
    style.strokeWidth = currentState_.lineWidth;
    style.opacity = 1.0f;
    renderer_.drawSDFCircle({cx, cy}, r, style, currentState_.transform);
}

void Canvas::fillEllipse(float cx, float cy, float rx, float ry)
{
    NodeStyle style;
    style.fillColor = currentState_.fillColor;
    style.strokeWidth = 0;
    style.opacity = 1.0f;
    renderer_.drawSDFEllipse({cx, cy}, {rx, ry}, style, currentState_.transform);
}

void Canvas::strokeEllipse(float cx, float cy, float rx, float ry)
{
    NodeStyle style;
    style.fillColor = Color{0, 0, 0, 0};
    style.strokeColor = currentState_.strokeColor;
    style.strokeWidth = currentState_.lineWidth;
    style.opacity = 1.0f;
    renderer_.drawSDFEllipse({cx, cy}, {rx, ry}, style, currentState_.transform);
}

void Canvas::fillRoundedRect(float x, float y, float w, float h, float radius)
{
    radius = std::min(radius, std::min(w, h) * 0.5f);
    NodeStyle style;
    style.fillColor = currentState_.fillColor;
    style.strokeWidth = 0;
    style.opacity = 1.0f;
    std::array<float, 4> radii = {radius, radius, radius, radius};
    Vec2 center = {x + w * 0.5f, y + h * 0.5f};
    renderer_.drawSDFRoundedRect(center, {w, h}, radii, style, currentState_.transform);
}

void Canvas::strokeRoundedRect(float x, float y, float w, float h, float radius)
{
    radius = std::min(radius, std::min(w, h) * 0.5f);
    NodeStyle style;
    style.fillColor = Color{0, 0, 0, 0};
    style.strokeColor = currentState_.strokeColor;
    style.strokeWidth = currentState_.lineWidth;
    style.opacity = 1.0f;
    std::array<float, 4> radii = {radius, radius, radius, radius};
    Vec2 center = {x + w * 0.5f, y + h * 0.5f};
    renderer_.drawSDFRoundedRect(center, {w, h}, radii, style, currentState_.transform);
}

// ============================================================
// Path Drawing (complex shapes fall back to triangulation)
// ============================================================

void Canvas::beginPath()
{
    currentPath_.clear();
}
void Canvas::moveTo(float x, float y)
{
    currentPath_.moveTo(x, y);
}
void Canvas::lineTo(float x, float y)
{
    currentPath_.lineTo(x, y);
}

void Canvas::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
    currentPath_.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y);
}

void Canvas::quadraticCurveTo(float cpx, float cpy, float x, float y)
{
    currentPath_.quadraticCurveTo(cpx, cpy, x, y);
}

void Canvas::arc(float cx, float cy, float r, float start, float end, bool ccw)
{
    currentPath_.arc(cx, cy, r, start, end, ccw);
}

void Canvas::closePath()
{
    currentPath_.closePath();
}

void Canvas::fill()
{
    auto sp = currentPath_.getSubPaths();
    renderer_.fillPath(sp, currentState_.fillColor, currentState_.transform);
}

void Canvas::fillWithPaint(const Paint& paint, float opacity)
{
    auto sp = currentPath_.getSubPaths();
    renderer_.fillPathWithPaint(sp, paint, opacity, currentState_.transform);
}

void Canvas::stroke()
{
    auto sp = currentPath_.getSubPaths();
    renderer_.strokePath(sp, currentState_.strokeColor, currentState_.lineWidth, currentState_.transform);
}

// ============================================================
// Text & Images
// ============================================================

void Canvas::fillText(const std::string& text, float x, float y)
{
    if (!activeFont_)
        return;

    // Detect axis-aligned transform for pixel snapping
    bool axisAligned = std::abs(currentState_.transform.m[1]) < 1e-5f && std::abs(currentState_.transform.m[3]) < 1e-5f;

    float penX = axisAligned ? std::round(x) : x;
    float baseline = y + activeFont_->ascent();
    if (axisAligned)
        baseline = std::round(baseline);

    for (std::size_t offset = 0; offset < text.size();)
    {
        const auto codepoint = nextUtf8Codepoint(text, offset);
        auto* glyph = activeFont_->getGlyph(static_cast<int>(codepoint));
        if (!glyph)
            glyph = activeFont_->getGlyph('?');
        if (!glyph)
            continue;

        float gx = penX + glyph->xoff;
        float gy = baseline + glyph->yoff;
        if (axisAligned)
        {
            gx = std::round(gx);
            gy = std::round(gy);
        }

        renderer_.drawGlyph(gx, gy, glyph->width, glyph->height, glyph->u0, glyph->v0, glyph->u1, glyph->v1,
                            activeFont_->atlasTexture(), currentState_.fillColor, currentState_.transform);
        penX += glyph->xadvance;
    }
}

float Canvas::measureText(const std::string& text) const
{
    if (!activeFont_)
        return 0.0f;

    float width = 0.0f;
    for (std::size_t offset = 0; offset < text.size();)
    {
        const auto codepoint = nextUtf8Codepoint(text, offset);
        const auto* glyph = activeFont_->getGlyph(static_cast<int>(codepoint));
        if (!glyph)
            glyph = activeFont_->getGlyph('?');
        if (glyph)
            width += glyph->xadvance;
    }

    return width;
}

float Canvas::lineHeight() const
{
    return activeFont_ ? activeFont_->lineHeight() : 0.0f;
}

Image Canvas::loadImage(const std::string& path)
{
    Image img;
    if (!img.load(path))
    {
        return {};
    }
    return img;
}

void Canvas::drawImage(const Image& img, float x, float y, float w, float h)
{
    if (!img.valid())
        return;
    float drawW = w > 0 ? w : static_cast<float>(img.width());
    float drawH = h > 0 ? h : static_cast<float>(img.height());
    renderer_.drawTexturedQuad(x, y, drawW, drawH, img.texture(), Color::White, currentState_.transform);
}

} // namespace vectorgl
