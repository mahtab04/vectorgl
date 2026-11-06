#include <iostream>
#include <stdexcept>
#include <vectorgl/ui/overlays.hpp>
using namespace vectorgl;
using namespace vectorgl::ui;
namespace
{
void require(bool value, const char* error)
{
    if (!value)
        throw std::runtime_error(error);
}
Event key(Key k, EventType type = EventType::KeyDown)
{
    Event e;
    e.type = type;
    e.key = k;
    return e;
}
Event point(EventType type, float x, float y)
{
    Event e;
    e.type = type;
    e.x = x;
    e.y = y;
    e.button = PointerButton::Primary;
    return e;
}
struct TestClock final : Clock
{
    double time = 0;
    double now() const override
    {
        return time;
    }
};
void popups()
{
    Context ui;
    OverlayManager overlays(ui);
    overlays.update({0, 0, 400, 300});
    int clicks = 0, selected = -1, closed = 0;
    Button trigger(ui, {}, {"Open", {100, 40}});
    ui.layout(trigger.id(), {300, 250, 90, 40});
    ui.focus(trigger.id());
    Button underlying(ui, {}, {"Behind", {100, 40}, [&] { ++clicks; }});
    ui.layout(underlying.id(), {0, 0, 100, 40});
    PopupOptions options;
    options.items = {{"First"}, {"Unavailable", false}, {"", true, true}, {"Last"}};
    options.onSelected = [&](std::size_t i) { selected = int(i); };
    options.onClosed = [&](DismissReason) { ++closed; };
    auto popup = overlays.openPopup(trigger.id(), options);
    overlays.update({0, 0, 400, 300});
    auto b = *overlays.contentBounds(popup);
    require(b.x >= 0 && b.x + b.width <= 400 && b.y + b.height <= 250, "popup did not flip/clamp at viewport edge");
    auto modal = ui.modalRoot();
    require(modal == overlays.root(popup), "popup missing modal input barrier");
    ui.dispatch(key(Key::Down));
    ui.dispatch(key(Key::Enter));
    ui.dispatch(key(Key::Enter, EventType::KeyUp));
    require(selected == 3 && closed == 1 && !overlays.contains(popup),
            "popup navigation did not skip disabled/separator rows");
    require(ui.focused() == trigger.id() && !ui.modalRoot(), "popup did not restore focus");
    popup = overlays.openPopup(trigger.id(), options);
    overlays.update({0, 0, 400, 300});
    b = *overlays.contentBounds(popup);
    ui.dispatch(point(EventType::PointerDown, b.x + 20, b.y + 64));
    ui.dispatch(point(EventType::PointerUp, b.x + 20, b.y + 64));
    ui.dispatch(key(Key::Down));
    ui.dispatch(key(Key::Enter));
    ui.dispatch(key(Key::Enter, EventType::KeyUp));
    require(selected == 0 && !overlays.contains(popup), "disabled-row click stole popup keyboard navigation");
    popup = overlays.openPopup(trigger.id(), options);
    overlays.update({0, 0, 400, 300});
    require(ui.dispatch(point(EventType::PointerDown, 20, 20)), "outside dismiss was not consumed");
    ui.dispatch(point(EventType::PointerUp, 20, 20));
    require(!overlays.contains(popup) && clicks == 0, "outside dismiss clicked through to underlying button");
    popup = overlays.openContextMenu(trigger.id(), {399, 299}, options);
    overlays.update({0, 0, 400, 300});
    b = *overlays.contentBounds(popup);
    require(b.x + b.width <= 400 && b.y + b.height <= 300, "context menu exceeded viewport");
    ui.dispatch(key(Key::Escape));
    require(!overlays.contains(popup), "Escape did not dismiss menu");
    popup = overlays.openPopup(trigger.id(), options);
    ui.setEnabled(trigger.id(), false);
    overlays.update({0, 0, 400, 300});
    require(!overlays.contains(popup) && !ui.modalRoot(), "disabled popup owner left an active overlay");
}
void dialogsAndReopen()
{
    Context ui;
    OverlayManager overlays(ui);
    overlays.update({0, 0, 800, 600});
    Button owner(ui);
    ui.layout(owner.id(), {20, 20, 100, 44});
    ui.focus(owner.id());
    int result = -1, count = 0;
    DialogOptions options;
    options.title = "Unsaved changes";
    options.message = "Save your work?";
    options.actions = {{"Save", 1}, {"Discard", 2}, {"Cancel", 0}};
    options.defaultAction = 0;
    options.cancelAction = 2;
    options.onFinished = [&](std::optional<int> value)
    {
        ++count;
        result = value.value_or(-2);
    };
    auto dialog = overlays.openDialog(options);
    overlays.update({0, 0, 800, 600});
    for (int i = 0; i < 12; ++i)
    {
        ui.dispatch(key(Key::Tab));
        require(ui.isDescendant(ui.focused(), overlays.root(dialog)), "modal Tab escaped dialog");
    }
    ui.dispatch(point(EventType::PointerDown, 5, 5));
    ui.dispatch(point(EventType::PointerUp, 5, 5));
    require(overlays.contains(dialog) && !count, "outside click dismissed dialog");
    ui.dispatch(key(Key::Escape));
    require(result == 0 && count == 1 && ui.focused() == owner.id(), "dialog cancel/focus restoration failed");
    ui.dispatch(key(Key::Escape));
    require(count == 1, "dialog completion repeated");
    dialog = overlays.openDialog(options);
    overlays.update({0, 0, 220, 150});
    auto b = *overlays.contentBounds(dialog);
    require(b.x >= 0 && b.y >= 0 && b.x + b.width <= 220 && b.y + b.height <= 150,
            "dialog was not constrained on resize");
    overlays.close(dialog);
    require(count == 2 && result == -2, "programmatic dialog close did not complete once");
    PopupOptions popup;
    popup.items = {{"Open dialog"}};
    popup.onSelected = [&](std::size_t) { dialog = overlays.openDialog(options); };
    overlays.openPopup(owner.id(), popup);
    overlays.update({0, 0, 800, 600});
    ui.dispatch(key(Key::Enter));
    ui.dispatch(key(Key::Enter, EventType::KeyUp));
    overlays.update({0, 0, 800, 600});
    require(overlays.contains(dialog), "selection callback could not replace popup with dialog");
    overlays.close(dialog);
    require(ui.focused() == owner.id(), "replacement overlay restored stale popup focus");
}
void comboAndTooltip()
{
    auto clock = std::make_shared<TestClock>();
    Context ui({clock, {}});
    OverlayManager overlays(ui);
    overlays.update({0, 0, 500, 400});
    int changed = 0;
    ComboBox combo(ui, overlays, {}, {{{"One"}, {"Two"}}, 0, {200, 44}, [&](std::size_t) { ++changed; }});
    ui.layout(combo.id(), {20, 20, 200, 44});
    ui.focus(combo.id());
    ui.dispatch(key(Key::Down));
    overlays.update({0, 0, 500, 400});
    ui.dispatch(key(Key::End));
    ui.dispatch(key(Key::Space));
    ui.dispatch(key(Key::Space, EventType::KeyUp));
    require(combo.selected() == 1 && changed == 1 && ui.focused() == combo.id(), "ComboBox selection failed");
    combo.setSelected(0);
    require(changed == 1, "ComboBox setter emitted change");
    overlays.setTooltip(combo.id(), "Choose an item", {.5, 2, {180, 64}});
    overlays.update({0, 0, 500, 400});
    require(!overlays.tooltipVisible(), "tooltip ignored delay");
    clock->time = .5;
    ui.pumpTimers();
    require(overlays.tooltipVisible(), "tooltip timer did not reveal");
    require(ui.hitTest({30, 30}) == combo.id(), "tooltip intercepted owner input");
    clock->time = 2.5;
    ui.pumpTimers();
    overlays.update({0, 0, 500, 400});
    require(!overlays.tooltipVisible() && !ui.nextWakeup(), "tooltip expiry restarted itself");
    ui.focus({});
    overlays.update({0, 0, 500, 400});
    ui.focus(combo.id());
    overlays.update({0, 0, 500, 400});
    clock->time = 3;
    ui.pumpTimers();
    require(overlays.tooltipVisible(), "tooltip did not reset after leaving owner");
    Event lost;
    lost.type = EventType::WindowFocusLost;
    ui.dispatch(lost);
    require(!overlays.tooltipVisible() && !ui.nextWakeup(), "window focus loss left tooltip active");
    combo.open();
    overlays.update({0, 0, 500, 400});
    ui.dispatch(lost);
    overlays.update({0, 0, 500, 400});
    require(!ui.modalRoot(), "window focus loss did not close popup");
    overlays.setTooltip(combo.id(), "Choose again", {.5, 2, {180, 64}});
    ui.focus(combo.id());
    overlays.update({0, 0, 500, 400});
    clock->time = 4;
    ui.pumpTimers();
    require(overlays.tooltipVisible() && ui.dispatch(key(Key::Escape)), "Escape did not consume tooltip dismissal");
    require(!overlays.tooltipVisible() && !ui.nextWakeup(), "Escape left tooltip timer active");
}
void scrollingNestingAndFailure()
{
    Context ui;
    OverlayManager overlays(ui);
    overlays.update({0, 0, 640, 480});
    Button owner(ui);
    ui.layout(owner.id(), {10, 10, 100, 44});
    PopupOptions list;
    for (int i = 0; i < 30; ++i)
        list.items.push_back({"Row " + std::to_string(i)});
    list.maximumHeight = 160;
    const auto popup = overlays.openPopup(owner.id(), list);
    overlays.update({0, 0, 640, 480});
    const auto focused = ui.focused();
    const auto before = ui.mapToRoot(focused, {});
    const auto b = *overlays.contentBounds(popup);
    Event wheel = point(EventType::Wheel, b.x + 20, b.y + 20);
    wheel.wheelY = -2;
    require(ui.dispatch(wheel), "popup wheel was not consumed");
    overlays.update({0, 0, 640, 480});
    require(ui.mapToRoot(focused, {})->y < before->y, "popup update snapped wheel offset back to focus");
    ui.takeInvalidation();
    overlays.update({0, 0, 640, 480});
    require(ui.pendingInvalidation() == Invalidation::None, "unchanged overlay update invalidated layout/paint");
    overlays.close(popup);
    DialogOptions dialog;
    dialog.actions = {{"Choose", 1}, {"Cancel", 0}};
    dialog.defaultAction = 0;
    dialog.cancelAction = 1;
    auto parent = overlays.openDialog(dialog);
    const auto dialogFocus = ui.focused();
    auto nested = overlays.openPopup(dialogFocus, list);
    overlays.update({0, 0, 640, 480});
    overlays.close(nested);
    require(ui.modalRoot() == overlays.root(parent) && ui.focused() == dialogFocus,
            "nested popup did not restore dialog scope/focus");
    overlays.closeAll();
    list.onClosed = [](DismissReason) { throw std::runtime_error("Expected close failure"); };
    auto broken = overlays.openPopup(owner.id(), list);
    bool failed = false;
    try
    {
        overlays.close(broken);
    }
    catch (const std::runtime_error&)
    {
        failed = true;
    }
    require(failed && !overlays.contains(broken) && !ui.modalRoot() && !ui.captured(),
            "close callback failure stranded overlay scope");
    Context other;
    OverlayManager foreign(other);
    foreign.update({0, 0, 640, 480});
    Button otherOwner(other);
    list.onClosed = {};
    const auto local = overlays.openPopup(owner.id(), list);
    const auto remote = foreign.openPopup(otherOwner.id(), list);
    overlays.close(remote);
    require(overlays.contains(local), "foreign overlay ID closed a local overlay");
}
} // namespace
int main()
{
    try
    {
        popups();
        dialogsAndReopen();
        comboAndTooltip();
        scrollingNestingAndFailure();
        std::cout << "Overlay behavior tests passed\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
