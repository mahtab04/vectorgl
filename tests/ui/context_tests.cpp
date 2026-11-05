#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <vectorgl/ui/context.hpp>

using namespace vectorgl::ui;
namespace
{
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <class Exception, class Function> void rejects(Function function)
{
    bool caught = false;
    try
    {
        function();
    }
    catch (const Exception&)
    {
        caught = true;
    }
    require(caught, "expected exception was not thrown");
}
Widget field(Bounds bounds = {0, 0, 100, 40})
{
    Widget widget;
    widget.bounds = bounds;
    widget.focusable = true;
    return widget;
}
Event key(Key value, bool shift = false, EventType type = EventType::KeyDown)
{
    Event event;
    event.type = type;
    event.key = value;
    event.modifiers.shift = shift;
    return event;
}
Event pointer(EventType type, float x, float y, std::uint64_t id = 0, PointerButton button = PointerButton::Primary)
{
    Event event;
    event.type = type;
    event.x = x;
    event.y = y;
    event.pointer = id;
    event.button = button;
    return event;
}
void identityAndFocus()
{
    Context context, other;
    const auto first = context.add(field());
    const auto second = context.add(field({110, 0, 100, 40}));
    const auto foreign = other.add(field());
    require(!context.contains(foreign) && !context.focus(foreign), "foreign ID accepted");
    auto invalid = field();
    invalid.parent = foreign;
    rejects<std::invalid_argument>([&] { context.add(invalid); });
    require(context.dispatch(key(Key::Tab)) && context.focused() == first, "initial tab failed");
    require(context.dispatch(key(Key::Tab)) && context.focused() == second, "forward tab failed");
    context.dispatch(key(Key::Tab, false, EventType::KeyUp));
    require(context.focused() == second, "Tab key up traversed twice");
    context.dispatch(key(Key::Tab, true));
    require(context.focused() == first, "reverse tab failed");
    context.setEnabled(first, false);
    require(context.focused() == second, "disabled focus owner did not transfer");
    context.setVisible(second, false);
    require(!context.focused(), "invisible focus owner retained");
    context.remove(first);
    const auto replacement = context.add(field());
    require(replacement.index == first.index && replacement != first, "slot generation was not advanced");
    require(!context.contains(first) && !context.focus(first), "stale ID accepted");
    context.remove(first);
    require(context.contains(replacement), "stale removal deleted replacement");
}
void routingAndClipping()
{
    Context context;
    int parentKeys = 0, fieldKeys = 0;
    Widget parent;
    parent.bounds = {0, 0, 80, 80};
    parent.clipChildren = true;
    parent.onEvent = [&](Context&, WidgetId, const Event& event)
    {
        if (event.type == EventType::KeyDown)
            ++parentKeys;
        if (event.type == EventType::Wheel)
        {
            require(event.wheelX == 2 && event.wheelY == -3 && event.wheelUnit == WheelUnit::Pixels,
                    "wheel axes/units changed during bubbling");
            return EventReply{true};
        }
        return EventReply{};
    };
    auto root = context.add(parent);
    auto child = field({40, 0, 100, 40});
    child.parent = root;
    child.onEvent = [&](Context&, WidgetId, const Event& event)
    {
        if (event.type == EventType::KeyDown && event.key == Key::Delete)
        {
            ++fieldKeys; // No text mutation: still consumed.
            return EventReply{true};
        }
        return EventReply{};
    };
    auto id = context.add(child);
    context.dispatch(pointer(EventType::PointerDown, 100, 20));
    require(!context.focused(), "child hit escaped ancestor clip");
    context.dispatch(pointer(EventType::PointerDown, 50, 20));
    require(context.focused() == id, "pointer did not focus child");
    require(context.dispatch(key(Key::Delete)) && fieldKeys == 1 && parentKeys == 0, "Delete escaped field");
    require(!context.dispatch(key(Key::A)) && parentKeys == 1, "unhandled key did not bubble");
    Event wheel = pointer(EventType::Wheel, 50, 20);
    wheel.wheelX = 2;
    wheel.wheelY = -3;
    wheel.wheelUnit = WheelUnit::Pixels;
    require(context.dispatch(wheel), "wheel did not bubble");
    context.setEnabled(root, false);
    require(!context.focused() && !context.dispatch(pointer(EventType::PointerDown, 50, 20)),
            "disabled ancestor allowed child input");
}
void captureLifecycle()
{
    Context context;
    int ups = 0, moves = 0, cancellations = 0;
    CancelReason reason = CancelReason::Explicit;
    auto widget = field();
    widget.onEvent = [&](Context&, WidgetId, const Event& event)
    {
        if (event.type == EventType::PointerDown)
            return EventReply{true, true, true};
        if (event.type == EventType::PointerUp)
            ++ups;
        if (event.type == EventType::PointerMove)
            ++moves;
        if (event.type == EventType::Cancel)
        {
            ++cancellations;
            reason = event.reason;
        }
        return EventReply{true};
    };
    auto id = context.add(widget);
    context.dispatch(pointer(EventType::PointerDown, 10, 10, 7));
    require(context.captured() == id, "capture not granted");
    context.dispatch(pointer(EventType::PointerMove, 500, 500, 8));
    require(moves == 0, "capture stole another pointer");
    context.dispatch(pointer(EventType::PointerMove, 500, 500, 7));
    context.dispatch(pointer(EventType::PointerUp, 500, 500, 7, PointerButton::Secondary));
    require(context.captured() == id, "different button ended capture");
    context.dispatch(pointer(EventType::PointerUp, 500, 500, 7));
    require(!context.captured() && ups == 2 && moves == 1 && cancellations == 0, "captured release failed");
    require(!context.dispatch(pointer(EventType::PointerUp, 500, 500, 7)), "late release reached old owner");
    for (auto type : {EventType::PointerLeave, EventType::WindowFocusLost})
    {
        context.dispatch(pointer(EventType::PointerDown, 10, 10));
        Event event;
        event.type = type;
        context.dispatch(event);
        require(!context.captured(), "capture survived window cancellation");
        require(reason ==
                    (type == EventType::PointerLeave ? CancelReason::PointerLeftWindow : CancelReason::WindowFocusLost),
                "wrong cancellation reason");
        context.dispatch(event);
    }
    require(cancellations == 2 && !context.focused(), "cancellation duplicated or focus survived host loss");
    context.dispatch(pointer(EventType::PointerDown, 10, 10));
    context.setEnabled(id, false);
    require(cancellations == 3 && !context.captured(), "disabled captured widget was not cancelled");
}
void modalScopes()
{
    Context context;
    int background = 0;
    auto behind = field();
    behind.onEvent = [&](Context&, WidgetId, const Event&)
    {
        ++background;
        return EventReply{};
    };
    auto backgroundId = context.add(behind);
    context.focus(backgroundId);
    const auto root = context.add(Widget{});
    auto dialogField = field({200, 0, 100, 40});
    dialogField.parent = root;
    const auto child = context.add(dialogField);
    context.pushModal(root);
    const int before = background;
    require(context.focused() == child && !context.focus(backgroundId), "modal did not trap focus");
    require(context.dispatch(pointer(EventType::PointerDown, 10, 10)), "modal barrier did not consume outside click");
    require(context.dispatch(key(Key::Delete)) && background == before, "modal input reached background");
    context.dispatch(key(Key::Tab));
    require(context.focused() == child, "Tab escaped modal");
    context.popModal();
    require(context.focused() == backgroundId, "modal focus not restored");
    context.pushModal(root);
    context.remove(root);
    require(!context.modalRoot() && !context.contains(child) && context.focused() == backgroundId,
            "removing modal root did not clean up subtree/focus");
    rejects<std::logic_error>([&] { context.popModal(); });
}
void callbackMutationAndFailure()
{
    Context context;
    int cancelled = 0, lost = 0;
    WidgetId added;
    auto widget = field();
    widget.onEvent = [&](Context& ctx, WidgetId self, const Event& event)
    {
        if (event.type == EventType::PointerDown)
            return EventReply{true, true, true};
        if (event.type == EventType::PointerMove)
        {
            ctx.remove(self);
            require(!ctx.contains(self), "removal did not immediately invalidate ID");
            added = ctx.add(field());
            rejects<std::logic_error>([&] { ctx.dispatch(key(Key::Tab)); });
        }
        if (event.type == EventType::Cancel)
            ++cancelled;
        if (event.type == EventType::FocusLost)
            ++lost;
        return EventReply{true};
    };
    auto id = context.add(widget);
    context.dispatch(pointer(EventType::PointerDown, 10, 10));
    context.dispatch(pointer(EventType::PointerMove, 300, 300));
    require(cancelled == 1 && lost == 1 && !context.captured() && context.focused() == added,
            "callback-driven removal did not settle safely");
    auto failing = field();
    failing.onEvent = [](Context&, WidgetId, const Event& event)
    {
        if (event.type == EventType::PointerDown)
            return EventReply{true, true, true};
        if (event.type == EventType::PointerMove)
            throw std::runtime_error("simulated client failure");
        return EventReply{true};
    };
    auto bad = context.add(failing);
    context.dispatch(pointer(EventType::PointerDown, 10, 10));
    rejects<std::runtime_error>([&] { context.dispatch(pointer(EventType::PointerMove, 300, 300)); });
    require(!context.captured() && context.contains(bad), "callback exception stranded capture");
    context.remove(bad);
    require(context.dispatch(key(Key::Tab)), "context did not recover after client exception");

    int releaseCancelled = 0;
    auto failedRelease = field();
    failedRelease.onEvent = [&](Context&, WidgetId, const Event& event)
    {
        if (event.type == EventType::PointerDown)
            return EventReply{true, false, true};
        if (event.type == EventType::PointerUp)
            throw std::runtime_error("release failed");
        if (event.type == EventType::Cancel && event.reason == CancelReason::HandlerFailed)
            ++releaseCancelled;
        return EventReply{};
    };
    context.add(failedRelease);
    context.dispatch(pointer(EventType::PointerDown, 10, 10));
    rejects<std::runtime_error>([&] { context.dispatch(pointer(EventType::PointerUp, 300, 300)); });
    require(releaseCancelled == 1 && !context.captured(), "failed release skipped cancellation cleanup");
}
void stackingAndNestedScopes()
{
    Context context;
    int firstHits = 0, lastHits = 0;
    auto first = field();
    first.onEvent = [&](Context&, WidgetId, const Event& event)
    {
        if (event.type == EventType::PointerDown)
            ++firstHits;
        return EventReply{true};
    };
    auto back = context.add(first);
    auto second = field();
    second.onEvent = [&](Context&, WidgetId, const Event& event)
    {
        if (event.type == EventType::PointerDown)
            ++lastHits;
        return EventReply{true};
    };
    auto front = context.add(second);
    context.takeInvalidation();
    context.dispatch(pointer(EventType::PointerDown, 10, 10));
    require(firstHits == 0 && lastHits == 1 && context.focused() == front, "hit order disagreed with sibling order");
    require(context.takeInvalidation() != Invalidation::None, "handled input failed to request paint");
    auto nested = field({200, 0, 100, 40});
    nested.parent = front;
    auto child = context.add(nested);
    context.pushModal(front);
    context.pushModal(child);
    require(context.focused() == child && context.modalRoot() == child, "nested modal did not focus child");
    context.setVisible(front, false);
    require(!context.modalRoot() && context.focused() == back, "hidden outer modal did not unwind nested scopes");
    // A focus notification can remove its intended successor without use-after-free.
    WidgetId successor;
    auto redirect = field();
    redirect.onEvent = [&](Context& ui, WidgetId, const Event& event)
    {
        if (event.type == EventType::FocusLost)
            ui.remove(successor);
        return EventReply{};
    };
    auto source = context.add(redirect);
    successor = context.add(field());
    context.focus(source);
    context.focus(successor);
    require(!context.focused() && !context.contains(successor), "focus callback selected a removed successor");

    auto stateful = field();
    stateful.onEvent = [count = 0](Context&, WidgetId, const Event& event) mutable
    {
        if (event.type == EventType::KeyDown)
            return EventReply{++count == 2};
        return EventReply{};
    };
    auto statefulId = context.add(std::move(stateful));
    context.focus(statefulId);
    require(!context.dispatch(key(Key::A)) && context.dispatch(key(Key::A)), "mutable callback state was copied away");
}
void createModalFromCallback()
{
    Context context;
    WidgetId modal, modalField;
    auto button = field();
    button.onEvent = [&](Context& ui, WidgetId, const Event& event)
    {
        if (event.type == EventType::KeyDown && event.key == Key::Enter)
        {
            modal = ui.add(Widget{});
            auto input = field();
            input.parent = modal;
            modalField = ui.add(input);
            ui.pushModal(modal);
            return EventReply{true};
        }
        return EventReply{};
    };
    auto id = context.add(button);
    context.focus(id);
    context.dispatch(key(Key::Enter));
    require(context.modalRoot() == modal && context.focused() == modalField,
            "callback-created modal was not published/focused at the dispatch boundary");
    context.popModal();
    require(context.focused() == id, "callback-created modal lost previous focus");
}
struct FakeClock final : Clock
{
    double time = 0;
    double now() const override
    {
        return time;
    }
};
struct FakeClipboard final : Clipboard
{
    std::string value;
    ClipboardResult read() override
    {
        return {{ServiceStatus::Success, {}}, value};
    }
    ServiceResult write(std::string_view text) override
    {
        value = text;
        return {ServiceStatus::Success, {}};
    }
};
void schedulingAndValidation()
{
    auto clock = std::make_shared<FakeClock>();
    auto clipboard = std::make_shared<FakeClipboard>();
    Context context({clock, clipboard});
    require(context.takeInvalidation() == Invalidation::All && context.takeInvalidation() == Invalidation::None,
            "dirty flags did not clear");
    context.invalidate(Invalidation::Measure);
    require(context.takeInvalidation() == (Invalidation::Measure | Invalidation::Arrange | Invalidation::Paint),
            "measure invalidation did not propagate");
    int fired = 0;
    auto widget = field();
    widget.onEvent = [&](Context& ctx, WidgetId self, const Event& event)
    {
        if (event.type == EventType::Timer)
        {
            ++fired;
            ctx.requestWakeup(self, ctx.now() + 0.5);
        }
        return EventReply{};
    };
    auto id = context.add(widget);
    context.takeInvalidation();
    context.requestWakeup(id, 0.5);
    context.pumpTimers();
    require(fired == 0 && context.nextWakeup() == 0.5, "timer fired early");
    clock->time = 0.5;
    context.pumpTimers();
    require(fired == 1 && context.nextWakeup() == 1.0 && context.takeInvalidation() == Invalidation::Paint,
            "timer did not reschedule/invalidate");
    context.pumpTimers();
    require(fired == 1, "timer fired twice");
    context.setVisible(id, false);
    require(!context.nextWakeup(), "hidden control kept host awake");
    context.remove(id);
    clock->time = 2;
    context.pumpTimers();
    require(fired == 1, "removed control received Timer");
    require(context.services().clipboard->write("test").status == ServiceStatus::Success &&
                context.services().clipboard->read().text == "test",
            "injected clipboard not available");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    rejects<std::invalid_argument>([&] { context.add(field({nan, 0, 1, 1})); });
    rejects<std::invalid_argument>([&] { context.requestWakeup(id, std::numeric_limits<double>::infinity()); });
    rejects<std::invalid_argument>([&] { context.dispatch(pointer(EventType::PointerDown, nan, 0)); });
    Event internal;
    internal.type = EventType::FocusGained;
    rejects<std::invalid_argument>([&] { context.dispatch(internal); });
    Context unavailable;
    require(!unavailable.services().clipboard, "default context unexpectedly installed system clipboard");
}
void timerMutation()
{
    auto clock = std::make_shared<FakeClock>();
    Context context({clock, {}});
    WidgetId second;
    int secondFired = 0;
    auto first = field();
    first.onEvent = [&](Context& ui, WidgetId, const Event& event)
    {
        if (event.type == EventType::Timer)
            ui.setVisible(second, false);
        return EventReply{};
    };
    auto a = context.add(first);
    auto follower = field();
    follower.onEvent = [&](Context&, WidgetId, const Event& event)
    {
        if (event.type == EventType::Timer)
            ++secondFired;
        return EventReply{};
    };
    second = context.add(follower);
    context.requestWakeup(a, 0);
    context.requestWakeup(second, 0);
    context.pumpTimers();
    require(secondFired == 0 && !context.nextWakeup(), "hidden timer fired or kept host awake");
    context.setVisible(second, true);
    require(context.nextWakeup() == 0, "temporarily hidden timer lost its deadline");
    context.pumpTimers();
    require(secondFired == 1 && !context.nextWakeup(), "resumed timer did not fire once");
    context.requestWakeup(second, 2);
    context.cancelWakeup(second);
    clock->time = 3;
    context.pumpTimers();
    require(secondFired == 1, "cancelled timer fired");
}
} // namespace

int main()
{
    try
    {
        identityAndFocus();
        routingAndClipping();
        captureLifecycle();
        modalScopes();
        callbackMutationAndFailure();
        stackingAndNestedScopes();
        createModalFromCallback();
        schedulingAndValidation();
        timerMutation();
        std::cout << "UI context behavior tests passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
