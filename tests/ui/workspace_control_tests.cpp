#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vectorgl/canvas.hpp>
#include <vectorgl/ui/workspace_controls.hpp>
using namespace vectorgl;
using namespace vectorgl::ui;
namespace
{
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
Event key(Key key, EventType type = EventType::KeyDown)
{
    Event e;
    e.type = type;
    e.key = key;
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
    require(ui.dispatch(e), "input escaped field");
}
void ellipsisAndPrimitives()
{
    auto measure = [](std::string_view text)
    {
        float count = 0;
        for (unsigned char c : text)
            if ((c & 0xc0) != 0x80)
                ++count;
        return count;
    };
    const std::string unicode = "A\xCE\xA9\xF0\x9F\x98\x80"
                                "BC";
    require(ellipsizeText(unicode, 5, measure) == unicode, "ellipsis changed fitting text");
    require(ellipsizeText(unicode, 3, measure) == "A\xCE\xA9\xE2\x80\xA6", "ellipsis split a UTF-8 codepoint");
    require(ellipsizeText(unicode, .5f, measure).empty(), "ellipsis overflowed tiny width");
    require(ellipsizeText("a\n\xff", 10, measure) == "a\xEF\xBF\xBD", "ellipsis did not normalize single-line UTF-8");
    bool rejected = false;
    try
    {
        ellipsizeText("text", 10, [](std::string_view) { return std::nanf(""); });
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected, "invalid text metrics were accepted");
    Canvas canvas;
    Context ui;
    Button behind(ui);
    ui.layout(behind.id(), {0, 0, 160, 44});
    Label label(ui, canvas, {}, LabelOptions{"Label"});
    ui.layout(label.id(), {0, 0, 160, 24});
    require(ui.measure(label.id()).width == 160, "headless label fallback measure changed");
    Separator line(ui);
    ui.layout(line.id(), {0, 0, 160, 1});
    require(ui.hitTest({20, 10}) == behind.id() && ui.hitTest({20, 0}) == behind.id(),
            "display-only primitives intercepted input");
    require(!ui.focus(label.id()) && !ui.focus(line.id()), "display-only primitive took focus");
}
void tabs()
{
    Context ui;
    int changes = 0;
    TabBar bar(ui, {}, {{{"One"}, {"Disabled", false}, {"Three"}}, 0, {100, 40}, [&](std::size_t) { ++changes; }});
    Button next(ui);
    ui.layout(bar.id(), {0, 0, 300, 40});
    ui.layout(next.id(), {0, 60, 100, 44});
    ui.focus(bar.tabId(0));
    ui.dispatch(key(Key::Tab));
    require(ui.focused() == next.id(), "Tab visited every tab instead of one roving stop");
    ui.focus(bar.tabId(0));
    ui.dispatch(key(Key::Right));
    require(bar.selected() == 2 && ui.focused() == bar.tabId(2) && changes == 1,
            "tab arrows did not skip disabled item");
    ui.dispatch(key(Key::Right));
    require(bar.selected() == 0 && changes == 2, "tab arrows did not wrap");
    ui.dispatch(key(Key::End));
    require(bar.selected() == 2, "End did not select last enabled tab");
    bar.setSelected(0);
    require(changes == 3, "tab setter emitted user callback");
    ui.focus(bar.tabId(0));
    bar.setTabEnabled(0, false);
    require(bar.selected() == 2 && ui.focused() == bar.tabId(2),
            "disabling selected tab did not choose next enabled tab");
    bar.setTabEnabled(2, false);
    require(!bar.selected() && ui.focused() == next.id(), "all-disabled tab bar stranded focus");
    bar.setTabEnabled(0, true);
    ui.focus(bar.tabId(0));
    ui.dispatch(key(Key::Space));
    ui.dispatch(key(Key::Space, EventType::KeyUp));
    require(bar.selected() == 0, "re-enabled tab could not be selected");
    ui.focus(next.id());
    bar.setTabEnabled(2, true);
    ui.dispatch(point(EventType::PointerDown, 250, 20));
    ui.dispatch(point(EventType::PointerUp, 250, 20));
    require(bar.selected() == 2 && ui.focused() == bar.tabId(2), "pointer activation of non-roving tab failed");
}
void pagesAndCollapse()
{
    Canvas canvas;
    Context ui;
    TabView view(ui, {}, {{{"Properties"}, {"History"}}, 0});
    TextField field(ui, canvas, view.page(0));
    Button history(ui, view.page(1));
    ui.layout(view.id(), {0, 0, 320, 240});
    ui.focus(field.id());
    input(ui, "Draft");
    require(ui.nextWakeup().has_value(), "field caret did not start");
    view.setSelected(1);
    require(ui.focused() == view.tabId(1) && !ui.nextWakeup() && !ui.isAvailable(field.id()),
            "page switch did not remove hidden editor from focus/timers");
    ui.layout(view.id(), {0, 0, 320, 240});
    ui.dispatch(key(Key::Tab));
    require(ui.focused() == history.id(), "hidden page remained in Tab traversal");
    view.setSelected(0);
    require(field.committedText() == "Draft", "page hiding did not apply field blur policy");
    int callbacks = 0, cancelled = 0;
    CollapsibleSection section(ui, {}, {"Advanced", true, {240, 44}, [&](bool) { ++callbacks; }});
    Widget captured;
    captured.parent = section.content();
    captured.bounds = {0, 0, 100, 40};
    captured.focusable = true;
    captured.onEvent = [&](Context&, WidgetId, const Event& e)
    {
        if (e.type == EventType::Cancel)
            ++cancelled;
        return EventReply{e.type == EventType::PointerDown, e.type == EventType::PointerDown,
                          e.type == EventType::PointerDown};
    };
    const auto child = ui.add(captured);
    ui.layout(section.id(), {340, 0, 240, 120});
    const auto p = *ui.mapToRoot(child, {10, 10});
    ui.dispatch(point(EventType::PointerDown, p.x, p.y));
    require(ui.captured() == child, "section child did not capture");
    section.setExpanded(false);
    require(!ui.captured() && cancelled == 1 && ui.focused() == section.headerId() && !ui.isAvailable(child),
            "collapse did not move focus/cancel descendant capture");
    require(callbacks == 0, "section setter emitted callback");
    ui.dispatch(key(Key::Right));
    require(section.expanded() && callbacks == 1, "header Right did not expand");
    ui.dispatch(key(Key::Left));
    require(!section.expanded() && callbacks == 2, "header Left did not collapse");
    ui.dispatch(key(Key::Tab));
    require(ui.focused() != child, "collapsed child remained in Tab traversal");
    require(ui.measure(section.id()).height == 44, "collapsed content still contributed intrinsic height");
}
void search()
{
    Canvas canvas;
    Context ui;
    int changed = 0;
    SearchField search(ui, canvas, {}, {{}, "Find", {240, 44}, [&](std::string_view) { ++changed; }});
    ui.layout(search.id(), {0, 0, 240, 44});
    require(!ui.isAvailable(search.clearId()), "empty search left clear action enabled");
    ui.focus(search.fieldId());
    input(ui, "\xCE\xA9");
    require(search.query() == "\xCE\xA9" && changed == 1 && ui.isAvailable(search.clearId()),
            "live Unicode search did not notify");
    ui.dispatch(key(Key::Enter));
    require(changed == 1, "search commit duplicated query callback");
    input(ui, "x");
    ui.dispatch(key(Key::Escape));
    require(search.query() == "\xCE\xA9" && changed == 3, "search Escape did not restore committed query");
    ui.focus(search.clearId());
    ui.dispatch(key(Key::Space));
    ui.dispatch(key(Key::Space, EventType::KeyUp));
    require(search.query().empty() && changed == 4 && ui.focused() == search.fieldId() &&
                !ui.isAvailable(search.clearId()),
            "search clear activation failed");
    search.clear();
    require(changed == 4, "empty search clear duplicated notification");
    search.setQuery("programmatic");
    require(changed == 4 && search.query() == "programmatic", "search setter emitted notification");
}
void callbackSelection()
{
    Context ui;
    int changes = 0;
    bool stale = false;
    TabView* current = nullptr;
    TabView view(ui, {},
                 {{{"Zero"}, {"One"}, {"Two"}},
                  2,
                  {120, 40},
                  [&](std::size_t index)
                  {
                      ++changes;
                      if (current && current->selected() != index)
                          stale = true;
                  }});
    current = &view;
    int cancelled = 0;
    Widget child;
    child.parent = view.page(2);
    child.bounds = {0, 0, 100, 40};
    child.focusable = true;
    child.onEvent = [&](Context&, WidgetId, const Event& e)
    {
        if (e.type == EventType::Cancel)
        {
            ++cancelled;
            view.setSelected(1);
        }
        const bool down = e.type == EventType::PointerDown;
        return EventReply{down, down, down};
    };
    auto id = ui.add(child);
    ui.layout(view.id(), {0, 0, 400, 200});
    const auto p = *ui.mapToRoot(id, {10, 10});
    ui.dispatch(point(EventType::PointerDown, p.x, p.y));
    ui.focus(view.tabId(2));
    ui.dispatch(key(Key::Home));
    require(cancelled == 1 && view.selected() == 1 && !ui.isAvailable(view.page(0)) && ui.isAvailable(view.page(1)) &&
                !ui.isAvailable(view.page(2)),
            "callback selection left multiple or stale visible pages");
    require(changes == 1 && !stale, "page selection callback did not describe its delivery state");
}
} // namespace
int main()
{
    try
    {
        ellipsisAndPrimitives();
        tabs();
        pagesAndCollapse();
        search();
        callbackSelection();
        std::cout << "Workspace control tests passed\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
