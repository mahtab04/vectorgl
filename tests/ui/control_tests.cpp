#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vectorgl/canvas.hpp>
#include <vectorgl/ui/controls.hpp>

using namespace vectorgl;
using namespace vectorgl::ui;
namespace
{
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
Event key(Key k, EventType type = EventType::KeyDown, bool control = false)
{
    Event e;
    e.type = type;
    e.key = k;
    e.modifiers.control = control;
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
void input(Context& ui, std::string text)
{
    Event e;
    e.type = EventType::TextInput;
    e.text = std::move(text);
    require(ui.dispatch(e), "text input escaped field");
}
struct FakeClipboard final : Clipboard
{
    bool failRead = true, failWrite = true;
    std::string text = "paste";
    ClipboardResult read() override
    {
        return {{failRead ? ServiceStatus::Error : ServiceStatus::Success, "read result"}, text};
    }
    ServiceResult write(std::string_view value) override
    {
        if (!failWrite)
            text = value;
        return {failWrite ? ServiceStatus::Error : ServiceStatus::Success, "write result"};
    }
};
struct FakeClock final : Clock
{
    double seconds = 0;
    double now() const override
    {
        return seconds;
    }
};
struct CommaFormat final : NumberFormat
{
    std::optional<double> parse(std::string_view text) const override
    {
        std::string normalized(text);
        for (auto& c : normalized)
            if (c == ',')
                c = '.';
        return InvariantNumberFormat{}.parse(normalized);
    }
    std::string format(double value, int precision) const override
    {
        auto result = InvariantNumberFormat{}.format(value, precision);
        for (auto& c : result)
            if (c == '.')
                c = ',';
        return result;
    }
};
void activation()
{
    Context ui;
    int clicks = 0;
    Button button(ui, {}, {"Apply", {120, 44}, [&] { ++clicks; }});
    ui.layout(button.id(), {10, 10, 120, 44});
    ui.dispatch(point(EventType::PointerDown, 20, 20));
    require(ui.captured() == button.id(), "button failed to capture");
    ui.dispatch(point(EventType::PointerMove, 300, 20));
    ui.dispatch(point(EventType::PointerUp, 300, 20));
    require(clicks == 0 && !ui.captured(), "release outside activated button");
    ui.dispatch(point(EventType::PointerDown, 20, 20));
    ui.dispatch(point(EventType::PointerMove, 300, 20));
    ui.dispatch(point(EventType::PointerMove, 20, 20));
    ui.dispatch(point(EventType::PointerUp, 20, 20));
    ui.dispatch(point(EventType::PointerUp, 20, 20));
    require(clicks == 1, "button activation was not exactly once");
    ui.focus(button.id());
    ui.dispatch(key(Key::Space));
    ui.dispatch(key(Key::Space, EventType::KeyRepeat));
    require(clicks == 1, "key repeat activated button");
    ui.dispatch(key(Key::Space, EventType::KeyUp));
    require(clicks == 2, "keyboard release did not activate");
    ui.dispatch(key(Key::Enter));
    ui.dispatch(key(Key::Escape));
    ui.dispatch(key(Key::Enter, EventType::KeyUp));
    require(clicks == 2, "cancelled keyboard press activated");
    ui.dispatch(point(EventType::PointerDown, 20, 20));
    ui.setEnabled(button.id(), false);
    ui.dispatch(point(EventType::PointerUp, 20, 20));
    require(clicks == 2 && !ui.captured() && !ui.focused(), "disabled button retained input state");
    CheckBox toggle(ui, {}, {"Visible", false, CheckStyle::Toggle});
    ui.focus(toggle.id());
    ui.dispatch(key(Key::Space));
    ui.dispatch(key(Key::Space, EventType::KeyUp));
    require(toggle.checked(), "toggle keyboard activation failed");
    int changed = 0;
    toggle.setOnChanged([&](bool) { ++changed; });
    toggle.setChecked(false);
    require(!changed, "programmatic check setter emitted action");

    std::unique_ptr<Button> disposable;
    disposable =
        std::make_unique<Button>(ui, WidgetId{}, ButtonOptions{"Remove", {120, 44}, [&] { disposable.reset(); }});
    ui.focus(disposable->id());
    ui.dispatch(key(Key::Enter));
    ui.dispatch(key(Key::Enter, EventType::KeyUp));
    require(!disposable && !ui.captured(), "self-removal callback failed");
}
void editing()
{
    Canvas canvas;
    auto clipboard = std::make_shared<FakeClipboard>();
    Context ui({{}, clipboard});
    int changes = 0, commits = 0, cancels = 0, failures = 0;
    TextFieldOptions options;
    options.value = "Start";
    options.validate = [](std::string_view value) { return value.empty() ? "Required" : ""; };
    options.onEdit = [&](const EditEvent& e)
    {
        if (e.kind == EditKind::Changed)
            ++changes;
        if (e.kind == EditKind::Committed)
            ++commits;
        if (e.kind == EditKind::Cancelled)
            ++cancels;
    };
    options.onServiceError = [&](const ServiceResult&) { ++failures; };
    TextField field(ui, canvas, {}, options);
    ui.focus(field.id());
    field.selectAll();
    ui.dispatch(key(Key::X, EventType::KeyDown, true));
    require(field.text() == "Start" && !changes && failures == 1, "failed cut changed buffer");
    ui.dispatch(key(Key::V, EventType::KeyDown, true));
    require(field.text() == "Start" && !changes && failures == 2, "failed paste changed buffer");
    // Selection survives both failures: successful paste must replace the entire value.
    clipboard->failRead = false;
    ui.dispatch(key(Key::V, EventType::KeyDown, true));
    require(field.text() == "paste" && changes == 1, "failed service changed selection");
    ui.dispatch(key(Key::Enter));
    ui.dispatch(key(Key::Enter, EventType::KeyRepeat));
    ui.dispatch(key(Key::Enter));
    require(commits == 1 && field.committedText() == "paste", "commit completion repeated");
    field.selectAll();
    input(ui, "\xCE\xA9\xF0\x9F\x98\x80");
    ui.dispatch(key(Key::Backspace));
    require(field.text() == "\xCE\xA9", "codepoint backspace failed");
    ui.dispatch(key(Key::Escape));
    require(field.text() == "paste" && cancels == 1, "Escape did not restore committed value");
    field.selectAll();
    ui.dispatch(key(Key::Backspace));
    require(field.error() == "Required" && !field.commit(), "invalid intermediate buffer committed");
    require(field.text().empty(), "validation rejected intermediate text");
    ui.focus({});
    require(field.text() == "paste" && cancels == 2, "invalid blur did not cancel");
    ui.focus(field.id());
    field.selectAll();
    input(ui, "New");
    ui.focus({});
    require(field.committedText() == "New" && commits == 2, "valid blur did not commit");
    ui.focus(field.id());
    ui.dispatch(key(Key::End));
    require(ui.dispatch(key(Key::Delete)), "no-op Delete escaped to application");
    clipboard->failWrite = false;
    field.selectAll();
    ui.dispatch(key(Key::C, EventType::KeyDown, true));
    require(clipboard->text == "New" && field.text() == "New", "copy mutated field");
    ui.dispatch(key(Key::X, EventType::KeyDown, true));
    require(field.text().empty(), "successful cut did not delete");
    field.cancel();
    options.blurPolicy = BlurPolicy::Cancel;
    TextField rollback(ui, canvas, {}, options);
    ui.focus(rollback.id());
    rollback.selectAll();
    input(ui, "Other");
    ui.focus({});
    require(rollback.text() == "Start", "cancel-on-blur policy failed");
}
void numeric()
{
    Canvas canvas;
    Context ui;
    int commits = 0, cancels = 0;
    NumericOptions options;
    options.value = 4;
    options.minimum = -10;
    options.maximum = 10;
    options.step = .25;
    options.onEdit = [&](const NumberEditEvent& e)
    {
        if (e.kind == EditKind::Committed)
            ++commits;
        if (e.kind == EditKind::Cancelled)
            ++cancels;
    };
    NumericEditor field(ui, canvas, {}, options);
    ui.layout(field.id(), {0, 0, 320, 44});
    ui.focus(field.fieldId());
    ui.dispatch(key(Key::A, EventType::KeyDown, true));
    input(ui, "-");
    ui.dispatch(key(Key::Enter));
    require(field.value() == 4 && field.text() == "-" && !commits, "numeric intermediate was committed/rejected");
    require(!field.commit() && field.editing() && !field.error().empty(),
            "numeric application commit bypassed validation");
    ui.dispatch(key(Key::Escape));
    require(field.text() == "4.00" && cancels == 1, "numeric cancel failed");
    ui.dispatch(key(Key::A, EventType::KeyDown, true));
    input(ui, "7.25");
    ui.dispatch(key(Key::Enter));
    require(field.value() == 7.25 && commits == 1, "numeric commit failed");
    ui.dispatch(key(Key::Up));
    require(field.value() == 7.5 && commits == 2, "numeric keyboard stepping failed");
    field.setValue(10);
    require(!field.step(1) && field.value() == 10, "numeric edge step escaped range");
    ui.dispatch(key(Key::A, EventType::KeyDown, true));
    input(ui, "100");
    ui.focus({});
    require(field.value() == 10 && field.text() == "10.00" && cancels == 2, "out-of-range blur failed");
    InvariantNumberFormat format;
    for (auto invalid : {"", "-", "NaN", "inf", "2abc", " 2", "2,4"})
        require(!format.parse(invalid), "invariant parse accepted invalid input");
    require(format.parse("1.25e2") == 125 && format.format(1.5, 2) == "1.50", "invariant number format failed");
    bool rejected = false;
    try
    {
        field.setValue(std::nan(""));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected && field.value() == 10, "invalid numeric setter changed state");
    options.format = std::make_shared<CommaFormat>();
    options.value = 1.25;
    options.onEdit = {};
    NumericEditor localized(ui, canvas, {}, options);
    require(localized.text() == "1,25", "injected format policy was ignored");
    ui.focus(localized.fieldId());
    ui.dispatch(key(Key::A, EventType::KeyDown, true));
    input(ui, "2,50");
    ui.dispatch(key(Key::Enter));
    require(localized.value() == 2.5, "injected parse policy was ignored");
}
void timersAndRemoval()
{
    Canvas canvas;
    auto clock = std::make_shared<FakeClock>();
    Context ui({clock, {}});
    int commits = 0;
    TextFieldOptions options;
    options.onEdit = [&](const EditEvent& e)
    {
        if (e.kind == EditKind::Committed)
            ++commits;
    };
    TextField field(ui, canvas, {}, options);
    ui.focus(field.id());
    require(ui.nextWakeup() == 0.5, "focused field did not schedule caret wakeup");
    clock->seconds = 0.5;
    ui.pumpTimers();
    require(ui.nextWakeup() == 1.0, "caret timer did not reschedule");
    input(ui, "edited");
    ui.setEnabled(field.id(), false);
    require(!ui.nextWakeup() && !ui.focused() && commits == 1, "disabled field left a caret timer/edit session");
    ui.setEnabled(field.id(), true);
    ui.focus(field.id());
    field.setOnEdit(
        [&](const EditEvent& e)
        {
            if (e.kind == EditKind::Changed)
                ui.remove(field.id());
        });
    input(ui, "x");
    require(!ui.contains(field.id()) && !ui.nextWakeup() && !ui.captured(),
            "removal from edit callback stranded state");
}
} // namespace
int main()
{
    try
    {
        activation();
        editing();
        numeric();
        timersAndRemoval();
        std::cout << "Control behavior tests passed\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
