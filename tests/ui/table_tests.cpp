#include <iostream>
#include <limits>
#include <stdexcept>
#include <vectorgl/ui/table_view.hpp>
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
    throw std::runtime_error("Invalid table input accepted");
}
std::vector<TableColumn> columns()
{
    return {{1, "Name", 120, 60, 220}, {2, "Value", 100, 50, 180}, {3, "Status", 100, 60, 150}};
}
std::shared_ptr<const TableModel> sample()
{
    return std::make_shared<VectorTableModel>(std::vector<TableItem>{{10, {{1, "One"}, {2, "100"}, {3, "Active"}}},
                                                                     {20, {{1, "Disabled"}}, false},
                                                                     {30, {{1, "Three"}, {2, "300"}}}});
}
Event key(Key value, Modifiers modifiers = {}, EventType type = EventType::KeyDown)
{
    Event e;
    e.type = type;
    e.key = value;
    e.modifiers = modifiers;
    return e;
}
Event headerPoint(Context& ui, TableView& table, ColumnId column, bool edge = false)
{
    const auto b = table.headerCellBounds(column).value();
    const auto p = ui.mapToRoot(table.headerId(), {b.x + (edge ? b.width - 1 : 20), b.y + b.height / 2}).value();
    Event e;
    e.type = EventType::PointerDown;
    e.x = p.x;
    e.y = p.y;
    e.button = PointerButton::Primary;
    return e;
}
void click(Context& ui, TableView& table, ColumnId column)
{
    auto e = headerPoint(ui, table, column);
    require(ui.dispatch(e), "Header press escaped");
    e.type = EventType::PointerUp;
    require(ui.dispatch(e), "Header release escaped");
}
void sortingAndSelection()
{
    Context ui;
    std::unique_ptr<TableView> table;
    int sorts = 0, selected = 0, activated = 0;
    TableViewOptions o;
    o.rowHeight = 30;
    o.onSortRequested = [&](TableSort sort)
    {
        ++sorts;
        if (sort.direction == SortDirection::Ascending)
            table->setModel(
                std::make_shared<VectorTableModel>(std::vector<TableItem>{{30, {{1, "Three"}}}, {10, {{1, "One"}}}}));
    };
    o.onSelectionChanged = [&](const auto&) { ++selected; };
    o.onActivated = [&](ItemId) { ++activated; };
    table = std::make_unique<TableView>(ui, WidgetId{}, sample(), columns(), o);
    ui.layout(table->id(), {10, 20, 400, 200});
    table->setSelection({30});
    click(ui, *table, 1);
    require(sorts == 1 && table->sort() == TableSort{1, SortDirection::Ascending} &&
                table->selection() == std::vector<ItemId>{30},
            "Sort hook/reorder lost selection");
    click(ui, *table, 1);
    require(sorts == 2 && table->sort() == TableSort{1, SortDirection::Descending}, "Repeated sort did not reverse");
    ui.focus(table->headerId());
    ui.dispatch(key(Key::Right));
    ui.dispatch(key(Key::Enter));
    ui.dispatch(key(Key::Enter, {}, EventType::KeyRepeat));
    require(sorts == 2, "Sort fired before key release");
    ui.dispatch(key(Key::Enter, {}, EventType::KeyUp));
    ui.dispatch(key(Key::Enter, {}, EventType::KeyUp));
    require(sorts == 3 && table->sort()->column == 2, "Keyboard sort repeated/wrong column");
    ui.dispatch(key(Key::Enter));
    ui.dispatch(Event{EventType::WindowFocusLost});
    ui.focus(table->headerId());
    ui.dispatch(key(Key::Enter, {}, EventType::KeyUp));
    require(sorts == 3, "Sort survived focus loss");
    ui.focus(table->bodyId());
    ui.dispatch(key(Key::End));
    ui.dispatch(key(Key::Enter));
    ui.dispatch(key(Key::Enter, {}, EventType::KeyUp));
    require(activated == 1 && selected <= 1, "Body activation/selection failed");
    ui.focus(table->headerId());
    ui.dispatch(key(Key::Tab));
    require(ui.focused() == table->bodyId(), "Header/body Tab order failed");
}
void resizingAndCancellation()
{
    Context ui;
    int resized = 0, sorts = 0;
    TableViewOptions o;
    o.onColumnResized = [&](ColumnId id, float width)
    {
        require(id == 1 && width >= 60, "Bad resize callback");
        ++resized;
    };
    o.onSortRequested = [&](TableSort) { ++sorts; };
    TableView table(ui, {}, sample(), columns(), o);
    ui.layout(table.id(), {0, 0, 300, 200});
    auto e = headerPoint(ui, table, 1, true);
    ui.dispatch(e);
    e.type = EventType::PointerMove;
    e.x += 500;
    ui.dispatch(e);
    require(table.columns()[0].width == 220 && resized == 0, "Drag did not clamp/live update");
    ui.layout(table.id(), {0, 0, 300, 200});
    e.type = EventType::PointerUp;
    ui.dispatch(e);
    require(resized == 1 && sorts == 0 && !ui.captured(), "Resize sorted or failed completion");
    table.setColumnWidth(1, 120);
    ui.layout(table.id(), {0, 0, 300, 200});
    e = headerPoint(ui, table, 1, true);
    ui.dispatch(e);
    e.type = EventType::PointerMove;
    e.x -= 100;
    ui.dispatch(e);
    require(table.columns()[0].width == 60, "Resize minimum not enforced");
    ui.dispatch(key(Key::Escape));
    require(table.columns()[0].width == 120 && resized == 1 && !ui.captured(), "Cancelled drag did not restore width");
    e = headerPoint(ui, table, 1, true);
    ui.dispatch(e);
    e.type = EventType::PointerMove;
    e.x += 30;
    ui.dispatch(e);
    ui.dispatch(Event{EventType::WindowFocusLost});
    require(table.columns()[0].width == 120, "Focus loss committed resize");
    ui.focus(table.headerId());
    Modifiers shift;
    shift.shift = true;
    ui.dispatch(key(Key::Right, shift));
    require(table.columns()[0].width == 128 && resized == 2, "Keyboard resize failed");
    e = headerPoint(ui, table, 1, true);
    ui.dispatch(e);
    e.type = EventType::PointerMove;
    e.x += 30;
    ui.dispatch(e);
    table.setModel(sample());
    require(table.columns()[0].width == 128 && !ui.captured(), "Model replacement retained header drag");
    ui.layout(table.id(), {0, 0, 300, 200});
    e = headerPoint(ui, table, 1, true);
    ui.dispatch(e);
    e.type = EventType::PointerMove;
    e.x += 30;
    ui.dispatch(e);
    ui.layout(table.id(), {0, 0, 360, 200});
    require(table.columns()[0].width == 128 && !ui.captured(), "Viewport resize retained header drag");
}
class CountingModel final : public TableModel
{
public:
    mutable std::size_t ids = 0, texts = 0;
    std::size_t rowCount() const override
    {
        return 2000;
    }
    ItemId itemId(std::size_t row) const override
    {
        ++ids;
        return row + 1;
    }
    std::string_view cellText(std::size_t, ColumnId) const override
    {
        ++texts;
        return "Cell";
    }
};
class DuplicateModel final : public TableModel
{
public:
    std::size_t rowCount() const override
    {
        return 2;
    }
    ItemId itemId(std::size_t) const override
    {
        return 1;
    }
    std::string_view cellText(std::size_t, ColumnId) const override
    {
        return {};
    }
};
void geometryAndValidation()
{
    Context ui;
    auto model = std::make_shared<CountingModel>();
    TableViewOptions o;
    o.rowHeight = 30;
    TableView table(ui, {}, model, columns(), o);
    ui.layout(table.id(), {0, 0, 200, 200});
    require(ui.layoutStats().arrangedWidgets < 14, "Table allocated per-row/cell widgets");
    require(table.setHorizontalScrollOffset(80) && table.horizontalScrollOffset() == 80,
            "Table horizontal extent failed");
    require(table.headerCellBounds(1)->x == 2 - 80, "Header horizontal offset did not match body");
    table.setScrollOffset(45000);
    require(table.visibleRows().first == 1500 && table.horizontalScrollOffset() == 80,
            "Vertical scroll changed column offset");
    table.setSelection({1800});
    require(table.horizontalScrollOffset() == 80, "Selection reveal reset horizontal scroll");
    ui.focus(table.bodyId());
    ui.dispatch(key(Key::End));
    require(model->ids == 2000 && model->texts == 0, "Table navigation rescanned model");
    const auto original = table.columns();
    const auto oldModel = table.model();
    rejects([&] { table.setModel(std::make_shared<DuplicateModel>()); });
    require(table.model() == oldModel && table.currentItem() == 2000, "Invalid table model replaced current state");
    auto bad = original;
    bad[0].id = 2;
    rejects([&] { table.setColumns(bad); });
    require(table.columns()[0].id == 1, "Invalid columns replaced good geometry");
    rejects([&] { table.setColumnWidth(1, std::numeric_limits<float>::quiet_NaN()); });
    rejects([&] { table.setColumnWidth(999, 120); });
    rejects([&] { table.setSort(TableSort{999}); });
    rejects([] { VectorTableModel invalid({{1, {{1, "A"}, {1, "B"}}}}); });
    rejects([] { VectorTableModel invalid({{1, {}}, {1, {}}}); });
    rejects([] { VectorTableModel invalid({{0, {}}}); });
    VectorTableModel sparse({{1, {{1, "A"}}}});
    require(sparse.cellText(0, 99).empty(), "Missing table cell was not empty");
    table.setSort(TableSort{2, SortDirection::Descending});
    table.setColumns({original[2], original[1], original[0]});
    require(table.sort()->column == 2, "Column reorder lost sort ID");
    table.setColumns({original[0]});
    require(!table.sort(), "Removed sort column retained indicator");
    table.setModel({});
    ui.layout(table.id(), {0, 0, 200, 200});
    require(table.selection().empty() && !table.currentItem(), "Null table model retained selection");
    table.setColumns({});
    ui.focus(table.headerId());
    ui.dispatch(key(Key::End));
}
void callbackAndModalSafety()
{
    Context ui;
    std::unique_ptr<TableView> table;
    TableViewOptions o;
    o.onSortRequested = [&](TableSort) { table.reset(); };
    table = std::make_unique<TableView>(ui, WidgetId{}, sample(), columns(), o);
    ui.layout(table->id(), {0, 0, 400, 200});
    click(ui, *table, 1);
    require(!table && !ui.captured(), "Sort callback destruction retained capture");
    o.onSortRequested = {};
    o.onColumnResized = [&](ColumnId, float) { table->setColumns({{99, "Replacement"}}); };
    table = std::make_unique<TableView>(ui, WidgetId{}, sample(), columns(), o);
    ui.layout(table->id(), {0, 0, 400, 200});
    auto e = headerPoint(ui, *table, 1, true);
    ui.dispatch(e);
    e.type = EventType::PointerMove;
    e.x += 20;
    ui.dispatch(e);
    e.type = EventType::PointerUp;
    ui.dispatch(e);
    require(table->columns()[0].id == 99 && !ui.captured(), "Resize callback replacement failed");
    ui.layout(table->id(), {0, 0, 400, 200});
    Widget modal;
    modal.bounds = {450, 0, 100, 100};
    modal.focusable = true;
    const auto modalId = ui.add(modal);
    ui.pushModal(modalId);
    click(ui, *table, 99);
    require(!table->sort(), "Modal allowed background sorting");
    ui.popModal();
    ui.remove(modalId);
    ui.setEnabled(table->id(), false);
    require(!ui.focus(table->headerId()) && !ui.focus(table->bodyId()), "Disabled table accepted focus");
}
void deferredReplacementAndFlags()
{
    Context ui;
    auto cols = columns();
    cols[1].resizable = false;
    cols[1].sortable = false;
    int sorts = 0;
    TableViewOptions o;
    o.onSortRequested = [&](TableSort) { ++sorts; };
    TableView table(ui, {}, sample(), cols, o);
    ui.layout(table.id(), {0, 0, 400, 200});
    click(ui, table, 2);
    require(sorts == 0 && !table.sort(), "Nonsortable column activated");
    rejects([&] { table.setSort(TableSort{2}); });
    auto e = headerPoint(ui, table, 2, true);
    ui.dispatch(e);
    e.type = EventType::PointerMove;
    e.x += 30;
    ui.dispatch(e);
    e.type = EventType::PointerUp;
    ui.dispatch(e);
    require(table.columns()[1].width == 100, "Nonresizable column changed width");
    e = headerPoint(ui, table, 1, true);
    ui.dispatch(e);
    e.type = EventType::PointerMove;
    e.x += 30;
    ui.dispatch(e);
    Widget timer;
    timer.onEvent = [&](Context&, WidgetId, const Event& event)
    {
        if (event.type == EventType::Timer)
            table.setColumns({{99, "Replacement", 160}});
        return EventReply{};
    };
    const auto timerId = ui.add(timer);
    ui.requestWakeup(timerId, ui.now() - 1);
    ui.pumpTimers();
    require(!ui.captured() && table.columns()[0].id == 99 && table.columns()[0].width == 160,
            "Deferred cancellation restored old width into a replacement column");
    ui.remove(timerId);
    ui.layout(table.id(), {0, 0, 400, 200});
    ui.setTransform(table.id(), Mat3x3::translation(50, 30) * Mat3x3::scaling(1.5f, 1.5f));
    click(ui, table, 99);
    require(table.sort() && table.sort()->column == 99, "Transformed header sorting failed");
}
} // namespace
int main()
{
    try
    {
        sortingAndSelection();
        resizingAndCancellation();
        geometryAndValidation();
        callbackAndModalSafety();
        deferredReplacementAndFlags();
        std::cout << "Table tests passed\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
