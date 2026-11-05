#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "vectorgl/ui/containers.hpp"

namespace vectorgl::ui
{
namespace
{
float axisSize(Size size, SplitAxis axis)
{
    return axis == SplitAxis::Horizontal ? size.width : size.height;
}
void validate(const SplitOptions& options)
{
    if ((options.axis != SplitAxis::Horizontal && options.axis != SplitAxis::Vertical) ||
        !std::isfinite(options.fraction) || options.fraction < 0 || options.fraction > 1 ||
        !std::isfinite(options.divider) || options.divider <= 0 || !std::isfinite(options.keyboardStep) ||
        options.keyboardStep <= 0 || !std::isfinite(options.firstMinimum) || options.firstMinimum < 0 ||
        !std::isfinite(options.secondMinimum) || options.secondMinimum < 0 || std::isnan(options.firstMaximum) ||
        options.firstMaximum < options.firstMinimum || std::isnan(options.secondMaximum) ||
        options.secondMaximum < options.secondMinimum)
        throw std::invalid_argument("Invalid split panel options");
}
} // namespace
struct SplitPanel::State
{
    Context& ui;
    SplitOptions options;
    WidgetId root, first, second, divider;
    float fraction, firstExtent = 0, availableExtent = 0, grab = 0, startFraction = 0;
    Size rootSize;
    bool dragging = false;
    std::uint64_t pointer = 0;
    std::function<void(bool)> finished;
    State(Context& context, SplitOptions value) : ui(context), options(value), fraction(value.fraction) {}
    void set(float value)
    {
        if (!std::isfinite(value))
            throw std::invalid_argument("Split fraction must be finite");
        value = std::clamp(value, 0.0f, 1.0f);
        if (fraction != value)
        {
            fraction = value;
            ui.invalidate(Invalidation::Arrange);
        }
    }
    void sync()
    {
        const auto bounds = ui.bounds(root), pane = ui.bounds(first);
        if (!bounds || !pane)
            return;
        const float extent = axisSize({bounds->width, bounds->height}, options.axis);
        const Size size{bounds->width, bounds->height};
        const bool resized = size != rootSize;
        rootSize = size;
        availableExtent = std::max(0.0f, extent - options.divider);
        firstExtent = axisSize({pane->width, pane->height}, options.axis);
        if (resized && dragging && ui.captured() == divider)
            ui.cancelCapture(CancelReason::Unavailable);
    }
    EventReply event(const Event& event)
    {
        if (event.type == EventType::Cancel)
        {
            if (dragging)
            {
                dragging = false;
                set(startFraction);
                auto callback = finished;
                if (callback)
                    callback(true);
            }
            return {};
        }
        if (event.type == EventType::PointerDown && event.button == PointerButton::Primary && !dragging)
        {
            const auto local = ui.mapFromRoot(root, {event.x, event.y});
            if (!local)
                return {};
            dragging = true;
            pointer = event.pointer;
            startFraction = fraction;
            grab = (options.axis == SplitAxis::Horizontal ? local->x : local->y) - firstExtent;
            return {true, true, true};
        }
        if (dragging && event.pointer == pointer && event.type == EventType::PointerMove)
        {
            const auto local = ui.mapFromRoot(root, {event.x, event.y});
            if (local && availableExtent > 0)
                set(((options.axis == SplitAxis::Horizontal ? local->x : local->y) - grab) / availableExtent);
            return {true};
        }
        if (dragging && event.pointer == pointer && event.type == EventType::PointerUp &&
            event.button == PointerButton::Primary)
        {
            dragging = false;
            auto callback = finished;
            if (callback)
                callback(false);
            return {true};
        }
        if (event.type == EventType::KeyDown || event.type == EventType::KeyRepeat)
        {
            if (event.key == Key::Home)
            {
                set(0);
                return {true};
            }
            if (event.key == Key::End)
            {
                set(1);
                return {true};
            }
            const bool previous = event.key == (options.axis == SplitAxis::Horizontal ? Key::Left : Key::Up);
            const bool next = event.key == (options.axis == SplitAxis::Horizontal ? Key::Right : Key::Down);
            if (previous || next)
            {
                if (availableExtent > 0)
                    set((firstExtent + (previous ? -options.keyboardStep : options.keyboardStep)) / availableExtent);
                return {true};
            }
        }
        return {};
    }
    struct Strategy final : Layout
    {
        std::shared_ptr<State> state;
        explicit Strategy(std::shared_ptr<State> value) : state(std::move(value)) {}
        Size measure(std::span<const LayoutItem> items, Constraints c) const override
        {
            if (items.size() != 3)
                throw std::logic_error("Split panel internal children were removed/hidden");
            const auto a = items[0].measure(), b = items[1].measure();
            const auto& o = state->options;
            const Size desired =
                o.axis == SplitAxis::Horizontal
                    ? Size{std::max(a.width, o.firstMinimum) + o.divider + std::max(b.width, o.secondMinimum),
                           std::max(a.height, b.height)}
                    : Size{std::max(a.width, b.width),
                           std::max(a.height, o.firstMinimum) + o.divider + std::max(b.height, o.secondMinimum)};
            return c.constrain(desired);
        }
        std::vector<Bounds> arrange(std::span<const LayoutItem> items, Size area) const override
        {
            if (items.size() != 3)
                throw std::logic_error("Split panel internal children were removed/hidden");
            const auto& o = state->options;
            const auto lowA = items[0].hints.minimumSize(), highA = items[0].hints.maximumSize();
            const auto lowB = items[1].hints.minimumSize(), highB = items[1].hints.maximumSize();
            const float minimumA = std::max(o.firstMinimum, axisSize(lowA, o.axis)),
                        maximumA = std::min(o.firstMaximum, axisSize(highA, o.axis));
            const float minimumB = std::max(o.secondMinimum, axisSize(lowB, o.axis)),
                        maximumB = std::min(o.secondMaximum, axisSize(highB, o.axis));
            if (minimumA > maximumA || minimumB > maximumB)
                throw std::invalid_argument("Split limits conflict with pane size hints");
            const float available = std::max(0.0f, axisSize(area, o.axis) - o.divider);
            float a = minimumA, b = minimumB;
            if (double(available) >= double(minimumA) + minimumB)
            {
                const float lower = std::max(minimumA, available - maximumB),
                            upper = std::min(maximumA, available - minimumB);
                a = lower <= upper ? std::clamp(state->fraction * available, lower, upper)
                                   : std::clamp(state->fraction * available, minimumA, maximumA);
                b = std::clamp(available - a, minimumB, maximumB);
            }
            if (o.axis == SplitAxis::Horizontal)
                return {{0, 0, a, std::clamp(area.height, lowA.height, highA.height)},
                        {a + o.divider, 0, b, std::clamp(area.height, lowB.height, highB.height)},
                        {a, 0, o.divider, area.height}};
            return {{0, 0, std::clamp(area.width, lowA.width, highA.width), a},
                    {0, a + o.divider, std::clamp(area.width, lowB.width, highB.width), b},
                    {0, a, area.width, o.divider}};
        }
    };
    struct DividerPainter final : WidgetPainter
    {
        std::shared_ptr<State> state;
        explicit DividerPainter(std::shared_ptr<State> value) : state(std::move(value)) {}
        void paint(PaintContext& p) override
        {
            p.fillRect(p.bounds(), state->dragging || p.hovered() || p.focused() ? p.theme().accent : p.theme().border);
        }
    };
};
SplitPanel::SplitPanel(Context& context, WidgetId parent, SplitOptions options)
{
    validate(options);
    state_ = std::make_shared<State>(context, options);
    auto state = state_;
    try
    {
        Widget root;
        root.parent = parent;
        root.clipChildren = true;
        root.layout = std::make_shared<State::Strategy>(state);
        root.onEvent = [state](Context&, WidgetId, const Event& e)
        {
            if (e.type == EventType::LayoutChanged)
                state->sync();
            return EventReply{};
        };
        state->root = context.add(root);
        Widget pane;
        pane.parent = state->root;
        pane.clipChildren = true;
        pane.painter = std::make_shared<SurfacePainter>(SurfaceRole::Panel);
        state->first = context.add(pane);
        state->second = context.add(pane);
        Widget divider;
        divider.parent = state->root;
        divider.focusable = true;
        divider.painter = std::make_shared<State::DividerPainter>(state);
        divider.onEvent = [state](Context&, WidgetId, const Event& e) { return state->event(e); };
        state->divider = context.add(divider);
    }
    catch (...)
    {
        try
        {
            context.remove(state->root);
        }
        catch (...)
        {
        }
        throw;
    }
}
SplitPanel::~SplitPanel()
{
    try
    {
        state_->ui.remove(state_->root);
    }
    catch (...)
    {
    }
}
WidgetId SplitPanel::id() const
{
    return state_->root;
}
WidgetId SplitPanel::first() const
{
    return state_->first;
}
WidgetId SplitPanel::second() const
{
    return state_->second;
}
WidgetId SplitPanel::divider() const
{
    return state_->divider;
}
float SplitPanel::fraction() const
{
    return state_->fraction;
}
float SplitPanel::firstExtent() const
{
    return state_->firstExtent;
}
void SplitPanel::setFraction(float fraction)
{
    if (!std::isfinite(fraction))
        throw std::invalid_argument("Split fraction must be finite");
    if (state_->ui.captured() == divider())
        state_->ui.cancelCapture();
    if (state_->ui.contains(id()))
        state_->set(fraction);
}
void SplitPanel::setOnDragFinished(std::function<void(bool)> callback)
{
    state_->finished = std::move(callback);
}
} // namespace vectorgl::ui
