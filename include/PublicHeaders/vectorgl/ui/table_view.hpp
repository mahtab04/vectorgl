#pragma once

#include <unordered_map>

#include "vectorgl/text.hpp"
#include "vectorgl/ui/list_view.hpp"

namespace vectorgl::ui
{
using ColumnId = std::uint64_t; // Nonzero, stable independently of column order.
/*! Immutable UI-thread row snapshot. IDs/enabled metadata are indexed on model
 * replacement; cell text is queried only for painted rows/columns. Methods and
 * painters must not mutate the UI/model; text views live as long as the snapshot. */
class TableModel
{
public:
    virtual ~TableModel() = default;
    virtual std::size_t rowCount() const = 0;
    virtual ItemId itemId(std::size_t row) const = 0;
    virtual std::string_view cellText(std::size_t row, ColumnId column) const = 0;
    virtual bool enabled(std::size_t) const
    {
        return true;
    }
};
struct TableValue
{
    ColumnId column = 0;
    std::string text;
};
struct TableItem
{
    ItemId id = 0;
    std::vector<TableValue> cells;
    bool enabled = true;
};
class VectorTableModel final : public TableModel
{
public:
    explicit VectorTableModel(std::vector<TableItem> items);
    std::size_t rowCount() const override;
    ItemId itemId(std::size_t row) const override;
    std::string_view cellText(std::size_t row, ColumnId column) const override; // Missing column is empty.
    bool enabled(std::size_t row) const override;

private:
    std::vector<TableItem> items_;
    std::vector<std::unordered_map<ColumnId, std::size_t>> cells_;
};
struct TableColumn
{
    ColumnId id = 0;
    std::string label;
    float width = 160, minimumWidth = 48, maximumWidth = 1024;
    bool resizable = true, sortable = true;
    TextAlign align = TextAlign::Left;
};
enum class SortDirection
{
    Ascending,
    Descending
};
struct TableSort
{
    ColumnId column = 0;
    SortDirection direction = SortDirection::Ascending;
    bool operator==(const TableSort&) const = default;
};
struct TableCell
{
    ListRow row;
    ColumnId column;
    std::size_t columnIndex;
    Bounds bounds;
    TextAlign align;
};
/*! Custom cell content, inside enforced cell/row/viewport clips and scoped Canvas
 * state. Row selection/focus backgrounds and column separators belong to the view. */
class TableCellPainter
{
public:
    virtual ~TableCellPainter() = default;
    virtual void paint(PaintContext& context, const TableModel& model, const TableCell& cell) = 0;
};
struct TableViewOptions
{
    float rowHeight = 36, headerHeight = 40, resizeGrip = 5, keyboardResizeStep = 8;
    Size preferredSize{640, 360};
    ListSelectionMode selectionMode = ListSelectionMode::Single;
    std::shared_ptr<TableCellPainter> cellPainter;
    std::function<void(const std::vector<ItemId>&)> onSelectionChanged;
    std::function<void(ItemId)> onActivated;
    std::function<void(TableSort)> onSortRequested; // Model is unchanged; application supplies sorted snapshot.
    std::function<void(ColumnId, float)>
        onColumnResized; // Completed pointer drag or keyboard resize, actual change only.
};
/*! Fixed-height virtualized table composed with ListView. Context must outlive it.
 * Header and body share column geometry and horizontal scroll position. Header is
 * one Tab stop; body is another. Setters are silent; input callbacks may destroy
 * the controller/replace models or columns. Pending header actions cancel on changes.
 */
class TableView
{
public:
    TableView(Context& context, WidgetId parent = {}, std::shared_ptr<const TableModel> model = {},
              std::vector<TableColumn> columns = {}, TableViewOptions options = {});
    ~TableView();
    TableView(const TableView&) = delete;
    TableView& operator=(const TableView&) = delete;
    WidgetId id() const;
    WidgetId headerId() const;
    WidgetId bodyId() const;
    WidgetId viewportId() const;
    void setModel(std::shared_ptr<const TableModel> model);
    void setProjection(std::shared_ptr<const TableModel> model, std::vector<ItemId> enabledItems);
    std::shared_ptr<const TableModel> model() const;
    void setColumns(std::vector<TableColumn> columns); // Validate before replacing; keeps active/sort column by ID.
    std::vector<TableColumn> columns() const;
    void setColumnWidth(ColumnId id, float width); // Clamped to limits/total extent. Cancels pending header input.
    std::optional<Bounds> headerCellBounds(ColumnId id) const; // Header-local, unclipped; map through headerId().
    void setSort(std::optional<TableSort> sort);               // Existing sortable column only; null clears indicator.
    std::optional<TableSort> sort() const;
    void setSelection(std::vector<ItemId> ids);
    std::vector<ItemId> selection() const;
    std::optional<ItemId> currentItem() const;
    ListRowRange visibleRows() const;
    bool scrollToItem(ItemId id);
    bool setScrollOffset(float offset);
    float scrollOffset() const;
    bool setHorizontalScrollOffset(float offset);
    float horizontalScrollOffset() const;

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace vectorgl::ui
