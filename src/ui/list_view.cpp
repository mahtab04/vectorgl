#include "vectorgl/ui/list_view.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "control_detail.hpp"
#include "vectorgl/detail/utf8.hpp"

namespace vectorgl::ui
{
VectorListModel::VectorListModel(std::vector<ListItem> items) : items_(std::move(items)) {}
std::size_t VectorListModel::rowCount() const
{
    return items_.size();
}
ItemId VectorListModel::itemId(std::size_t row) const
{
    return items_.at(row).id;
}
std::string_view VectorListModel::text(std::size_t row) const
{
    return items_.at(row).text;
}
bool VectorListModel::enabled(std::size_t row) const
{
    return items_.at(row).enabled;
}

ListRowRange visibleListRows(std::size_t count, float height, float offset, float viewport)
{
    if (!std::isfinite(height) || height <= 0 || !std::isfinite(offset) || offset < 0 || !std::isfinite(viewport) ||
        viewport < 0)
        throw std::invalid_argument("Invalid list row geometry");
    const auto bounded = [count](double value)
    { return value >= double(count) ? count : static_cast<std::size_t>(value); };
    const auto first = bounded(std::floor(double(offset) / height));
    if (viewport == 0)
        return {first, first};
    return {first, bounded(std::ceil((double(offset) + viewport) / height))};
}
namespace
{
class TextRowPainter final : public ListRowPainter
{
    void paint(PaintContext& p, const ListModel& model, const ListRow& row) override
    {
        const auto b = row.bounds;
        const auto& theme = p.theme();
        p.fillRect(b, row.selected ? theme.pressed : theme.surface);
        if (row.current && row.focused)
        {
            const float line = std::min({2.0f, b.width / 2, b.height / 2});
            p.fillRect({b.x, b.y, b.width, line}, theme.accent);
            p.fillRect({b.x, b.y + b.height - line, b.width, line}, theme.accent);
            p.fillRect({b.x, b.y, line, b.height}, theme.accent);
            p.fillRect({b.x + b.width - line, b.y, line, b.height}, theme.accent);
        }
        auto& c = p.canvas();
        if (!theme.fontPath.empty() && c.setFont(theme.fontPath, theme.fontSize))
        {
            c.setFillColor(row.enabled ? theme.text : theme.muted);
            c.clipRect(b.x + std::min(10.0f, b.width), b.y, std::max(0.0f, b.width - 20), b.height);
            c.fillText(vectorgl::detail::singleLine(model.text(row.index)), b.x + 10,
                       b.y + std::max(0.0f, (b.height - c.lineHeight()) / 2));
        }
    }
};
} // namespace
struct ListView::State final : WidgetPainter
{
    struct Entry
    {
        ItemId id;
        bool enabled;
    };
    Context& ui;
    ListViewOptions options;
    WidgetId root, rowsWidget;
    std::unique_ptr<ScrollView> scroll;
    std::shared_ptr<const ListModel> model;
    std::vector<Entry> entries;
    std::vector<std::size_t> enabled;
    std::unordered_map<ItemId, std::size_t> indices;
    std::unordered_set<ItemId> selected;
    std::optional<ItemId> current, anchor, pressed;
    Modifiers pressModifiers;
    bool pointerPress = false;
    std::uint64_t pointer = 0;
    Key activationKey = Key::None;
    State(Context& c, ListViewOptions value) : ui(c), options(std::move(value))
    {
        detail::validPreferred(options.preferredSize);
        if (!std::isfinite(options.rowHeight) || options.rowHeight <= 0 ||
            (options.selectionMode != ListSelectionMode::Single &&
             options.selectionMode != ListSelectionMode::Multiple))
            throw std::invalid_argument("Invalid list view options");
        if (!options.rowPainter)
            options.rowPainter = std::make_shared<TextRowPainter>();
    }
    std::vector<ItemId> selection() const
    {
        std::vector<ItemId> result(selected.begin(), selected.end());
        std::sort(result.begin(), result.end(), [&](ItemId a, ItemId b) { return indices.at(a) < indices.at(b); });
        return result;
    }
    ListRowRange visible() const
    {
        const auto b = ui.bounds(scroll->viewportId());
        return visibleListRows(entries.size(), options.rowHeight, scroll->offset().y, b ? b->height : 0);
    }
    bool reveal(ItemId id)
    {
        const auto found = indices.find(id);
        const auto b = ui.bounds(scroll->viewportId());
        if (found == indices.end() || !b)
            return false;
        const float top = float(found->second) * options.rowHeight;
        float offset = scroll->offset().y;
        if (top < offset || b->height < options.rowHeight)
            offset = top;
        else if (top + options.rowHeight > offset + b->height)
            offset = top + options.rowHeight - b->height;
        return scroll->setOffset({0, offset});
    }
    void replace(std::shared_ptr<const ListModel> next)
    {
        // Validate and allocate before publishing any new state.
        const auto count = next ? next->rowCount() : 0;
        // Bound float coordinate precision to at least a logical pixel for practical row sizes.
        if (double(count) * options.rowHeight > 16777216.0 || count > 1000000)
            throw std::length_error("List snapshot exceeds supported extent/row limit");
        std::vector<Entry> newEntries;
        std::vector<std::size_t> newEnabled;
        std::unordered_map<ItemId, std::size_t> newIndices;
        newEntries.reserve(count);
        newIndices.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto id = next->itemId(i);
            if (!id || !newIndices.emplace(id, i).second)
                throw std::invalid_argument("List item IDs must be nonzero and unique");
            const bool available = next->enabled(i);
            newEntries.push_back({id, available});
            if (available)
                newEnabled.push_back(i);
        }
        auto retained = selected;
        std::erase_if(retained,
                      [&](ItemId id)
                      {
                          const auto at = newIndices.find(id);
                          return at == newIndices.end() || !newEntries[at->second].enabled;
                      });
        auto retainedCurrent = current;
        const auto exists = [&](std::optional<ItemId> id)
        {
            const auto at = id ? newIndices.find(*id) : newIndices.end();
            return at != newIndices.end() && newEntries[at->second].enabled;
        };
        if (!exists(retainedCurrent))
        {
            const std::size_t oldIndex = current ? indices.at(*current) : 0;
            auto at = std::lower_bound(newEnabled.begin(), newEnabled.end(), oldIndex);
            if (at == newEnabled.end() && !newEnabled.empty())
                --at;
            retainedCurrent = at == newEnabled.end() ? std::nullopt : std::optional<ItemId>(newEntries[*at].id);
        }
        auto retainedAnchor = exists(anchor) ? anchor : retainedCurrent;
        if (ui.captured() == root)
            ui.cancelCapture(CancelReason::Unavailable);
        pressed.reset();
        pointerPress = false;
        activationKey = Key::None;
        model = std::move(next);
        entries.swap(newEntries);
        enabled.swap(newEnabled);
        indices.swap(newIndices);
        selected.swap(retained);
        current = retainedCurrent;
        anchor = retainedAnchor;
        LayoutHints hints;
        hints.height = float(count) * options.rowHeight;
        ui.setLayoutHints(rowsWidget, hints);
        ui.invalidate(Invalidation::All);
    }
    void choose(ItemId id, Modifiers modifiers, bool focusOnly = false)
    {
        if (!indices.contains(id) || !entries[indices.at(id)].enabled)
            return;
        auto before = selected;
        current = id;
        if (!focusOnly)
        {
            const bool multi = options.selectionMode == ListSelectionMode::Multiple;
            const bool toggle = modifiers.control || modifiers.meta;
            if (multi && modifiers.shift && anchor && indices.contains(*anchor))
            {
                if (!toggle)
                    selected.clear();
                const auto low = std::min(indices.at(*anchor), indices.at(id));
                const auto high = std::max(indices.at(*anchor), indices.at(id));
                for (auto at = std::lower_bound(enabled.begin(), enabled.end(), low);
                     at != enabled.end() && *at <= high; ++at)
                    selected.insert(entries[*at].id);
            }
            else
            {
                if (!multi || !toggle)
                    selected.clear();
                if (multi && toggle && selected.contains(id))
                    selected.erase(id);
                else
                    selected.insert(id);
                anchor = id;
            }
        }
        reveal(id);
        ui.invalidate(Invalidation::Paint | Invalidation::Semantics);
        if (before != selected && options.onSelectionChanged)
        {
            const auto values = selection();
            auto callback = options.onSelectionChanged;
            callback(values); // May replace the model/remove this control.
        }
    }
    std::optional<ItemId> atPoint(const Event& e) const
    {
        const auto view = ui.bounds(scroll->viewportId());
        const auto p = ui.mapFromRoot(scroll->viewportId(), {e.x, e.y});
        if (!view || !p || !view->contains(p->x, p->y))
            return {};
        const auto row = std::floor((double(p->y - view->y) + scroll->offset().y) / options.rowHeight);
        if (row < 0 || row >= double(entries.size()))
            return {};
        const auto& entry = entries[static_cast<std::size_t>(row)];
        return entry.enabled ? std::optional<ItemId>(entry.id) : std::nullopt;
    }
    EventReply event(const Event& e)
    {
        // Cleanup precedes client filters, including filters that throw.
        if (e.type == EventType::Cancel || e.type == EventType::FocusLost)
        {
            pressed.reset();
            pointerPress = false;
            activationKey = Key::None;
        }
        if (options.eventFilter)
        {
            auto callback = options.eventFilter;
            const auto reply = callback(e);
            if (!ui.contains(root))
                return {true};
            const bool cleanup = e.type == EventType::Cancel || e.type == EventType::FocusLost ||
                                 e.type == EventType::FocusGained || e.type == EventType::Timer ||
                                 e.type == EventType::LayoutChanged || e.type == EventType::FocusWithin;
            if (reply && !cleanup)
                return *reply;
        }
        if (e.type == EventType::Cancel || e.type == EventType::FocusLost)
        {
            return {};
        }
        if (e.type == EventType::FocusGained)
        {
            if (current && !pointerPress)
                reveal(*current);
            return {};
        }
        if (e.type == EventType::PointerDown && e.button == PointerButton::Primary)
        {
            pressed = atPoint(e);
            pointerPress = true;
            pressModifiers = e.modifiers;
            pointer = e.pointer;
            return {true, true, true};
        }
        if (pointerPress && e.type == EventType::PointerUp && e.button == PointerButton::Primary &&
            e.pointer == pointer)
        {
            const auto target = atPoint(e), original = pressed;
            pressed.reset();
            pointerPress = false;
            if (target && target == original)
                choose(*target, pressModifiers);
            return {true};
        }
        if (e.type == EventType::KeyDown && e.key == Key::Escape && pointerPress)
        {
            ui.cancelCapture();
            return {true};
        }
        if (e.key == Key::Enter &&
            (e.type == EventType::KeyDown || e.type == EventType::KeyRepeat || e.type == EventType::KeyUp))
        {
            if (e.type == EventType::KeyDown)
                activationKey = Key::Enter;
            if (e.type == EventType::KeyUp && activationKey == Key::Enter)
            {
                activationKey = Key::None;
                if (current && options.onActivated)
                {
                    auto callback = options.onActivated;
                    callback(*current);
                }
            }
            return {true};
        }
        if (e.type == EventType::KeyUp && e.key == Key::Space)
            return {true};
        if (e.type != EventType::KeyDown && e.type != EventType::KeyRepeat)
            return {};
        const bool command = e.modifiers.control || e.modifiers.meta;
        if (e.key == Key::A && command)
        {
            if (options.selectionMode == ListSelectionMode::Multiple)
            {
                const auto before = selected;
                for (auto row : enabled)
                    selected.insert(entries[row].id);
                ui.invalidate(Invalidation::Paint | Invalidation::Semantics);
                if (before != selected && options.onSelectionChanged)
                {
                    auto callback = options.onSelectionChanged;
                    const auto values = selection();
                    callback(values);
                }
            }
            return {true};
        }
        if (e.key == Key::Space)
        {
            if (current && e.type == EventType::KeyDown)
                choose(*current, e.modifiers);
            return {true};
        }
        if (e.key != Key::Up && e.key != Key::Down && e.key != Key::Home && e.key != Key::End && e.key != Key::PageUp &&
            e.key != Key::PageDown)
            return {};
        if (enabled.empty())
            return {true};
        const auto old = current ? indices.at(*current) : enabled.front();
        auto at = std::lower_bound(enabled.begin(), enabled.end(), old);
        std::size_t position = static_cast<std::size_t>(at - enabled.begin());
        if (e.key == Key::Home)
            position = 0;
        else if (e.key == Key::End)
            position = enabled.size() - 1;
        else if (e.key == Key::Up)
            position = position ? position - 1 : 0;
        else if (e.key == Key::Down)
            position = std::min(position + 1, enabled.size() - 1);
        else
        {
            const auto b = ui.bounds(scroll->viewportId());
            const auto step = std::max(1.0, std::floor(double(b ? b->height : 0) / options.rowHeight));
            const auto target = e.key == Key::PageUp ? std::max(0.0, double(old) - step)
                                                     : std::min(double(entries.size() - 1), double(old) + step);
            if (e.key == Key::PageUp)
            {
                at = std::upper_bound(enabled.begin(), enabled.end(), static_cast<std::size_t>(target));
                if (at != enabled.begin())
                    --at;
            }
            else
            {
                at = std::lower_bound(enabled.begin(), enabled.end(), static_cast<std::size_t>(target));
                if (at == enabled.end())
                    --at;
            }
            position = static_cast<std::size_t>(at - enabled.begin());
        }
        choose(entries[enabled[position]].id, e.modifiers, command && !e.modifiers.shift);
        return {true};
    }
    void paint(PaintContext& p) override
    {
        const auto range = visible();
        const auto snapshot = model;
        if (!snapshot)
            return;
        const auto b = p.bounds();
        for (auto row = range.first; row < range.end; ++row)
        {
            const auto& entry = entries[row];
            ListRow data{entry.id,
                         row,
                         {b.x, b.y + float(row) * options.rowHeight, b.width, options.rowHeight},
                         p.enabled() && entry.enabled,
                         selected.contains(entry.id),
                         current == entry.id,
                         ui.focused() == root};
            auto& c = p.canvas();
            c.save();
            try
            {
                c.clipRect(data.bounds.x, data.bounds.y, data.bounds.width, data.bounds.height);
                options.rowPainter->paint(p, *snapshot, data);
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
ListView::ListView(Context& ui, WidgetId parent, std::shared_ptr<const ListModel> model, ListViewOptions options)
    : state_(std::make_shared<State>(ui, std::move(options)))
{
    auto state = state_;
    try
    {
        Widget root;
        root.parent = parent;
        root.focusable = true;
        root.clipChildren = true;
        root.layout = std::make_shared<RowLayout>(Padding{2, 2, 2, 2});
        root.painter = std::make_shared<SurfacePainter>(SurfaceRole::Panel);
        root.onEvent = [state](Context&, WidgetId, const Event& e) { return state->event(e); };
        state->root = ui.add(root);
        ScrollOptions scrollOptions;
        scrollOptions.focusable = false;
        scrollOptions.preferredSize = state->options.preferredSize;
        state->scroll = std::make_unique<ScrollView>(ui, state->root, scrollOptions);
        LayoutHints flex;
        flex.flex = 1;
        ui.setLayoutHints(state->scroll->id(), flex);
        state->scroll->setContentLayout(std::make_shared<ColumnLayout>());
        Widget rows;
        rows.parent = state->scroll->content();
        rows.painter = state;
        state->rowsWidget = ui.add(rows);
        state->replace(std::move(model));
    }
    catch (...)
    {
        detail::remove(ui, state->root);
        throw;
    }
}
ListView::~ListView()
{
    detail::remove(state_->ui, state_->root);
}
WidgetId ListView::id() const
{
    return state_->root;
}
WidgetId ListView::viewportId() const
{
    return state_->scroll->viewportId();
}
void ListView::setModel(std::shared_ptr<const ListModel> model)
{
    state_->replace(std::move(model));
}
std::shared_ptr<const ListModel> ListView::model() const
{
    return state_->model;
}
std::vector<ItemId> ListView::selection() const
{
    return state_->selection();
}
std::optional<ItemId> ListView::currentItem() const
{
    return state_->current;
}
void ListView::setCurrentItem(ItemId id)
{
    const auto at = state_->indices.find(id);
    if (at == state_->indices.end() || !state_->entries[at->second].enabled)
        throw std::invalid_argument("Current item must identify an enabled list row");
    state_->current = state_->anchor = id;
    state_->reveal(id);
    state_->ui.invalidate(Invalidation::Paint | Invalidation::Semantics);
}
bool ListView::navigateToItem(ItemId id, Modifiers modifiers)
{
    auto state = state_; // The selection callback may destroy this controller.
    const auto at = state->indices.find(id);
    const auto modal = state->ui.modalRoot();
    if (at == state->indices.end() || !state->entries[at->second].enabled || !state->ui.isAvailable(state->root) ||
        (modal && !state->ui.isDescendant(state->root, modal)))
        return false;
    state->choose(id, modifiers, (modifiers.control || modifiers.meta) && !modifiers.shift);
    return true;
}
bool ListView::scrollToItem(ItemId id)
{
    return state_->reveal(id);
}
bool ListView::setScrollOffset(float offset)
{
    return state_->scroll->setOffset({0, offset});
}
float ListView::scrollOffset() const
{
    return state_->scroll->offset().y;
}
ListRowRange ListView::visibleRows() const
{
    return state_->visible();
}
void ListView::setSelection(std::vector<ItemId> ids)
{
    std::unordered_set<ItemId> next;
    for (auto id : ids)
    {
        const auto at = state_->indices.find(id);
        if (at == state_->indices.end() || !state_->entries[at->second].enabled)
            throw std::invalid_argument("Selection must identify enabled list items");
        next.insert(id);
    }
    if (state_->options.selectionMode == ListSelectionMode::Single && next.size() > 1)
        throw std::invalid_argument("Single list selection accepts at most one item");
    state_->selected.swap(next);
    if (!ids.empty())
        state_->current = state_->anchor = ids.front();
    if (state_->current)
        state_->reveal(*state_->current);
    state_->ui.invalidate(Invalidation::Paint | Invalidation::Semantics);
}
} // namespace vectorgl::ui
