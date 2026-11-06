#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "vectorgl/canvas.hpp"
#include "vectorgl/ui/controls.hpp"

namespace vectorgl::ui::detail
{
inline void validPreferred(Size size)
{
    if (!std::isfinite(size.width) || !std::isfinite(size.height) || size.width < 0 || size.height < 0)
        throw std::invalid_argument("Control preferred size must be finite and nonnegative");
}
inline void remove(Context& ui, WidgetId id) noexcept
{
    try
    {
        ui.remove(id);
    }
    catch (...)
    {
    }
}
struct ClickState : WidgetPainter
{
    Context& ui;
    WidgetId id;
    std::string label;
    Size preferred;
    bool pressed = false, inside = false, pointerPress = false;
    std::uint64_t pointer = 0;
    Key activationKey = Key::None;
    ClickState(Context& context, std::string text, Size size) : ui(context), label(std::move(text)), preferred(size) {}
    virtual void activate() = 0;
    virtual bool checked() const
    {
        return false;
    }
    virtual float textInset() const
    {
        return 12;
    }
    virtual void indicator(PaintContext&) {}
    void reset()
    {
        pressed = pointerPress = inside = false;
        activationKey = Key::None;
        ui.invalidate();
    }
    EventReply event(const Event& e)
    {
        if (e.type == EventType::Cancel || e.type == EventType::FocusLost)
        {
            reset();
            return {};
        }
        if (e.type == EventType::PointerDown && e.button == PointerButton::Primary)
        {
            reset();
            pressed = inside = pointerPress = true;
            pointer = e.pointer;
            return {true, true, true};
        }
        if (pointerPress && e.pointer == pointer && e.type == EventType::PointerMove)
        {
            auto b = ui.bounds(id);
            inside = b && b->contains(e.localX, e.localY);
            return {true};
        }
        if (pointerPress && e.pointer == pointer && e.type == EventType::PointerUp &&
            e.button == PointerButton::Primary)
        {
            auto b = ui.bounds(id);
            const bool fire = b && b->contains(e.localX, e.localY);
            reset();
            if (fire)
                activate();
            return {true};
        }
        if (e.key == Key::Space || e.key == Key::Enter)
        {
            if (e.type == EventType::KeyDown && !pressed)
            {
                pressed = inside = true;
                activationKey = e.key;
            }
            if (e.type == EventType::KeyUp && activationKey == e.key)
            {
                reset();
                activate();
            }
            if (e.type == EventType::KeyDown || e.type == EventType::KeyRepeat || e.type == EventType::KeyUp)
                return {true};
        }
        if (e.type == EventType::KeyDown && e.key == Key::Escape && pressed)
        {
            reset();
            return {true};
        }
        return {};
    }
    void paint(PaintContext& p) override
    {
        const auto colors =
            controlColors(p.theme(), {p.enabled(), p.hovered(), p.focused(), pressed && inside, checked()});
        p.fillRect(p.bounds(), colors.background);
        auto& c = p.canvas();
        const auto b = p.bounds();
        c.setStrokeColor(colors.border);
        c.setLineWidth(p.focused() ? p.theme().borderWidth + 1 : p.theme().borderWidth);
        c.strokeRect(b.x, b.y, b.width, b.height);
        indicator(p);
        if (!p.theme().fontPath.empty() && c.setFont(p.theme().fontPath, p.theme().fontSize))
        {
            c.clipRect(b.x, b.y, b.width, b.height);
            c.setFillColor(colors.text);
            c.fillText(label, b.x + textInset(), b.y + std::max(0.0f, (b.height - c.lineHeight()) * .5f));
        }
    }
};
inline WidgetId registerClick(const std::shared_ptr<ClickState>& state, WidgetId parent)
{
    validPreferred(state->preferred);
    Widget w;
    w.parent = parent;
    w.focusable = true;
    w.painter = state;
    w.onMeasure = [state](const MeasureContext& c) { return c.constraints.constrain(state->preferred); };
    w.onEvent = [state](Context&, WidgetId, const Event& e) { return state->event(e); };
    state->id = state->ui.add(w);
    return state->id;
}
} // namespace vectorgl::ui::detail
