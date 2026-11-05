#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vectorgl/ui/containers.hpp>

using namespace vectorgl;
using namespace vectorgl::ui;
namespace
{
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
void near(float value, float expected)
{
    if (std::abs(value - expected) >= 0.02f)
        throw std::runtime_error("container geometry mismatch: " + std::to_string(value) + " expected " +
                                 std::to_string(expected));
}
Event pointer(EventType type, float x, float y)
{
    Event e;
    e.type = type;
    e.x = x;
    e.y = y;
    e.button = PointerButton::Primary;
    return e;
}
Event key(Key value)
{
    Event e;
    e.type = EventType::KeyDown;
    e.key = value;
    return e;
}
Event wheel(float x, float y, float dx, float dy, WheelUnit unit = WheelUnit::Lines)
{
    auto e = pointer(EventType::Wheel, x, y);
    e.wheelX = dx;
    e.wheelY = dy;
    e.wheelUnit = unit;
    return e;
}
void modelGeometry()
{
    ScrollModel model(true, true);
    model.setGeometry({100, 80}, {400, 300});
    near(model.maximumOffset().x, 300);
    near(model.maximumOffset().y, 220);
    const auto consumed = model.scrollBy({500, -5});
    near(consumed.x, 300);
    near(consumed.y, 0);
    model.setOffset({0, 0});
    require(model.reveal({200, 150, 20, 20}, 0), "reveal did not move");
    near(model.offset().x, 120);
    near(model.offset().y, 90);
    auto thumb = model.thumb(true, 100, 12, 20);
    near(thumb.width, 25);
    near(thumb.x, 30);
    model.setGeometry({100, 80}, {50, 40});
    near(model.offset().x, 0);
    near(model.offset().y, 0);
    require(model.thumb(true, 100, 12).width == 0, "nonoverflowing view displayed a thumb");
}
void splitInteraction()
{
    Context ui;
    int completed = 0, cancelled = 0;
    {
        SplitOptions options;
        options.fraction = 0.5f;
        options.firstMinimum = 100;
        options.secondMinimum = 80;
        options.firstMaximum = 180;
        SplitPanel split(ui, {}, options);
        split.setOnDragFinished(
            [&](bool cancel)
            {
                if (cancel)
                    ++cancelled;
                else
                    ++completed;
            });
        ui.layout(split.id(), {0, 0, 400, 200});
        near(split.firstExtent(), 180);
        near(ui.bounds(split.second())->width, 212);
        ui.dispatch(pointer(EventType::PointerDown, 184, 40));
        require(ui.captured() == split.divider(), "split capture failed");
        ui.dispatch(pointer(EventType::PointerMove, 40, 40));
        ui.layout(split.id(), {0, 0, 400, 200});
        near(split.firstExtent(), 100);
        ui.dispatch(pointer(EventType::PointerUp, -50, 40));
        require(completed == 1 && !ui.captured(), "split release outside failed");
        ui.dispatch(pointer(EventType::PointerUp, -50, 40));
        require(completed == 1, "split completed twice");
        const float before = split.fraction();
        ui.dispatch(pointer(EventType::PointerDown, split.firstExtent() + 4, 40));
        ui.dispatch(pointer(EventType::PointerMove, 150, 40));
        ui.layout(split.id(), {0, 0, 400, 200});
        Event lost;
        lost.type = EventType::WindowFocusLost;
        ui.dispatch(lost);
        require(cancelled == 1 && !ui.captured(), "split focus loss did not cancel");
        near(split.fraction(), before);
        ui.layout(split.id(), {0, 0, 400, 200});
        ui.focus(split.divider());
        ui.dispatch(key(Key::End));
        ui.layout(split.id(), {0, 0, 400, 200});
        near(split.firstExtent(), 180);
        ui.dispatch(key(Key::Home));
        ui.layout(split.id(), {0, 0, 400, 200});
        near(split.firstExtent(), 100);
        ui.layout(split.id(), {0, 0, 120, 200});
        near(ui.bounds(split.first())->width, 100);
        near(ui.bounds(split.second())->width, 80);
        ui.layout(split.id(), {0, 0, 400, 200});
        ui.dispatch(pointer(EventType::PointerDown, split.firstExtent() + 4, 40));
        require(ui.captured() == split.divider(), "split recapture failed");
    }
    require(!ui.captured() && cancelled == 2, "split destruction stranded capture");
    SplitOptions vertical;
    vertical.axis = SplitAxis::Vertical;
    vertical.firstMinimum = 40;
    vertical.secondMinimum = 40;
    SplitPanel split(ui, {}, vertical);
    ui.layout(split.id(), {0, 0, 200, 200});
    ui.focus(split.divider());
    const float before = split.firstExtent();
    ui.dispatch(key(Key::Down));
    ui.layout(split.id(), {0, 0, 200, 200});
    require(split.firstExtent() > before, "vertical split keyboard movement failed");
}
void scrollingAndFocus()
{
    Context ui;
    ScrollView view(ui);
    view.setContentSize({228, 600});
    Widget child;
    child.parent = view.content();
    child.bounds = {10, 500, 70, 24};
    child.focusable = true;
    const auto target = ui.add(child);
    ui.layout(view.id(), {0, 0, 240, 180});
    near(view.maximumOffset().y, 420);
    require(ui.dispatch(wheel(20, 20, 0, -1)), "wheel escaped scroll view");
    near(view.offset().y, 40);
    ui.layout(view.id(), {0, 0, 240, 180});
    require(ui.layoutStats().arrangedWidgets == 0, "scroll translation invalidated logical layout");
    ui.focus(target);
    require(view.offset().y > 300, "focus did not reveal offscreen descendant");
    require(ui.hitTest({20, 160}) == target, "scrolled hit coordinates were stale");
    require(ui.hitTest({20, -10}) != target, "clipped item accepted outside input");
    child.onEvent = [](Context&, WidgetId, const Event& e)
    { return EventReply{e.type == EventType::KeyDown && e.key == Key::Down}; };
    child.bounds.y = 400;
    auto editor = ui.add(child);
    ui.layout(view.id(), {0, 0, 240, 180});
    ui.focus(editor);
    const float before = view.offset().y;
    ui.dispatch(key(Key::Down));
    near(view.offset().y, before);
    ui.focus(view.id());
    ui.dispatch(key(Key::Home));
    near(view.offset().y, 0);
    ui.dispatch(key(Key::PageDown));
    require(view.offset().y > 100, "keyboard page scroll failed");
    view.setOffset({0, 420});
    require(ui.dispatch(wheel(20, 20, 0, -1)), "boundary wheel reached application commands");
    near(view.offset().y, 420);
    view.setContentSize({228, 100});
    ui.layout(view.id(), {0, 0, 240, 180});
    near(view.offset().y, 0);
    near(view.maximumOffset().y, 0);
}
void nestedResiduals()
{
    Context ui;
    ScrollOptions both;
    both.horizontal = true;
    ScrollView outer(ui, {}, both);
    outer.setContentSize({500, 1000});
    ScrollView inner(ui, outer.content());
    inner.setContentSize({188, 200});
    ui.setBounds(inner.id(), {0, 0, 200, 100});
    ui.setTransform(inner.id(), Mat3x3::translation(20, 20));
    ui.layout(outer.id(), {0, 0, 240, 180});
    inner.setOffset({0, 90});
    ui.dispatch(wheel(30, 30, -2, -1));
    near(inner.offset().y, 100);
    near(outer.offset().x, 80);
    near(outer.offset().y, 30);
    outer.setOffset({0, 0});
    inner.setOffset({0, 90});
    ui.dispatch(wheel(30, 30, 0, -15, WheelUnit::Pixels));
    near(inner.offset().y, 100);
    near(outer.offset().y, 5);
    outer.setOffset({0, 0});
    const auto before = outer.offset();
    {
        ScrollOptions contained;
        contained.chaining = ScrollChaining::Contain;
        ScrollView owned(ui, outer.content(), contained);
        ui.setBounds(owned.id(), {0, 0, 200, 100});
        ui.setTransform(owned.id(), Mat3x3::translation(20, 20));
        owned.setContentSize({188, 100});
        ui.layout(outer.id(), {0, 0, 240, 180});
        ui.dispatch(wheel(30, 30, 0, -1));
        near(outer.offset().y, before.y);
    }
    // Focus reveal proceeds inner-to-outer using the newly updated content transforms.
    Widget row;
    row.parent = inner.content();
    row.bounds = {10, 170, 30, 20};
    row.focusable = true;
    auto target = ui.add(row);
    ui.setTransform(inner.id(), Mat3x3::translation(20, 400));
    ui.layout(outer.id(), {0, 0, 240, 180});
    outer.setOffset({0, 0});
    inner.setOffset({0, 0});
    ui.focus(target);
    require(inner.offset().y > 0 && outer.offset().y > 0, "nested focus reveal failed");
}
void scrollbarCapture()
{
    Context ui;
    WidgetId root;
    {
        ScrollView view(ui);
        root = view.id();
        view.setContentSize({228, 600});
        ui.layout(root, {0, 0, 240, 180});
        ui.dispatch(pointer(EventType::PointerDown, 234, 5));
        require(ui.captured() == view.verticalBar(), "scroll thumb capture failed");
        ui.dispatch(pointer(EventType::PointerMove, 234, 170));
        require(view.offset().y > 350, "thumb drag did not scroll");
        ui.dispatch(pointer(EventType::PointerUp, 400, 250));
        require(!ui.captured(), "thumb release outside was lost");
        view.setOffset({0, 0});
        ui.dispatch(pointer(EventType::PointerDown, 234, 150));
        require(view.offset().y > 100 && !ui.captured(), "track page click failed");
        view.setOffset({0, 0});
        ui.dispatch(pointer(EventType::PointerDown, 234, 5));
    }
    require(!ui.contains(root) && !ui.captured(), "scroll view destruction left registrations or capture");
}
void constraintsAndFailures()
{
    Context ui;
    SplitPanel split(ui);
    LayoutHints hints;
    hints.minimum.height = 45;
    hints.maximum.height = 60;
    ui.setLayoutHints(split.first(), hints);
    ui.layout(split.id(), {0, 0, 600, 200});
    near(ui.bounds(split.first())->height, 60);
    const auto before = split.fraction();
    ui.dispatch(pointer(EventType::PointerDown, split.firstExtent() + 4, 20));
    ui.dispatch(pointer(EventType::PointerMove, 350, 20));
    ui.layout(split.id(), {0, 0, 620, 200});
    require(!ui.captured(), "resize did not cancel divider capture");
    near(split.fraction(), before);
    ui.layout(split.id(), {0, 0, 620, 200});
    ui.dispatch(pointer(EventType::PointerDown, split.firstExtent() + 4, 20));
    require(ui.dispatch(key(Key::Escape)) && !ui.captured(), "Escape did not consume/cancel split drag");

    bool rejected = false;
    try
    {
        ScrollOptions invalid;
        invalid.lineStep = 0;
        ScrollView view(ui, {}, invalid);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected, "invalid scroll options accepted");
    Widget widget;
    widget.bounds = {0, 0, 20, 20};
    widget.onEvent = [](Context&, WidgetId, const Event& e)
    {
        EventReply reply;
        if (e.type == EventType::Wheel)
            reply.remainingWheel = WheelDelta{0, -2};
        return reply;
    };
    ui.add(widget);
    rejected = false;
    try
    {
        ui.dispatch(wheel(5, 5, 0, -1));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected && !ui.captured(), "invalid residual bypassed callback failure handling");
}
} // namespace
int main()
{
    try
    {
        modelGeometry();
        splitInteraction();
        scrollingAndFocus();
        nestedResiduals();
        scrollbarCapture();
        constraintsAndFailures();
        std::cout << "Container behavior tests passed\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
