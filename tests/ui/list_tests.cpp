#include <iostream>
#include <limits>
#include <stdexcept>
#include <vectorgl/ui/list_view.hpp>

using namespace vectorgl;
using namespace vectorgl::ui;
namespace
{
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F&& f)
{
    try
    {
        f();
    }
    catch (const std::invalid_argument&)
    {
        return;
    }
    throw std::runtime_error("Invalid list input was accepted");
}
Event key(Key value, Modifiers modifiers = {}, EventType type = EventType::KeyDown)
{
    Event e;
    e.type = type;
    e.key = value;
    e.modifiers = modifiers;
    return e;
}
void click(Context& ui, ListView& list, std::size_t row, Modifiers modifiers = {}, bool outside = false)
{
    const auto b = ui.bounds(list.viewportId()).value();
    const auto p =
        ui.mapToRoot(list.viewportId(), {b.x + 12, b.y + float(row) * 30 + 15 - list.scrollOffset()}).value();
    Event e;
    e.type = EventType::PointerDown;
    e.x = p.x;
    e.y = p.y;
    e.button = PointerButton::Primary;
    e.modifiers = modifiers;
    require(ui.dispatch(e), "List click escaped to app");
    e.type = EventType::PointerUp;
    if (outside)
        e.x = -50;
    require(ui.dispatch(e), "List release escaped to app");
}
std::shared_ptr<const ListModel> sample()
{
    return std::make_shared<VectorListModel>(
        std::vector<ListItem>{{10, "One"}, {20, "Disabled", false}, {30, "Three"}, {40, "Four"}, {50, "Five"}});
}
void geometry()
{
    require(visibleListRows(2000, 30, 0, 90) == ListRowRange{0, 3}, "Exact visible range failed");
    require(visibleListRows(2000, 30, 15, 90) == ListRowRange{0, 4}, "Partial visible range failed");
    require(visibleListRows(2000, 30, 59970, 100) == ListRowRange{1999, 2000}, "End range overflowed");
    require(visibleListRows(0, 30, 0, 100) == ListRowRange{}, "Empty range failed");
    require(visibleListRows(2000, 30, 15, 0) == ListRowRange{}, "Zero viewport painted rows");
    rejects([] { visibleListRows(2, 0, 0, 100); });
    rejects([] { visibleListRows(2, 30, -1, 100); });
    rejects([] { visibleListRows(2, 30, 0, std::numeric_limits<float>::infinity()); });
}
void selectionAndInput()
{
    Context ui;
    int changes = 0, activations = 0;
    ItemId activated = 0;
    ListViewOptions o;
    o.rowHeight = 30;
    o.selectionMode = ListSelectionMode::Multiple;
    o.onSelectionChanged = [&](const auto&) { ++changes; };
    o.onActivated = [&](ItemId id)
    {
        activated = id;
        ++activations;
    };
    ListView list(ui, {}, sample(), o);
    ui.layout(list.id(), {20, 30, 200, 95});
    require(list.visibleRows() == ListRowRange{0, 4}, "View geometry not synchronized");
    click(ui, list, 0);
    require(ui.focused() == list.id() && list.selection() == std::vector<ItemId>{10} && changes == 1,
            "Click did not select/focus");
    click(ui, list, 1);
    require(changes == 1, "Disabled row changed selection");
    Modifiers ctrl;
    ctrl.control = true;
    click(ui, list, 2, ctrl);
    require(list.selection() == std::vector<ItemId>({10, 30}), "Ctrl click did not extend");
    click(ui, list, 2, ctrl);
    require(list.selection() == std::vector<ItemId>{10}, "Ctrl click did not toggle");
    list.setSelection({10});
    ui.dispatch(key(Key::Down));
    require(list.currentItem() == 30 && list.selection() == std::vector<ItemId>{30}, "Arrow did not skip disabled row");
    Modifiers shift;
    shift.shift = true;
    ui.dispatch(key(Key::End, shift));
    require(list.selection() == std::vector<ItemId>({30, 40, 50}), "Shift range failed");
    ui.dispatch(key(Key::Home, ctrl));
    require(list.currentItem() == 10 && list.selection() == std::vector<ItemId>({30, 40, 50}),
            "Ctrl navigation changed selection");
    ui.dispatch(key(Key::A, ctrl));
    require(list.selection() == std::vector<ItemId>({10, 30, 40, 50}), "Select all included disabled rows");
    ui.dispatch(key(Key::End));
    require(list.scrollOffset() > 0, "Navigation did not reveal last row");
    ui.dispatch(key(Key::Enter));
    ui.dispatch(key(Key::Enter, {}, EventType::KeyRepeat));
    require(activations == 0, "Activation fired before release");
    ui.dispatch(key(Key::Enter, {}, EventType::KeyUp));
    ui.dispatch(key(Key::Enter, {}, EventType::KeyUp));
    require(activations == 1 && activated == 50, "Activation repeated or lost identity");
    list.setSelection({10});
    click(ui, list, 0, {}, true);
    require(list.selection() == std::vector<ItemId>{10} && !ui.captured(), "Outside release selected/captured");
    ui.dispatch(key(Key::Enter));
    ui.dispatch(Event{EventType::WindowFocusLost});
    ui.focus(list.id());
    ui.dispatch(key(Key::Enter, {}, EventType::KeyUp));
    require(activations == 1, "Activation survived focus loss");
    ui.setEnabled(list.id(), false);
    require(!ui.focus(list.id()), "Disabled list accepted focus");
}
void replacement()
{
    Context ui;
    int changes = 0;
    ListViewOptions o;
    o.rowHeight = 30;
    o.selectionMode = ListSelectionMode::Multiple;
    o.onSelectionChanged = [&](const auto&) { ++changes; };
    ListView list(ui, {}, sample(), o);
    ui.layout(list.id(), {0, 0, 200, 100});
    list.setSelection({30, 50});
    list.setModel(std::make_shared<VectorListModel>(std::vector<ListItem>{{50, "Five"}, {10, "One"}, {30, "Three"}}));
    ui.layout(list.id(), {0, 0, 200, 100});
    require(list.selection() == std::vector<ItemId>({50, 30}) && list.currentItem() == 30, "Reorder lost stable IDs");
    const auto old = list.model();
    rejects([&] { list.setModel(std::make_shared<VectorListModel>(std::vector<ListItem>{{10, "A"}, {10, "B"}})); });
    rejects([&] { list.setModel(std::make_shared<VectorListModel>(std::vector<ListItem>{{0, "A"}})); });
    require(list.model() == old && list.selection().size() == 2, "Invalid model replaced good snapshot");
    rejects([&] { list.setSelection({999}); });
    list.setModel(std::make_shared<VectorListModel>(std::vector<ListItem>{{50, "Five", false}, {10, "One"}}));
    ui.layout(list.id(), {0, 0, 200, 100});
    require(list.selection().empty() && list.currentItem() == 10 && changes == 0,
            "Removed/disabled IDs retained or setter notified");
    list.setModel({});
    ui.layout(list.id(), {0, 0, 200, 100});
    require(!list.currentItem() && list.selection().empty() && list.scrollOffset() == 0, "Empty reset retained state");
    ListView single(ui, {}, sample());
    rejects([&] { single.setSelection({10, 30}); });
    rejects([&] { single.setSelection({20}); });
    rejects(
        [&]
        {
            ListViewOptions bad;
            bad.rowHeight = -1;
            ListView invalid(ui, {}, {}, bad);
        });
}
class CountingModel final : public ListModel
{
public:
    mutable std::size_t ids = 0, availability = 0, texts = 0;
    std::size_t rowCount() const override
    {
        return 2000;
    }
    ItemId itemId(std::size_t row) const override
    {
        ++ids;
        return row + 1;
    }
    bool enabled(std::size_t) const override
    {
        ++availability;
        return true;
    }
    std::string_view text(std::size_t) const override
    {
        ++texts;
        return "Row";
    }
};
void boundedWorkAndCancellation()
{
    Context ui;
    auto model = std::make_shared<CountingModel>();
    ListViewOptions o;
    o.rowHeight = 30;
    ListView list(ui, {}, model, o);
    require(model->ids == 2000 && model->texts == 0, "Snapshot indexing fetched row text");
    ui.layout(list.id(), {0, 0, 200, 300});
    require(ui.layoutStats().arrangedWidgets < 12 && list.visibleRows() == ListRowRange{0, 10},
            "List created per-row widgets");
    list.setScrollOffset(45000);
    require(list.visibleRows() == ListRowRange{1500, 1510}, "Far scrolling range failed");
    ui.focus(list.id());
    ui.dispatch(key(Key::End));
    require(list.currentItem() == 2000 && list.visibleRows().end == 2000, "End navigation failed");
    require(model->ids == 2000 && model->availability == 2000 && model->texts == 0,
            "Scrolling/navigation rescanned data source");
    list.setScrollOffset(0);
    Event e;
    e.type = EventType::PointerDown;
    e.x = 10;
    e.y = 10;
    e.button = PointerButton::Primary;
    ui.dispatch(e);
    require(ui.captured() == list.id(), "List did not capture press");
    list.setModel(sample());
    require(!ui.captured(), "Snapshot replacement retained stale capture");
    e.type = EventType::PointerUp;
    ui.dispatch(e);
    require(list.selection().empty(), "Stale pointer release selected replacement row");
}
void reentrantCallbacks()
{
    Context ui;
    std::unique_ptr<ListView> list;
    ListViewOptions o;
    o.rowHeight = 30;
    o.onSelectionChanged = [&](const auto&) { list->setModel({}); };
    list = std::make_unique<ListView>(ui, WidgetId{}, sample(), o);
    ui.layout(list->id(), {0, 0, 200, 100});
    click(ui, *list, 0);
    require(!list->model() && list->selection().empty() && !ui.captured(), "Callback reset left stale state");
    list.reset();
    o.onSelectionChanged = [&](const auto&) { list.reset(); };
    list = std::make_unique<ListView>(ui, WidgetId{}, sample(), o);
    ui.layout(list->id(), {0, 0, 200, 100});
    ui.focus(list->id());
    ui.dispatch(key(Key::Down));
    require(!list && !ui.focused() && !ui.captured(), "Callback destruction retained focus/capture");
}
void sparseNavigationAndClipping()
{
    Context ui;
    std::vector<ListItem> items;
    for (ItemId id = 1; id <= 50; ++id)
        items.push_back({id, "Row", id == 1 || id == 50});
    ListViewOptions o;
    o.rowHeight = 30;
    ListView list(ui, {}, std::make_shared<VectorListModel>(std::move(items)), o);
    ui.layout(list.id(), {0, 0, 200, 100});
    ui.focus(list.id());
    ui.dispatch(key(Key::End));
    ui.dispatch(key(Key::PageUp));
    require(list.currentItem() == 1, "PageUp got stuck in disabled rows");
    ui.dispatch(key(Key::PageDown));
    require(list.currentItem() == 50, "PageDown got stuck in disabled rows");
    list.setModel(sample());
    ui.layout(list.id(), {0, 0, 200, 100});
    list.setScrollOffset(0);
    ui.setTransform(list.id(), Mat3x3::translation(80, 40) * Mat3x3::scaling(1.5f, 1.5f));
    click(ui, list, 2);
    require(list.selection() == std::vector<ItemId>{30}, "Transformed list selected wrong row");
    Widget modal;
    modal.bounds = {400, 0, 100, 100};
    modal.focusable = true;
    const auto modalId = ui.add(modal);
    ui.pushModal(modalId);
    click(ui, list, 0);
    require(list.selection() == std::vector<ItemId>{30}, "Modal allowed background list selection");
    ui.popModal();
    ui.remove(modalId);
    ui.setEnabled(list.id(), false);
    require(!ui.isDescendant(ui.hitTest({100, 60}), list.id()), "Disabled list remained clickable");
}
} // namespace
int main()
{
    try
    {
        geometry();
        selectionAndInput();
        replacement();
        boundedWorkAndCancellation();
        reentrantCallbacks();
        sparseNavigationAndClipping();
        std::cout << "List tests passed\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
