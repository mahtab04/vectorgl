#pragma once

#include <unordered_map>

#include "vectorgl/ui/list_view.hpp"

namespace vectorgl::ui
{
/*! Immutable UI-thread forest. Reachable IDs are nonzero and globally unique.
 * A node may have only one parent; cycles/shared children are rejected.
 * Metadata is indexed on replacement; text is queried only for painted rows.
 * Methods/painters must not mutate the UI or snapshot. */
class TreeModel
{
public:
    virtual ~TreeModel() = default;
    virtual std::size_t rootCount() const = 0;
    virtual ItemId rootId(std::size_t index) const = 0;
    virtual std::size_t childCount(ItemId parent) const = 0;
    virtual ItemId childId(ItemId parent, std::size_t index) const = 0;
    virtual std::string_view text(ItemId id) const = 0;
    virtual bool enabled(ItemId) const
    {
        return true;
    }
};
struct TreeItem
{
    ItemId id = 0, parent = 0; // parent == 0 is a root. Input order determines sibling order.
    std::string text;
    bool enabled = true;
};
class VectorTreeModel final : public TreeModel
{
public:
    explicit VectorTreeModel(std::vector<TreeItem> items);
    std::size_t rootCount() const override;
    ItemId rootId(std::size_t index) const override;
    std::size_t childCount(ItemId parent) const override;
    ItemId childId(ItemId parent, std::size_t index) const override;
    std::string_view text(ItemId id) const override;
    bool enabled(ItemId id) const override;

private:
    std::vector<TreeItem> items_;
    std::unordered_map<ItemId, std::size_t> indices_;
    std::vector<ItemId> roots_;
    std::vector<std::vector<ItemId>> children_;
};
struct TreeRow
{
    ListRow row;
    std::size_t depth;
    bool hasChildren, expanded;
    Bounds disclosureBounds, textBounds;
};
/*! Optional complete row painter, including disclosure decoration. TreeView
 * retains disclosure hit testing. Each call has enforced row/ancestor clips and
 * scoped Canvas state. No UI/model mutation or beginFrame/endFrame. */
class TreeRowPainter
{
public:
    virtual ~TreeRowPainter() = default;
    virtual void paint(PaintContext& context, const TreeModel& model, const TreeRow& row) = 0;
};
struct TreeViewOptions
{
    float rowHeight = 36, indentation = 20, disclosureWidth = 24;
    Size preferredSize{320, 320};
    ListSelectionMode selectionMode = ListSelectionMode::Single;
    std::shared_ptr<TreeRowPainter> rowPainter;
    std::function<void(const std::vector<ItemId>&)> onSelectionChanged;
    std::function<void(ItemId)> onActivated;
    std::function<void(ItemId, bool expanded)> onExpansionChanged;
};
/*! Fixed-height virtual tree composed with ListView. Context must outlive it.
 * Initially collapsed, with one Tab stop. Selection retains hidden enabled
 * nodes; collapse keeps selection and moves hidden current to a surviving
 * enabled ancestor. Model replacement preserves expansion and selection by ID.
 * Programmatic setters are silent; input callbacks may replace/destroy the tree.
 */
class TreeView
{
public:
    TreeView(Context& context, WidgetId parent = {}, std::shared_ptr<const TreeModel> model = {},
             TreeViewOptions options = {});
    ~TreeView();
    TreeView(const TreeView&) = delete;
    TreeView& operator=(const TreeView&) = delete;
    WidgetId id() const;
    WidgetId viewportId() const;
    void setModel(std::shared_ptr<const TreeModel> model); // Invalid snapshots leave the existing tree intact.
    std::shared_ptr<const TreeModel> model() const;
    bool isExpanded(ItemId id) const;
    bool setExpanded(ItemId id, bool expanded);     // Unknown ID throws; a leaf is a no-op.
    void setExpandedItems(std::vector<ItemId> ids); // Existing branches only; duplicates collapse.
    std::vector<ItemId> expandedItems() const;
    void setSelection(std::vector<ItemId> ids); // Existing enabled IDs, including hidden descendants.
    std::vector<ItemId> selection() const;
    std::optional<ItemId> currentItem() const;
    std::size_t visibleItemCount() const;
    ItemId visibleItem(std::size_t row) const;
    ListRowRange visibleRows() const;
    bool scrollToItem(ItemId id); // Only visible items; does not expand ancestors.
    bool setScrollOffset(float offset);
    float scrollOffset() const;

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace vectorgl::ui
