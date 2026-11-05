#include "vectorgl/ui/painting.hpp"

#include <cmath>
#include <stdexcept>

#include "vectorgl/canvas.hpp"

namespace vectorgl::ui
{
Viewport::Viewport(float width, float height, int fw, int fh)
    : width_(width), height_(height), framebufferWidth_(fw), framebufferHeight_(fh)
{
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0 || fw <= 0 || fh <= 0 ||
        !std::isfinite(scaleX()) || !std::isfinite(scaleY()))
        throw std::invalid_argument("UI viewport requires positive finite logical and framebuffer sizes");
}
PaintContext::PaintContext(Canvas& canvas, const Theme& theme, Bounds bounds, bool enabled, bool focused, bool hovered)
    : canvas_(canvas), theme_(theme), bounds_(bounds), enabled_(enabled), focused_(focused), hovered_(hovered)
{
}
void PaintContext::fillRect(Bounds b, Color color) const
{
    canvas_.setFillColor(color);
    canvas_.beginPath();
    canvas_.moveTo(b.x, b.y);
    canvas_.lineTo(b.x + b.width, b.y);
    canvas_.lineTo(b.x + b.width, b.y + b.height);
    canvas_.lineTo(b.x, b.y + b.height);
    canvas_.closePath();
    canvas_.fill();
}
void SurfacePainter::paint(PaintContext& context)
{
    const auto& theme = context.theme();
    const auto bounds = context.bounds();
    context.fillRect(bounds, !context.enabled() ? theme.disabled : context.hovered() ? theme.hover : theme.surface);
    auto& canvas = context.canvas();
    canvas.setLineWidth(context.focused() ? theme.borderWidth + 1 : theme.borderWidth);
    canvas.setStrokeColor(context.focused() ? theme.accent : theme.border);
    canvas.strokeRect(bounds.x, bounds.y, bounds.width, bounds.height);
}
} // namespace vectorgl::ui
