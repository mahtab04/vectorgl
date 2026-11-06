#pragma once

#include <string>

#include "vectorgl/color.hpp"
#include "vectorgl/ui/widget.hpp"

namespace vectorgl
{
class Canvas;
}
namespace vectorgl::ui
{
/*! Logical UI extent and its destination framebuffer. Independent X/Y ratios. */
class Viewport
{
public:
    Viewport(float logicalWidth, float logicalHeight, int framebufferWidth, int framebufferHeight);
    float width() const
    {
        return width_;
    }
    float height() const
    {
        return height_;
    }
    int framebufferWidth() const
    {
        return framebufferWidth_;
    }
    int framebufferHeight() const
    {
        return framebufferHeight_;
    }
    float scaleX() const
    {
        return static_cast<float>(framebufferWidth_) / width_;
    }
    float scaleY() const
    {
        return static_cast<float>(framebufferHeight_) / height_;
    }

private:
    float width_, height_;
    int framebufferWidth_, framebufferHeight_;
};
struct Theme
{
    Color background{0.035f, 0.047f, 0.071f, 1};
    Color surface{0.09f, 0.12f, 0.18f, 1};
    Color hover{0.14f, 0.20f, 0.28f, 1};
    Color text{0.85f, 0.90f, 0.96f, 1};
    Color muted{0.54f, 0.63f, 0.74f, 1};
    Color accent{0.22f, 0.75f, 0.94f, 1};
    Color border{0.23f, 0.31f, 0.42f, 1};
    Color disabled{0.12f, 0.14f, 0.18f, 1};
    std::string fontPath;
    float fontSize = 18;
    float borderWidth = 1;
    Color pressed{0.12f, 0.32f, 0.43f, 1};
    Color error{0.98f, 0.35f, 0.38f, 1};
};
/*! Ephemeral paint data. Canvas already has the widget-to-framebuffer transform
 * and ancestor clips. Painters must not begin/end frames or unbalance save/restore.
 * Canvas styling/transforms/clips are scoped; current paths and selected fonts
 * follow Canvas's normal semantics. Do not retain this object after paint().
 */
class PaintContext
{
public:
    PaintContext(Canvas& canvas, const Theme& theme, Bounds bounds, bool enabled, bool focused, bool hovered);
    Canvas& canvas() const
    {
        return canvas_;
    }
    const Theme& theme() const
    {
        return theme_;
    }
    Bounds bounds() const
    {
        return bounds_;
    }
    bool enabled() const
    {
        return enabled_;
    }
    bool focused() const
    {
        return focused_;
    }
    bool hovered() const
    {
        return hovered_;
    }
    /*! Path-based fill supports shear/reflections, unlike analytic SDF rectangles. */
    void fillRect(Bounds bounds, Color color) const;

private:
    Canvas& canvas_;
    const Theme& theme_;
    Bounds bounds_;
    bool enabled_, focused_, hovered_;
};
/*! Polymorphic painting strategy; application state and input can live in a
 * derived object captured by Widget::onEvent. Context remains the focus authority.
 */
class WidgetPainter
{
public:
    virtual ~WidgetPainter() = default;
    virtual void paint(PaintContext& context) = 0;
};
enum class SurfaceRole
{
    Panel,
    Control
};
/*! Minimal themed surface/focus outline. Passive panels retain their fill while
 * pointer routing moves between the panel and its children. Controls use hover fill.
 */
class SurfacePainter : public WidgetPainter
{
public:
    explicit SurfacePainter(SurfaceRole role = SurfaceRole::Control) : role_(role) {}
    void paint(PaintContext& context) override;

private:
    SurfaceRole role_;
};
} // namespace vectorgl::ui
