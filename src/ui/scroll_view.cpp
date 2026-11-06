#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "vectorgl/ui/containers.hpp"

namespace vectorgl::ui
{
namespace
{
constexpr float infinity = std::numeric_limits<float>::infinity();
void validate(const ScrollOptions& o)
{
    if ((!o.horizontal && !o.vertical) || !std::isfinite(o.lineStep) || o.lineStep <= 0 ||
        !std::isfinite(o.scrollbar) || o.scrollbar <= 0 || !std::isfinite(o.minimumThumb) || o.minimumThumb <= 0 ||
        !std::isfinite(o.preferredSize.width) || !std::isfinite(o.preferredSize.height) || o.preferredSize.width < 0 ||
        o.preferredSize.height < 0 ||
        (o.chaining != ScrollChaining::Ancestors && o.chaining != ScrollChaining::Contain))
        throw std::invalid_argument("Invalid scroll view options");
}
bool changed(Vec2 delta)
{
    return delta.x != 0 || delta.y != 0;
}
} // namespace
struct ScrollView::State
{
    Context& ui;
    ScrollOptions options;
    ScrollModel model;
    WidgetId root, viewport, content, horizontal, vertical;
    bool dragging = false, dragHorizontal = false;
    float grab = 0;
    std::uint64_t pointer = 0;
    State(Context& context, ScrollOptions value) : ui(context), options(value), model(value.horizontal, value.vertical)
    {
    }
    void apply()
    {
        if (!ui.contains(content))
            return;
        const auto offset = model.offset();
        ui.setTransform(content, Mat3x3::translation(-offset.x, -offset.y));
        ui.invalidate(Invalidation::Paint | Invalidation::Semantics);
    }
    void sync()
    {
        const auto view = ui.bounds(viewport), extent = ui.bounds(content);
        if (!view || !extent)
            return;
        const Size size{view->width, view->height}, full{extent->width, extent->height};
        const bool resized = size != model.viewport() || full != model.content();
        model.setGeometry(size, full);
        if (resized && dragging && (ui.captured() == horizontal || ui.captured() == vertical))
            ui.cancelCapture(CancelReason::Unavailable);
        apply();
    }
    bool reveal(WidgetId target, float margin)
    {
        if (!ui.isDescendant(target, content))
            return false;
        const auto bounds = ui.bounds(target);
        if (!bounds)
            return false;
        Vec2 low{infinity, infinity}, high{-infinity, -infinity};
        for (Vec2 point :
             {Vec2{bounds->x, bounds->y}, Vec2{bounds->x + bounds->width, bounds->y},
              Vec2{bounds->x, bounds->y + bounds->height}, Vec2{bounds->x + bounds->width, bounds->y + bounds->height}})
        {
            const auto rootPoint = ui.mapToRoot(target, point);
            const auto mapped = rootPoint ? ui.mapFromRoot(content, *rootPoint) : std::nullopt;
            if (!mapped)
                return false;
            low.x = std::min(low.x, mapped->x);
            low.y = std::min(low.y, mapped->y);
            high.x = std::max(high.x, mapped->x);
            high.y = std::max(high.y, mapped->y);
        }
        if (!model.reveal({low.x, low.y, high.x - low.x, high.y - low.y}, margin))
            return false;
        apply();
        return true;
    }
    EventReply event(const Event& event)
    {
        if (event.type == EventType::LayoutChanged)
        {
            sync();
            return {};
        }
        if (event.type == EventType::FocusWithin)
        {
            reveal(event.related, 4);
            return {};
        }
        if (event.type == EventType::Wheel)
        {
            const double step = event.wheelUnit == WheelUnit::Lines ? options.lineStep : 1;
            const auto maximum = model.maximumOffset(), before = model.offset();
            model.setOffset(
                {static_cast<float>(std::clamp(double(before.x) - event.wheelX * step, 0.0, double(maximum.x))),
                 static_cast<float>(std::clamp(double(before.y) - event.wheelY * step, 0.0, double(maximum.y)))});
            const auto after = model.offset();
            if (changed(after - before))
                apply();
            auto residual = [step](float original, float consumed)
            {
                const double remaining = double(original) + consumed / step;
                return static_cast<float>(
                    std::clamp(remaining, std::min(0.0, double(original)), std::max(0.0, double(original))));
            };
            EventReply reply;
            reply.handled = options.chaining == ScrollChaining::Contain;
            reply.blockApplication = true;
            reply.remainingWheel =
                WheelDelta{residual(event.wheelX, after.x - before.x), residual(event.wheelY, after.y - before.y)};
            return reply;
        }
        if (event.type == EventType::KeyDown || event.type == EventType::KeyRepeat)
        {
            Vec2 delta;
            const auto view = model.viewport();
            bool recognized = true;
            switch (event.key)
            {
            case Key::Left:
                delta.x = -options.lineStep;
                break;
            case Key::Right:
                delta.x = options.lineStep;
                break;
            case Key::Up:
                delta.y = -options.lineStep;
                break;
            case Key::Down:
                delta.y = options.lineStep;
                break;
            case Key::PageUp:
                if (options.vertical)
                    delta.y = -view.height * 0.9f;
                else
                    delta.x = -view.width * 0.9f;
                break;
            case Key::PageDown:
                if (options.vertical)
                    delta.y = view.height * 0.9f;
                else
                    delta.x = view.width * 0.9f;
                break;
            case Key::Home:
            case Key::End:
            {
                auto position = model.offset();
                const auto maximum = model.maximumOffset();
                if (options.vertical || event.modifiers.control)
                    position.y = event.key == Key::Home ? 0 : maximum.y;
                if (!options.vertical || event.modifiers.control)
                    position.x = event.key == Key::Home ? 0 : maximum.x;
                if (changed(model.setOffset(position)))
                    apply();
                return {true};
            }
            default:
                recognized = false;
                break;
            }
            if (recognized)
            {
                if (changed(model.scrollBy(delta)))
                    apply();
                return {true};
            }
        }
        return {};
    }
    EventReply barEvent(bool h, const Event& event)
    {
        if (event.type == EventType::Cancel)
        {
            dragging = false;
            return {};
        }
        const auto bar = ui.bounds(h ? horizontal : vertical);
        if (!bar)
            return {};
        const float length = h ? bar->width : bar->height;
        const auto thumb = model.thumb(h, length, options.scrollbar, options.minimumThumb);
        const float start = h ? thumb.x : thumb.y, size = h ? thumb.width : thumb.height;
        const float point = h ? event.localX : event.localY;
        if (event.type == EventType::PointerDown && event.button == PointerButton::Primary && !dragging && size > 0)
        {
            if (point >= start && point < start + size)
            {
                dragging = true;
                dragHorizontal = h;
                pointer = event.pointer;
                grab = point - start;
                return {true, false, true};
            }
            Vec2 delta;
            const auto view = model.viewport();
            if (h)
                delta.x = (point < start ? -1 : 1) * view.width * 0.9f;
            else
                delta.y = (point < start ? -1 : 1) * view.height * 0.9f;
            if (changed(model.scrollBy(delta)))
                apply();
            return {true};
        }
        if (dragging && dragHorizontal == h && event.pointer == pointer)
        {
            if (event.type == EventType::PointerMove)
            {
                const float travel = length - size;
                auto position = model.offset();
                const auto maximum = model.maximumOffset();
                const float value =
                    travel > 0 ? std::clamp((point - grab) / travel, 0.0f, 1.0f) * (h ? maximum.x : maximum.y) : 0;
                if (h)
                    position.x = value;
                else
                    position.y = value;
                if (changed(model.setOffset(position)))
                    apply();
                return {true};
            }
            if (event.type == EventType::PointerUp && event.button == PointerButton::Primary)
            {
                dragging = false;
                return {true};
            }
        }
        return {};
    }
    struct RootLayout final : Layout
    {
        ScrollOptions options;
        explicit RootLayout(ScrollOptions value) : options(value) {}
        Size measure(std::span<const LayoutItem>, Constraints c) const override
        {
            return c.constrain(options.preferredSize);
        }
        std::vector<Bounds> arrange(std::span<const LayoutItem> items, Size area) const override
        {
            if (items.size() != 3)
                throw std::logic_error("Scroll view internal children were removed/hidden");
            const float width = std::max(0.0f, area.width - (options.vertical ? options.scrollbar : 0));
            const float height = std::max(0.0f, area.height - (options.horizontal ? options.scrollbar : 0));
            return {{0, 0, width, height},
                    {width, 0, options.vertical ? std::min(area.width, options.scrollbar) : 0, height},
                    {0, height, width, options.horizontal ? std::min(area.height, options.scrollbar) : 0}};
        }
    };
    struct ContentLayout final : Layout
    {
        ScrollOptions options;
        explicit ContentLayout(ScrollOptions value) : options(value) {}
        Size measure(std::span<const LayoutItem>, Constraints c) const override
        {
            return c.constrain(options.preferredSize);
        }
        std::vector<Bounds> arrange(std::span<const LayoutItem> items, Size area) const override
        {
            if (items.size() != 1)
                throw std::logic_error("Scroll view content container was removed/hidden");
            const auto extent = items[0].measure(
                {{}, {options.horizontal ? infinity : area.width, options.vertical ? infinity : area.height}});
            return {{0, 0, std::max(area.width, extent.width), std::max(area.height, extent.height)}};
        }
    };
    struct BarPainter final : WidgetPainter
    {
        std::shared_ptr<State> state;
        bool horizontal;
        BarPainter(std::shared_ptr<State> value, bool h) : state(std::move(value)), horizontal(h) {}
        void paint(PaintContext& p) override
        {
            p.fillRect(p.bounds(), p.theme().background);
            const auto b = p.bounds();
            const auto thumb = state->model.thumb(horizontal, horizontal ? b.width : b.height, state->options.scrollbar,
                                                  state->options.minimumThumb);
            if (thumb.width > 0 && thumb.height > 0)
                p.fillRect(thumb, (state->dragging && state->dragHorizontal == horizontal) || p.hovered()
                                      ? p.theme().accent
                                      : p.theme().border);
        }
    };
};
ScrollView::ScrollView(Context& context, WidgetId parent, ScrollOptions options)
{
    validate(options);
    state_ = std::make_shared<State>(context, options);
    auto state = state_;
    try
    {
        Widget root;
        root.parent = parent;
        root.clipChildren = true;
        root.focusable = options.focusable;
        root.painter = std::make_shared<SurfacePainter>(SurfaceRole::Panel);
        root.layout = std::make_shared<State::RootLayout>(options);
        root.onEvent = [state](Context&, WidgetId, const Event& e) { return state->event(e); };
        state->root = context.add(root);
        Widget viewport;
        viewport.parent = state->root;
        viewport.clipChildren = true;
        viewport.layout = std::make_shared<State::ContentLayout>(options);
        state->viewport = context.add(viewport);
        Widget content;
        content.parent = state->viewport;
        state->content = context.add(content);
        Widget bar;
        bar.parent = state->root;
        bar.painter = std::make_shared<State::BarPainter>(state, false);
        bar.onEvent = [state](Context&, WidgetId, const Event& e) { return state->barEvent(false, e); };
        state->vertical = context.add(bar);
        bar.painter = std::make_shared<State::BarPainter>(state, true);
        bar.onEvent = [state](Context&, WidgetId, const Event& e) { return state->barEvent(true, e); };
        state->horizontal = context.add(bar);
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
ScrollView::~ScrollView()
{
    try
    {
        state_->ui.remove(state_->root);
    }
    catch (...)
    {
    }
}
WidgetId ScrollView::id() const
{
    return state_->root;
}
WidgetId ScrollView::content() const
{
    return state_->content;
}
WidgetId ScrollView::viewportId() const
{
    return state_->viewport;
}
WidgetId ScrollView::horizontalBar() const
{
    return state_->horizontal;
}
WidgetId ScrollView::verticalBar() const
{
    return state_->vertical;
}
void ScrollView::setContentLayout(std::shared_ptr<const Layout> layout)
{
    state_->ui.setLayout(content(), std::move(layout));
}
void ScrollView::setContentSize(Size size)
{
    Constraints{}.constrain(size);
    state_->ui.setLayout(content(), {});
    state_->ui.setBounds(content(), {0, 0, size.width, size.height});
}
Vec2 ScrollView::offset() const
{
    return state_->model.offset();
}
Vec2 ScrollView::maximumOffset() const
{
    return state_->model.maximumOffset();
}
bool ScrollView::setOffset(Vec2 value)
{
    if (!state_->ui.contains(id()))
        return false;
    if (!changed(state_->model.setOffset(value)))
        return false;
    state_->apply();
    return true;
}
bool ScrollView::scrollBy(Vec2 delta)
{
    if (!state_->ui.contains(id()))
        return false;
    if (!changed(state_->model.scrollBy(delta)))
        return false;
    state_->apply();
    return true;
}
bool ScrollView::scrollToVisible(WidgetId descendant, float margin)
{
    return state_->reveal(descendant, margin);
}
} // namespace vectorgl::ui
