#include <iostream>
#include <stdexcept>
#include <vectorgl/ui/context.hpp>
#include <vectorgl/ui/list_view.hpp>
#include <vectorgl/ui/table_view.hpp>
using namespace vectorgl;
using namespace vectorgl::ui;
namespace
{
void require(bool value, const char* text)
{
    if (!value)
        throw std::runtime_error(text);
}
struct Symbol
{
    int value;
};
Event point(EventType type, float x, float y)
{
    Event e;
    e.type = type;
    e.x = x;
    e.y = y;
    e.button = type == EventType::PointerMove ? PointerButton::None : PointerButton::Primary;
    return e;
}
void sessions()
{
    Context ui;
    int completed = 0, drops = 0, leaves = 0, resets = 0;
    DragResult result;
    Widget source;
    source.bounds = {0, 0, 40, 40};
    source.focusable = true;
    source.onEvent = [&](Context& context, WidgetId id, const Event& e)
    {
        if (e.type == EventType::Cancel)
            ++resets;
        if (e.type == EventType::PointerDown)
            require(context.beginDrag(id, DragPayload::make(Symbol{42}),
                                      [&](const DragResult& r)
                                      {
                                          result = r;
                                          ++completed;
                                      }),
                    "Drag failed to start");
        return EventReply{true};
    };
    const auto sourceId = ui.add(source);
    Widget target;
    target.bounds = {80, 0, 80, 80};
    target.clipChildren = true;
    target.onDrag = [&](Context&, WidgetId, const DragEvent& e)
    {
        require(e.payload.get<Symbol>() && !e.payload.get<int>() && e.payload.get<Symbol>()->value == 42,
                "Payload type/lifetime failure");
        if (e.phase == DragPhase::Drop)
            ++drops;
        if (e.phase == DragPhase::Leave)
            ++leaves;
        return e.localX < 120;
    };
    const auto targetId = ui.add(target);
    ui.dispatch(point(EventType::PointerDown, 10, 10));
    ui.dispatch(point(EventType::PointerMove, 90, 10));
    require(ui.dragState() && ui.dragState()->target == targetId && ui.dragState()->accepted,
            "Drop feedback target missing");
    ui.dispatch(point(EventType::PointerUp, 90, 10));
    require(completed == 1 && drops == 1 && leaves == 1 && resets == 1 && result.outcome == DragOutcome::Dropped &&
                !ui.captured(),
            "Drop completion was not exactly once");
    require(result.payload.get<Symbol>()->value == 42, "Retained payload expired");
    ui.dispatch(point(EventType::PointerUp, 90, 10));
    require(completed == 1, "Stale release repeated drop");
    ui.dispatch(point(EventType::PointerDown, 10, 10));
    ui.dispatch(point(EventType::PointerUp, 140, 10));
    require(result.outcome == DragOutcome::Rejected && drops == 1 && completed == 2, "Rejected target received Drop");
    for (auto type : {EventType::WindowFocusLost, EventType::PointerLeave, EventType::Cancel})
    {
        ui.dispatch(point(EventType::PointerDown, 10, 10));
        ui.dispatch(point(EventType::PointerMove, 90, 10));
        ui.dispatch(Event{type});
        require(result.outcome == DragOutcome::Cancelled && !ui.dragState() && !ui.captured(),
                "Cancellation left active drag");
    }
    ui.dispatch(point(EventType::PointerDown, 10, 10));
    Event escape;
    escape.type = EventType::KeyDown;
    escape.key = Key::Escape;
    require(ui.dispatch(escape), "Drag Escape escaped");
    require(!ui.dragState(), "Escape did not cancel drag");
    ui.dispatch(point(EventType::PointerDown, 10, 10));
    ui.setEnabled(sourceId, false);
    require(!ui.dragState() && result.reason == CancelReason::Unavailable, "Disabled source retained session");
    ui.setEnabled(sourceId, true);
    ui.dispatch(point(EventType::PointerDown, 10, 10));
    ui.remove(sourceId);
    require(!ui.dragState(), "Removed source retained drag");
}
void boundariesAndCallbacks()
{
    Context ui;
    int completed = 0;
    Widget source;
    source.bounds = {0, 0, 30, 30};
    source.onEvent = [&](Context& c, WidgetId id, const Event& e)
    {
        if (e.type == EventType::PointerDown)
            c.beginDrag(id, DragPayload::make(Symbol{1}), [&](const DragResult&) { ++completed; });
        return EventReply{true};
    };
    const auto sourceId = ui.add(source);
    Widget clip;
    clip.bounds = {50, 0, 50, 50};
    clip.clipChildren = true;
    const auto clipId = ui.add(clip);
    Widget target;
    target.parent = clipId;
    target.bounds = {0, 0, 100, 100};
    target.transform = Mat3x3::translation(50, 0);
    target.onDrag = [](Context&, WidgetId, const DragEvent&) { return true; };
    const auto targetId = ui.add(target);
    ui.dispatch(point(EventType::PointerDown, 10, 10));
    ui.dispatch(point(EventType::PointerMove, 120, 10));
    require(!ui.dragState()->target, "Drag ignored ancestor clip");
    ui.dispatch(point(EventType::PointerMove, 60, 10));
    require(ui.dragState()->target == targetId, "Transformed drop target missed");
    ui.remove(targetId);
    require(!ui.dragState()->target, "Removed target retained feedback");
    Widget modal;
    modal.bounds = {150, 0, 50, 50};
    modal.focusable = true;
    const auto modalId = ui.add(modal);
    ui.pushModal(modalId);
    require(!ui.dragState() && completed == 1, "Modal interruption did not cancel");
    ui.popModal();
    ui.remove(modalId);
    Widget failing;
    failing.bounds = {50, 0, 50, 50};
    failing.onDrag = [](Context&, WidgetId, const DragEvent& e) -> bool
    {
        if (e.phase == DragPhase::Enter)
            throw std::runtime_error("expected target failure");
        return false;
    };
    ui.add(failing);
    ui.dispatch(point(EventType::PointerDown, 10, 10));
    bool threw = false;
    try
    {
        ui.dispatch(point(EventType::PointerMove, 60, 10));
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    require(threw && !ui.dragState() && !ui.captured() && completed == 2, "Throwing target retained session/payload");
    require(!ui.beginDrag(sourceId, DragPayload::make(1)), "Drag started outside source input callback");
}
void projections()
{
    Context ui;
    auto full = std::make_shared<VectorListModel>(std::vector<ListItem>{{1, "One"}, {2, "Two"}, {3, "Three"}});
    ListView list(ui, {}, full);
    list.setSelection({2});
    auto filtered = std::make_shared<VectorListModel>(std::vector<ListItem>{{1, "One"}, {3, "Three"}});
    list.setProjection(filtered, {1, 2, 3});
    require(list.selection() == std::vector<ItemId>{2} && list.currentItem() != 2,
            "Filtering discarded hidden selection or retained hidden current");
    list.setProjection(full, {1, 2, 3});
    require(list.selection() == std::vector<ItemId>{2}, "Unfiltering lost selection");
    list.setProjection(filtered, {1, 3});
    require(list.selection().empty(), "Removal retained stale hidden selection");
    const auto before = list.model();
    bool rejected = false;
    try
    {
        list.setProjection(filtered, {1, 1, 3});
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    require(rejected && list.model() == before, "Invalid universe changed projection");
    TableView table(ui, {}, std::make_shared<VectorTableModel>(std::vector<TableItem>{{1, {}}, {2, {}}}),
                    {{1, "Value"}});
    table.setSelection({2});
    table.setProjection(std::make_shared<VectorTableModel>(std::vector<TableItem>{{1, {}}}), {1, 2});
    require(table.selection() == std::vector<ItemId>{2}, "Table filtering discarded selection");
}
void reentrancyAndOwnership()
{
    Context ui;
    int completed = 0, leaves = 0;
    std::weak_ptr<int> lifetime;
    DragOutcome outcome = DragOutcome::Cancelled;
    Widget source;
    source.bounds = {0, 0, 30, 30};
    source.onEvent = [&](Context& c, WidgetId id, const Event& e)
    {
        if (e.type == EventType::PointerDown)
        {
            auto owned = std::make_shared<int>(7);
            lifetime = owned;
            c.beginDrag(id, DragPayload::make(owned),
                        [&](const DragResult& result)
                        {
                            ++completed;
                            outcome = result.outcome;
                        });
        }
        return EventReply{true};
    };
    const auto sourceId = ui.add(source);
    Widget parent;
    parent.bounds = {80, 0, 100, 100};
    parent.onDrag = [](Context&, WidgetId, const DragEvent&) { return true; };
    const auto parentId = ui.add(parent);
    Widget child;
    child.parent = parentId;
    child.bounds = {80, 0, 100, 100};
    child.onDrag = [](Context&, WidgetId, const DragEvent&) { return false; };
    const auto rejectId = ui.add(child);
    ui.dispatch(point(EventType::PointerDown, 10, 10));
    auto foreign = point(EventType::PointerUp, 90, 10);
    foreign.pointer = 99;
    ui.dispatch(foreign);
    require(ui.dragState() && !lifetime.expired(), "Foreign pointer finished/released payload");
    ui.dispatch(point(EventType::PointerUp, 90, 10));
    require(completed == 1 && outcome == DragOutcome::Rejected && lifetime.expired(),
            "Rejected child fell through or payload leaked");
    ui.remove(rejectId);
    Widget remover;
    remover.bounds = {80, 0, 100, 100};
    remover.onDrag = [&](Context& c, WidgetId id, const DragEvent& e)
    {
        if (e.phase == DragPhase::Drop)
            c.remove(id);
        if (e.phase == DragPhase::Leave)
            ++leaves;
        return true;
    };
    ui.add(remover);
    ui.dispatch(point(EventType::PointerDown, 10, 10));
    ui.dispatch(point(EventType::PointerUp, 90, 10));
    require(completed == 2 && outcome == DragOutcome::Dropped && leaves == 1 && lifetime.expired(),
            "Drop callback removal broke exactly-once cleanup");
    Widget removeSource;
    removeSource.bounds = {80, 0, 100, 100};
    removeSource.onDrag = [&](Context& c, WidgetId, const DragEvent& e)
    {
        if (e.phase == DragPhase::Enter)
            c.remove(sourceId);
        return true;
    };
    ui.add(removeSource);
    ui.dispatch(point(EventType::PointerDown, 10, 10));
    ui.dispatch(point(EventType::PointerMove, 90, 10));
    require(completed == 3 && outcome == DragOutcome::Cancelled && !ui.dragState() && lifetime.expired(),
            "Enter callback source removal leaked session");
}
} // namespace
int main()
{
    try
    {
        sessions();
        boundariesAndCallbacks();
        projections();
        reentrancyAndOwnership();
        std::cout << "Drag/drop tests passed\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
