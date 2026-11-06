#include "vectorgl/ui/table_view.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "control_detail.hpp"
#include "vectorgl/detail/utf8.hpp"

namespace vectorgl::ui
{
namespace
{
constexpr float maximumExtent = 16777216;
}
VectorTableModel::VectorTableModel(std::vector<TableItem> items) : items_(std::move(items))
{
    if (items_.size() > 1000000)
        throw std::length_error("Table exceeds supported row count");
    cells_.resize(items_.size());
    std::unordered_set<ItemId> ids;
    for (std::size_t row = 0; row < items_.size(); ++row)
    {
        const auto& item = items_[row];
        if (!item.id || !ids.insert(item.id).second)
            throw std::invalid_argument("Table row IDs must be nonzero and unique");
        if (item.cells.size() > 4096)
            throw std::length_error("Table exceeds supported cell count");
        for (std::size_t cell = 0; cell < item.cells.size(); ++cell)
            if (!item.cells[cell].column || !cells_[row].emplace(item.cells[cell].column, cell).second)
                throw std::invalid_argument("Cell column IDs must be nonzero and unique within a row");
    }
}
std::size_t VectorTableModel::rowCount() const
{
    return items_.size();
}
ItemId VectorTableModel::itemId(std::size_t row) const
{
    return items_.at(row).id;
}
bool VectorTableModel::enabled(std::size_t row) const
{
    return items_.at(row).enabled;
}
std::string_view VectorTableModel::cellText(std::size_t row, ColumnId column) const
{
    const auto& map = cells_.at(row);
    const auto at = map.find(column);
    return at == map.end() ? std::string_view{} : std::string_view(items_.at(row).cells.at(at->second).text);
}
namespace
{
class TableRows final : public ListModel
{
public:
    std::shared_ptr<const TableModel> source;
    explicit TableRows(std::shared_ptr<const TableModel> value) : source(std::move(value)) {}
    std::size_t rowCount() const override
    {
        return source ? source->rowCount() : 0;
    }
    ItemId itemId(std::size_t row) const override
    {
        return source->itemId(row);
    }
    bool enabled(std::size_t row) const override
    {
        return source->enabled(row);
    }
    std::string_view text(std::size_t) const override
    {
        return {};
    }
};
class TextCellPainter final : public TableCellPainter
{
    void paint(PaintContext& p, const TableModel& model, const TableCell& cell) override
    {
        const auto b = cell.bounds;
        auto& c = p.canvas();
        if (b.width <= 20 || p.theme().fontPath.empty() || !c.setFont(p.theme().fontPath, p.theme().fontSize))
            return;
        c.clipRect(b.x + 10, b.y, b.width - 20, b.height);
        c.setFillColor(cell.row.enabled ? p.theme().text : p.theme().muted);
        TextLayoutOptions text;
        text.maxWidth = b.width - 20;
        text.align = cell.align;
        text.wrap = TextWrap::None;
        c.fillText(vectorgl::detail::singleLine(model.cellText(cell.row.index, cell.column)), b.x + 10,
                   b.y + std::max(0.0f, (b.height - c.lineHeight()) / 2), text);
    }
};
} // namespace
struct TableView::State
{
    Context& ui;
    TableViewOptions options;
    WidgetId root, header;
    std::unique_ptr<ListView> list;
    std::shared_ptr<const TableModel> model;
    std::vector<TableColumn> columns;
    std::vector<float> edges{0};
    std::unordered_map<ColumnId, std::size_t> indices;
    std::optional<TableSort> sorting;
    std::size_t active = 0;
    std::optional<std::size_t> pressed;
    bool resizing = false;
    float pressX = 0, originalWidth = 0;
    Bounds pressBounds;
    std::uint64_t pointer = 0;
    Key activation = Key::None;
    State(Context& context, TableViewOptions value) : ui(context), options(std::move(value))
    {
        detail::validPreferred(options.preferredSize);
        for (float v : {options.headerHeight, options.resizeGrip, options.keyboardResizeStep})
            if (!std::isfinite(v) || v <= 0 || v > maximumExtent)
                throw std::invalid_argument("Invalid table geometry");
        if (!options.cellPainter)
            options.cellPainter = std::make_shared<TextCellPainter>();
    }
    Bounds band() const
    {
        const auto h = ui.bounds(header), v = ui.bounds(list->viewportId());
        if (!h || !v)
            return {};
        const auto rootPoint = ui.mapToRoot(list->viewportId(), {v->x, v->y});
        const auto local = rootPoint ? ui.mapFromRoot(header, *rootPoint) : std::nullopt;
        return local ? Bounds{local->x, h->y, v->width, h->height} : Bounds{};
    }
    Bounds headerBounds(std::size_t column) const
    {
        const auto b = band();
        return {b.x + edges[column] - list->horizontalScrollOffset(), b.y, columns[column].width, b.height};
    }
    std::size_t firstColumn() const
    {
        const auto at = std::upper_bound(edges.begin(), edges.end(), list->horizontalScrollOffset());
        return at == edges.begin() ? 0 : static_cast<std::size_t>(at - edges.begin() - 1);
    }
    void rebuildEdges()
    {
        double total = 0;
        for (std::size_t i = 0; i < columns.size(); ++i)
        {
            total += columns[i].width;
            edges[i + 1] = static_cast<float>(total);
        }
    }
    bool width(std::size_t index, float requested)
    {
        auto& column = columns[index];
        const double others = double(edges.back()) - column.width;
        const float limit = static_cast<float>(std::min(double(column.maximumWidth), double(maximumExtent) - others));
        const float actual = std::clamp(requested, column.minimumWidth, std::max(column.minimumWidth, limit));
        if (actual == column.width)
            return false;
        column.width = actual;
        rebuildEdges();
        list->setContentWidth(edges.back());
        ui.invalidate(Invalidation::Paint | Invalidation::Semantics);
        return true;
    }
    void clearPress(bool restore)
    {
        const auto old = pressed;
        const bool wasResize = resizing;
        pressed.reset();
        resizing = false;
        activation = Key::None;
        if (restore && old && wasResize && *old < columns.size())
            width(*old, originalWidth);
    }
    void cancel()
    {
        // Context may defer Cancel delivery until the current callback unwinds.
        // Restore against the old column set now, before publishing replacements.
        clearPress(true);
        if (ui.captured() == header)
            ui.cancelCapture(CancelReason::Unavailable);
    }
    void replaceColumns(std::vector<TableColumn> next)
    {
        if (next.size() > 4096)
            throw std::length_error("Table exceeds supported column count");
        std::unordered_map<ColumnId, std::size_t> map;
        std::vector<float> newEdges{0};
        double total = 0;
        for (std::size_t i = 0; i < next.size(); ++i)
        {
            const auto& c = next[i];
            if (!c.id || !map.emplace(c.id, i).second || !std::isfinite(c.width) || !std::isfinite(c.minimumWidth) ||
                !std::isfinite(c.maximumWidth) || c.minimumWidth <= 0 || c.maximumWidth < c.minimumWidth ||
                c.width < c.minimumWidth || c.width > c.maximumWidth ||
                (c.align != TextAlign::Left && c.align != TextAlign::Center && c.align != TextAlign::Right))
                throw std::invalid_argument("Invalid table columns");
            total += c.width;
            if (total > maximumExtent)
                throw std::length_error("Table exceeds supported column extent");
            newEdges.push_back(static_cast<float>(total));
        }
        const auto oldActive = active < columns.size() ? std::optional<ColumnId>(columns[active].id) : std::nullopt;
        auto nextSort = sorting;
        if (nextSort && (!map.contains(nextSort->column) || !next[map.at(nextSort->column)].sortable))
            nextSort.reset();
        cancel();
        list->setContentWidth(newEdges.back());
        columns.swap(next);
        edges.swap(newEdges);
        indices.swap(map);
        sorting = nextSort;
        active = oldActive && indices.contains(*oldActive) ? indices.at(*oldActive) : 0;
        ui.invalidate(Invalidation::Paint | Invalidation::Semantics);
    }
    void revealColumn()
    {
        if (active >= columns.size())
            return;
        const auto b = band();
        float offset = list->horizontalScrollOffset();
        if (edges[active] < offset || columns[active].width > b.width)
            offset = edges[active];
        else if (edges[active + 1] > offset + b.width)
            offset = edges[active + 1] - b.width;
        list->setHorizontalScrollOffset(offset);
    }
    void requestSort(std::size_t index)
    {
        if (index >= columns.size() || !columns[index].sortable)
            return;
        const TableSort value{columns[index].id, sorting && sorting->column == columns[index].id &&
                                                         sorting->direction == SortDirection::Ascending
                                                     ? SortDirection::Descending
                                                     : SortDirection::Ascending};
        sorting = value;
        ui.invalidate(Invalidation::Paint | Invalidation::Semantics);
        if (options.onSortRequested)
        {
            auto callback = options.onSortRequested;
            callback(value);
        }
    }
    std::optional<std::size_t> hit(const Event& e, bool grip) const
    {
        const auto b = band();
        if (!b.contains(e.localX, e.localY))
            return {};
        const float x = e.localX - b.x + list->horizontalScrollOffset();
        if (grip)
        {
            const auto edge = std::lower_bound(edges.begin() + 1, edges.end(), x - options.resizeGrip);
            for (auto at = edge; at != edges.end() && *at <= x + options.resizeGrip; ++at)
            {
                const auto index = static_cast<std::size_t>(at - edges.begin() - 1);
                if (columns[index].resizable)
                    return index;
            }
            return {};
        }
        const auto at = std::upper_bound(edges.begin(), edges.end(), x);
        const auto index = at == edges.begin() ? columns.size() : static_cast<std::size_t>(at - edges.begin() - 1);
        return index < columns.size() ? std::optional<std::size_t>(index) : std::nullopt;
    }
    EventReply event(const Event& e)
    {
        if (e.type == EventType::Cancel || e.type == EventType::FocusLost)
        {
            clearPress(true);
            return {};
        }
        if (e.type == EventType::FocusGained)
        {
            if (!pressed)
                revealColumn();
            return {};
        }
        if (e.type == EventType::LayoutChanged)
        {
            if (pressed && resizing && ui.bounds(header) != std::optional<Bounds>(pressBounds))
                cancel();
            return {};
        }
        if (e.type == EventType::Wheel)
        {
            cancel();
            const double step = e.wheelUnit == WheelUnit::Lines ? 40 : 1;
            const auto bounded = [](double value)
            { return static_cast<float>(std::clamp(value, 0.0, double(maximumExtent))); };
            list->setHorizontalScrollOffset(bounded(double(list->horizontalScrollOffset()) - e.wheelX * step));
            list->setScrollOffset(bounded(double(list->scrollOffset()) - e.wheelY * step));
            return {true};
        }
        if (e.type == EventType::PointerDown && e.button == PointerButton::Primary)
        {
            clearPress(true);
            auto index = hit(e, true);
            resizing = index.has_value();
            if (!index)
                index = hit(e, false);
            pressed = index;
            if (index)
            {
                active = *index;
                pressX = e.localX;
                originalWidth = columns[*index].width;
                pressBounds = ui.bounds(header).value_or(Bounds{});
                pointer = e.pointer;
            }
            ui.invalidate();
            return {true, true, index.has_value()};
        }
        if (pressed && e.pointer == pointer && activation == Key::None)
        {
            if (e.type == EventType::PointerMove)
            {
                if (resizing)
                    width(*pressed, static_cast<float>(std::clamp(double(originalWidth) + e.localX - pressX, 0.0,
                                                                  double(maximumExtent))));
                return {true};
            }
            if (e.type == EventType::PointerUp && e.button == PointerButton::Primary)
            {
                const auto index = *pressed;
                const bool wasResize = resizing, changed = columns[index].width != originalWidth;
                const auto target = hit(e, false);
                clearPress(false);
                if (wasResize && changed && options.onColumnResized)
                {
                    auto callback = options.onColumnResized;
                    callback(columns[index].id, columns[index].width);
                }
                else if (!wasResize && target == index)
                    requestSort(index);
                return {true};
            }
        }
        if (e.type == EventType::KeyDown && e.key == Key::Escape && pressed)
        {
            cancel();
            return {true};
        }
        if ((e.key == Key::Enter || e.key == Key::Space) &&
            (e.type == EventType::KeyDown || e.type == EventType::KeyRepeat || e.type == EventType::KeyUp))
        {
            if (e.type == EventType::KeyDown && !pressed && active < columns.size())
            {
                pressed = active;
                activation = e.key;
            }
            if (e.type == EventType::KeyUp && activation == e.key && pressed)
            {
                const auto index = *pressed;
                clearPress(false);
                requestSort(index);
            }
            return {true};
        }
        if (e.type != EventType::KeyDown && e.type != EventType::KeyRepeat)
            return {};
        if (e.key != Key::Left && e.key != Key::Right && e.key != Key::Home && e.key != Key::End)
            return {};
        if (columns.empty())
            return {true};
        cancel();
        if (e.modifiers.shift && (e.key == Key::Left || e.key == Key::Right))
        {
            if (columns[active].resizable &&
                width(active, columns[active].width +
                                  (e.key == Key::Left ? -options.keyboardResizeStep : options.keyboardResizeStep)) &&
                options.onColumnResized)
            {
                auto callback = options.onColumnResized;
                callback(columns[active].id, columns[active].width);
            }
            return {true};
        }
        if (e.key == Key::Home)
            active = 0;
        else if (e.key == Key::End)
            active = columns.size() - 1;
        else if (e.key == Key::Left)
            active = active ? active - 1 : 0;
        else
            active = std::min(active + 1, columns.size() - 1);
        revealColumn();
        ui.invalidate();
        return {true};
    }
    struct Rows final : ListRowPainter
    {
        std::weak_ptr<State> owner;
        explicit Rows(std::weak_ptr<State> value) : owner(std::move(value)) {}
        void paint(PaintContext& p, const ListModel&, const ListRow& row) override
        {
            const auto s = owner.lock();
            if (!s || !s->model)
                return;
            const auto& theme = p.theme();
            const auto b = row.bounds;
            p.fillRect(b, row.selected ? theme.pressed : theme.surface);
            if (row.current && row.focused)
            {
                const float line = std::min(2.0f, b.height / 2);
                p.fillRect({b.x, b.y, b.width, line}, theme.accent);
                p.fillRect({b.x, b.y + b.height - line, b.width, line}, theme.accent);
            }
            const auto view = s->ui.bounds(s->list->viewportId());
            const float end = s->list->horizontalScrollOffset() + (view ? view->width : 0);
            auto& c = p.canvas();
            for (auto i = s->firstColumn(); i < s->columns.size() && s->edges[i] < end; ++i)
            {
                const auto& column = s->columns[i];
                const Bounds cell{b.x + s->edges[i], b.y, column.width, b.height};
                c.save();
                try
                {
                    c.clipRect(cell.x, cell.y, cell.width, cell.height);
                    s->options.cellPainter->paint(p, *s->model, {row, column.id, i, cell, column.align});
                }
                catch (...)
                {
                    c.restore();
                    throw;
                }
                c.restore();
                p.fillRect({cell.x + cell.width - 1, cell.y, std::min(1.0f, cell.width), cell.height}, theme.border);
            }
        }
    };
    struct Header final : WidgetPainter
    {
        std::weak_ptr<State> owner;
        explicit Header(std::weak_ptr<State> value) : owner(std::move(value)) {}
        void paint(PaintContext& p) override
        {
            const auto s = owner.lock();
            if (!s)
                return;
            SurfacePainter{SurfaceRole::Panel}.paint(p);
            const auto band = s->band();
            if (band.width <= 0 || band.height <= 0)
                return;
            auto& c = p.canvas();
            c.clipRect(band.x, band.y, band.width, band.height);
            const float end = s->list->horizontalScrollOffset() + band.width;
            for (auto i = s->firstColumn(); i < s->columns.size() && s->edges[i] < end; ++i)
            {
                const auto b = s->headerBounds(i);
                const auto& column = s->columns[i];
                p.fillRect(b, s->pressed == i ? p.theme().pressed : p.theme().surface);
                p.fillRect({b.x + b.width - 1, b.y, std::min(1.0f, b.width), b.height}, p.theme().border);
                if (p.focused() && s->active == i)
                    p.fillRect({b.x, b.y + b.height - 2, b.width, std::min(2.0f, b.height)}, p.theme().accent);
                c.save();
                try
                {
                    c.clipRect(b.x + std::min(10.0f, b.width), b.y, std::max(0.0f, b.width - 20), b.height);
                    if (b.width > 20 && !p.theme().fontPath.empty() &&
                        c.setFont(p.theme().fontPath, p.theme().fontSize))
                    {
                        TextLayoutOptions text;
                        text.maxWidth = std::max(1.0f, b.width - 40);
                        text.wrap = TextWrap::None;
                        text.align = column.align;
                        c.setFillColor(p.enabled() ? p.theme().text : p.theme().muted);
                        c.fillText(vectorgl::detail::singleLine(column.label), b.x + 10,
                                   b.y + std::max(0.0f, (b.height - c.lineHeight()) / 2), text);
                    }
                    if (s->sorting && s->sorting->column == column.id && b.width > 32)
                    {
                        const float x = b.x + b.width - 15, y = b.y + b.height / 2;
                        const float sign = s->sorting->direction == SortDirection::Ascending ? 1.0f : -1.0f;
                        c.setFillColor(p.enabled() ? p.theme().accent : p.theme().muted);
                        c.beginPath();
                        c.moveTo(x - 4, y + sign * 2);
                        c.lineTo(x + 4, y + sign * 2);
                        c.lineTo(x, y - sign * 4);
                        c.closePath();
                        c.fill();
                    }
                }
                catch (...)
                {
                    c.restore();
                    throw;
                }
                c.restore();
            }
        }
    };
};
TableView::TableView(Context& ui, WidgetId parent, std::shared_ptr<const TableModel> model,
                     std::vector<TableColumn> columns, TableViewOptions options)
    : state_(std::make_shared<State>(ui, std::move(options)))
{
    auto s = state_;
    const std::weak_ptr<State> weak = s;
    try
    {
        Widget root;
        root.parent = parent;
        root.clipChildren = true;
        root.layout = std::make_shared<ColumnLayout>();
        s->root = ui.add(root);
        Widget header;
        header.parent = s->root;
        header.focusable = true;
        header.clipChildren = true;
        header.layoutHints.height = s->options.headerHeight;
        header.painter = std::make_shared<State::Header>(weak);
        header.onEvent = [weak](Context&, WidgetId, const Event& e)
        {
            if (auto state = weak.lock())
                return state->event(e);
            return EventReply{};
        };
        s->header = ui.add(header);
        ListViewOptions list;
        list.rowHeight = s->options.rowHeight;
        list.preferredSize = s->options.preferredSize;
        list.horizontalScroll = true;
        list.selectionMode = s->options.selectionMode;
        list.rowPainter = std::make_shared<State::Rows>(weak);
        list.onSelectionChanged = [weak](const auto& ids)
        {
            if (auto state = weak.lock(); state && state->options.onSelectionChanged)
            {
                auto callback = state->options.onSelectionChanged;
                callback(ids);
            }
        };
        list.onActivated = [weak](ItemId id)
        {
            if (auto state = weak.lock(); state && state->options.onActivated)
            {
                auto callback = state->options.onActivated;
                callback(id);
            }
        };
        s->list = std::make_unique<ListView>(ui, s->root, nullptr, std::move(list));
        LayoutHints flex;
        flex.flex = 1;
        ui.setLayoutHints(s->list->id(), flex);
        s->replaceColumns(std::move(columns));
        setModel(std::move(model));
    }
    catch (...)
    {
        detail::remove(ui, s->root);
        throw;
    }
}
TableView::~TableView()
{
    detail::remove(state_->ui, state_->root);
}
WidgetId TableView::id() const
{
    return state_->root;
}
WidgetId TableView::headerId() const
{
    return state_->header;
}
WidgetId TableView::bodyId() const
{
    return state_->list->id();
}
WidgetId TableView::viewportId() const
{
    return state_->list->viewportId();
}
void TableView::setModel(std::shared_ptr<const TableModel> model)
{
    // ListView validates the snapshot before changing its state.
    state_->list->setModel(std::make_shared<TableRows>(model));
    state_->model = std::move(model);
    state_->cancel();
}
std::shared_ptr<const TableModel> TableView::model() const
{
    return state_->model;
}
void TableView::setColumns(std::vector<TableColumn> columns)
{
    state_->replaceColumns(std::move(columns));
}
std::vector<TableColumn> TableView::columns() const
{
    return state_->columns;
}
void TableView::setColumnWidth(ColumnId id, float width)
{
    const auto at = state_->indices.find(id);
    if (at == state_->indices.end() || !std::isfinite(width))
        throw std::invalid_argument("Invalid table column width");
    state_->cancel();
    state_->width(at->second, width);
}
std::optional<Bounds> TableView::headerCellBounds(ColumnId id) const
{
    const auto at = state_->indices.find(id);
    return at == state_->indices.end() ? std::nullopt : std::optional<Bounds>(state_->headerBounds(at->second));
}
void TableView::setSort(std::optional<TableSort> sort)
{
    if (sort &&
        (!state_->indices.contains(sort->column) || !state_->columns[state_->indices.at(sort->column)].sortable ||
         (sort->direction != SortDirection::Ascending && sort->direction != SortDirection::Descending)))
        throw std::invalid_argument("Invalid table sort");
    state_->cancel();
    state_->sorting = sort;
    state_->ui.invalidate();
}
std::optional<TableSort> TableView::sort() const
{
    return state_->sorting;
}
void TableView::setSelection(std::vector<ItemId> ids)
{
    state_->list->setSelection(std::move(ids));
}
std::vector<ItemId> TableView::selection() const
{
    return state_->list->selection();
}
std::optional<ItemId> TableView::currentItem() const
{
    return state_->list->currentItem();
}
ListRowRange TableView::visibleRows() const
{
    return state_->list->visibleRows();
}
bool TableView::scrollToItem(ItemId id)
{
    return state_->list->scrollToItem(id);
}
bool TableView::setScrollOffset(float offset)
{
    return state_->list->setScrollOffset(offset);
}
float TableView::scrollOffset() const
{
    return state_->list->scrollOffset();
}
bool TableView::setHorizontalScrollOffset(float offset)
{
    return state_->list->setHorizontalScrollOffset(offset);
}
float TableView::horizontalScrollOffset() const
{
    return state_->list->horizontalScrollOffset();
}
} // namespace vectorgl::ui
