#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "vectorgl/ui/containers.hpp"

namespace vectorgl::ui
{
using ItemId = std::uint64_t; // Nonzero, unique within a model; independent of row order.

/*! Immutable UI-thread snapshot. ID/enabled metadata is indexed once on setModel;
 * text is requested only for painted rows. Methods must not mutate the UI/model.
 * Keep IDs stable when producing filtered/sorted replacement snapshots. */
class ListModel
{
public:
    virtual ~ListModel() = default;
    virtual std::size_t rowCount() const = 0;
    virtual ItemId itemId(std::size_t row) const = 0;
    virtual std::string_view text(std::size_t row) const = 0;
    virtual bool enabled(std::size_t) const
    {
        return true;
    }
};
struct ListItem
{
    ItemId id = 0;
    std::string text;
    bool enabled = true;
};
class VectorListModel final : public ListModel
{
public:
    explicit VectorListModel(std::vector<ListItem> items);
    std::size_t rowCount() const override;
    ItemId itemId(std::size_t row) const override;
    std::string_view text(std::size_t row) const override;
    bool enabled(std::size_t row) const override;

private:
    std::vector<ListItem> items_;
};
struct ListRowRange
{
    std::size_t first = 0, end = 0; // Half-open; includes partially visible rows.
    bool operator==(const ListRowRange&) const = default;
};
ListRowRange visibleListRows(std::size_t count, float rowHeight, float offset, float viewportHeight);
struct ListRow
{
    ItemId id;
    std::size_t index;
    Bounds bounds;
    bool enabled, selected, current, focused;
};
/*! Optional rendering strategy. Called only for visible rows, inside a row clip
 * and scoped Canvas state. No UI/model mutation or beginFrame/endFrame here. */
class ListRowPainter
{
public:
    virtual ~ListRowPainter() = default;
    virtual void paint(PaintContext& context, const ListModel& model, const ListRow& row) = 0;
};
enum class ListSelectionMode
{
    Single,
    Multiple
};
struct ListViewOptions
{
    float rowHeight = 36;
    Size preferredSize{320, 280};
    ListSelectionMode selectionMode = ListSelectionMode::Single;
    std::shared_ptr<ListRowPainter> rowPainter; // Null selects the themed text painter.
    std::function<void(const std::vector<ItemId>&)> onSelectionChanged;
    std::function<void(ItemId)> onActivated; // Enter release; repeats do not activate.
    // Optional composite-control input filter. nullopt keeps normal handling;
    // an EventReply replaces it. Notifications must return nullopt.
    std::function<std::optional<EventReply>(const Event&)> eventFilter;
    bool horizontalScroll = false; // Optional minimum content width; vertical-only behavior remains the default.
};
/*! Fixed-height virtual list: a constant-size widget subtree, O(visible rows)
 * painting, O(N) snapshot indexing, O(selected rows) selection storage.
 * Context must outlive this controller. One Tab stop; disabled rows are skipped.
 * Programmatic setters/model replacement are silent. Replacement retains selected
 * IDs still present/enabled; a removed current row falls to the nearest enabled row
 * at/after its former index, then before it. Selection may become empty.
 */
class ListView
{
public:
    ListView(Context& context, WidgetId parent = {}, std::shared_ptr<const ListModel> model = {},
             ListViewOptions options = {});
    ~ListView();
    ListView(const ListView&) = delete;
    ListView& operator=(const ListView&) = delete;
    WidgetId id() const;
    WidgetId viewportId() const;
    void setModel(std::shared_ptr<const ListModel> model); // Null is empty; invalid metadata leaves old model intact.
    // Filtered projection + complete enabled ID universe. Hidden selections survive;
    // removing an ID from the universe removes its selection. Published atomically.
    void setProjection(std::shared_ptr<const ListModel> model, std::vector<ItemId> enabledItems);
    std::shared_ptr<const ListModel> model() const;
    void setSelection(std::vector<ItemId> ids); // Existing enabled IDs only; duplicates collapse.
    std::vector<ItemId> selection() const;      // Full model/universe order; safe across callbacks/model replacement.
    std::optional<ItemId> currentItem() const;
    void setCurrentItem(ItemId id);                           // Silent current/anchor change; selection is unchanged.
    bool navigateToItem(ItemId id, Modifiers modifiers = {}); // User navigation semantics, reveal and notification.
    bool scrollToItem(ItemId id);
    bool setScrollOffset(float offset);
    float scrollOffset() const;
    void setContentWidth(float width); // Minimum row extent; requires horizontalScroll for nonzero width.
    bool setHorizontalScrollOffset(float offset);
    float horizontalScrollOffset() const;
    ListRowRange visibleRows() const;

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace vectorgl::ui
