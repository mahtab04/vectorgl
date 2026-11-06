#include "vectorgl/ui/tree_view.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

#include "control_detail.hpp"
#include "vectorgl/detail/utf8.hpp"

namespace vectorgl::ui
{
VectorTreeModel::VectorTreeModel(std::vector<TreeItem> items) : items_(std::move(items))
{
    if (items_.size() > 1000000)
        throw std::length_error("Tree exceeds supported node count");
    children_.resize(items_.size());
    for (std::size_t i = 0; i < items_.size(); ++i)
        if (!items_[i].id || !indices_.emplace(items_[i].id, i).second)
            throw std::invalid_argument("Tree IDs must be nonzero and unique");
    for (const auto& item : items_)
    {
        if (!item.parent)
            roots_.push_back(item.id);
        else
        {
            const auto at = indices_.find(item.parent);
            if (at == indices_.end())
                throw std::invalid_argument("Tree parent is missing");
            children_[at->second].push_back(item.id);
        }
    }
    auto pending = roots_;
    std::size_t visited = 0;
    while (!pending.empty())
    {
        const auto id = pending.back();
        pending.pop_back();
        ++visited;
        const auto& children = children_[indices_.at(id)];
        pending.insert(pending.end(), children.begin(), children.end());
    }
    if (visited != items_.size())
        throw std::invalid_argument("Tree contains a parent cycle");
}
std::size_t VectorTreeModel::rootCount() const
{
    return roots_.size();
}
ItemId VectorTreeModel::rootId(std::size_t index) const
{
    return roots_.at(index);
}
std::size_t VectorTreeModel::childCount(ItemId parent) const
{
    return children_.at(indices_.at(parent)).size();
}
ItemId VectorTreeModel::childId(ItemId parent, std::size_t index) const
{
    return children_.at(indices_.at(parent)).at(index);
}
std::string_view VectorTreeModel::text(ItemId id) const
{
    return items_.at(indices_.at(id)).text;
}
bool VectorTreeModel::enabled(ItemId id) const
{
    return items_.at(indices_.at(id)).enabled;
}

namespace
{
constexpr std::size_t noParent = std::numeric_limits<std::size_t>::max();
struct IndexedTree
{
    struct Node
    {
        ItemId id;
        std::size_t parent, depth;
        bool enabled;
        std::vector<std::size_t> children;
    };
    std::shared_ptr<const TreeModel> model;
    std::vector<Node> nodes;
    std::vector<std::size_t> roots;
    std::unordered_map<ItemId, std::size_t> indices;
    explicit IndexedTree(std::shared_ptr<const TreeModel> value) : model(std::move(value))
    {
        if (!model)
            return;
        struct Pending
        {
            ItemId id;
            std::size_t parent, depth;
        };
        const auto rootCount = model->rootCount();
        if (rootCount > 1000000)
            throw std::length_error("Tree exceeds supported node count");
        std::vector<Pending> pending;
        for (auto i = rootCount; i > 0; --i)
            pending.push_back({model->rootId(i - 1), noParent, 0});
        while (!pending.empty())
        {
            const auto next = pending.back();
            pending.pop_back();
            if (nodes.size() >= 1000000 || next.depth > 512)
                throw std::length_error("Tree exceeds supported node count/depth");
            const auto index = nodes.size();
            if (!next.id || !indices.emplace(next.id, index).second)
                throw std::invalid_argument("Tree IDs must be nonzero, unique and acyclic");
            const auto count = model->childCount(next.id);
            if (count > 1000000 - nodes.size() - pending.size() - 1)
                throw std::length_error("Tree exceeds supported node count");
            nodes.push_back({next.id, next.parent, next.depth, model->enabled(next.id), {}});
            if (next.parent == noParent)
                roots.push_back(index);
            else
                nodes[next.parent].children.push_back(index);
            for (auto i = count; i > 0; --i)
                pending.push_back({model->childId(next.id, i - 1), index, next.depth + 1});
        }
    }
};
class FlatTree final : public ListModel
{
public:
    std::shared_ptr<const IndexedTree> tree;
    std::vector<std::size_t> rows;
    std::unordered_map<ItemId, std::size_t> indices;
    FlatTree(std::shared_ptr<const IndexedTree> value, const std::unordered_set<ItemId>& expanded)
        : tree(std::move(value))
    {
        std::vector<std::size_t> pending(tree->roots.rbegin(), tree->roots.rend());
        while (!pending.empty())
        {
            const auto index = pending.back();
            pending.pop_back();
            const auto& node = tree->nodes[index];
            indices.emplace(node.id, rows.size());
            rows.push_back(index);
            if (expanded.contains(node.id))
                pending.insert(pending.end(), node.children.rbegin(), node.children.rend());
        }
    }
    const IndexedTree::Node& node(std::size_t row) const
    {
        return tree->nodes.at(rows.at(row));
    }
    std::size_t rowCount() const override
    {
        return rows.size();
    }
    ItemId itemId(std::size_t row) const override
    {
        return node(row).id;
    }
    bool enabled(std::size_t row) const override
    {
        return node(row).enabled;
    }
    std::string_view text(std::size_t row) const override
    {
        return tree->model->text(itemId(row));
    }
};
class TextTreePainter final : public TreeRowPainter
{
    void paint(PaintContext& p, const TreeModel& model, const TreeRow& data) override
    {
        const auto& row = data.row;
        const auto b = row.bounds;
        const auto& theme = p.theme();
        p.fillRect(b, row.selected ? theme.pressed : theme.surface);
        if (row.current && row.focused)
        {
            const auto line = std::min({2.0f, b.width / 2, b.height / 2});
            p.fillRect({b.x, b.y, b.width, line}, theme.accent);
            p.fillRect({b.x, b.y + b.height - line, b.width, line}, theme.accent);
            p.fillRect({b.x, b.y, line, b.height}, theme.accent);
            p.fillRect({b.x + b.width - line, b.y, line, b.height}, theme.accent);
        }
        auto& c = p.canvas();
        if (data.hasChildren && data.disclosureBounds.width > 0)
        {
            const auto d = data.disclosureBounds;
            const float x = d.x + d.width / 2, y = d.y + d.height / 2;
            const float radius = std::min({4.0f, d.width / 3, d.height / 3});
            c.setFillColor(row.enabled ? theme.accent : theme.muted);
            c.beginPath();
            if (data.expanded)
            {
                c.moveTo(x - radius, y - radius / 2);
                c.lineTo(x + radius, y - radius / 2);
                c.lineTo(x, y + radius);
            }
            else
            {
                c.moveTo(x - radius / 2, y - radius);
                c.lineTo(x + radius, y);
                c.lineTo(x - radius / 2, y + radius);
            }
            c.closePath();
            c.fill();
        }
        if (data.textBounds.width > 0 && !theme.fontPath.empty() && c.setFont(theme.fontPath, theme.fontSize))
        {
            const auto text = data.textBounds;
            c.clipRect(text.x, text.y, text.width, text.height);
            c.setFillColor(row.enabled ? theme.text : theme.muted);
            c.fillText(vectorgl::detail::singleLine(model.text(row.id)), text.x,
                       text.y + std::max(0.0f, (text.height - c.lineHeight()) / 2));
        }
    }
};
} // namespace
struct TreeView::State
{
    Context& ui;
    TreeViewOptions options;
    std::unique_ptr<ListView> list;
    std::shared_ptr<const IndexedTree> tree;
    std::shared_ptr<const FlatTree> flat;
    std::unordered_set<ItemId> expanded;
    std::optional<ItemId> disclosurePress;
    std::uint64_t pointer = 0, revision = 0;
    State(Context& context, TreeViewOptions value) : ui(context), options(std::move(value))
    {
        detail::validPreferred(options.preferredSize);
        if (!std::isfinite(options.indentation) || options.indentation <= 0 ||
            !std::isfinite(options.disclosureWidth) || options.disclosureWidth <= 0)
            throw std::invalid_argument("Invalid tree indentation/disclosure width");
        if (!options.rowPainter)
            options.rowPainter = std::make_shared<TextTreePainter>();
    }
    TreeRow rowData(const ListRow& row) const
    {
        const auto& node = flat->node(row.index);
        const auto b = row.bounds;
        const float inset = static_cast<float>(std::min(double(b.width), double(node.depth) * options.indentation));
        const float width = std::min(options.disclosureWidth, std::max(0.0f, b.width - inset));
        const Bounds disclosure{b.x + inset, b.y, width, b.height};
        const Bounds text{disclosure.x + width, b.y, std::max(0.0f, b.width - inset - width - 8), b.height};
        return {row, node.depth, !node.children.empty(), expanded.contains(node.id), disclosure, text};
    }
    struct Painter final : ListRowPainter
    {
        std::weak_ptr<State> owner;
        explicit Painter(std::weak_ptr<State> value) : owner(std::move(value)) {}
        void paint(PaintContext& p, const ListModel&, const ListRow& row) override
        {
            if (auto state = owner.lock())
                state->options.rowPainter->paint(p, *state->tree->model, state->rowData(row));
        }
    };
    void apply(std::shared_ptr<const IndexedTree> nextTree, std::unordered_set<ItemId> nextExpanded)
    {
        auto nextFlat = std::make_shared<FlatTree>(nextTree, nextExpanded);
        auto nextCurrent = list->currentItem();
        // When descendants disappear, prefer a surviving visible enabled ancestor.
        const auto nextAt = nextCurrent ? nextFlat->indices.find(*nextCurrent) : nextFlat->indices.end();
        if (nextCurrent && (nextAt == nextFlat->indices.end() || !nextFlat->enabled(nextAt->second)) && tree)
        {
            auto at = tree->indices.find(*nextCurrent);
            if (at != tree->indices.end())
                for (auto parent = tree->nodes[at->second].parent; parent != noParent;
                     parent = tree->nodes[parent].parent)
                {
                    const auto id = tree->nodes[parent].id;
                    const auto visible = nextFlat->indices.find(id);
                    if (visible != nextFlat->indices.end() && nextFlat->enabled(visible->second))
                    {
                        nextCurrent = id;
                        break;
                    }
                }
        }
        list->setModel(nextFlat); // Validates extent before publishing the hierarchy/expansion state.
        tree = std::move(nextTree);
        flat = std::move(nextFlat);
        expanded.swap(nextExpanded);
        disclosurePress.reset();
        ++revision;
        if (nextCurrent)
        {
            const auto at = flat->indices.find(*nextCurrent);
            if (at != flat->indices.end() && flat->enabled(at->second))
                list->setCurrentItem(*nextCurrent);
        }
    }
    bool change(ItemId id, bool value, bool notify)
    {
        const auto at = tree->indices.find(id);
        if (at == tree->indices.end())
            throw std::invalid_argument("Unknown tree item");
        if (tree->nodes[at->second].children.empty() || expanded.contains(id) == value)
            return false;
        auto next = expanded;
        if (value)
            next.insert(id);
        else
            next.erase(id);
        const auto before = list->selection();
        apply(tree, std::move(next));
        const auto version = revision;
        if (notify && before != list->selection() && options.onSelectionChanged)
        {
            auto callback = options.onSelectionChanged;
            const auto values = list->selection();
            callback(values);
        }
        if (notify && revision == version && ui.contains(list->id()) && options.onExpansionChanged)
        {
            auto callback = options.onExpansionChanged;
            callback(id, value);
        }
        return true;
    }
    std::optional<ItemId> disclosureAt(const Event& e) const
    {
        const auto view = ui.bounds(list->viewportId());
        const auto point = ui.mapFromRoot(list->viewportId(), {e.x, e.y});
        if (!view || !point || !view->contains(point->x, point->y))
            return {};
        const double row = std::floor((double(point->y - view->y) + list->scrollOffset()) / options.rowHeight);
        if (row < 0 || row >= double(flat->rowCount()))
            return {};
        const auto index = static_cast<std::size_t>(row);
        const auto& node = flat->node(index);
        const auto data =
            rowData({node.id, index, {0, 0, view->width, options.rowHeight}, node.enabled, false, false, false});
        if (node.enabled && !node.children.empty() &&
            data.disclosureBounds.contains(point->x - view->x, options.rowHeight / 2))
            return node.id;
        return {};
    }
    std::optional<EventReply> event(const Event& e)
    {
        if (e.type == EventType::Cancel || e.type == EventType::FocusLost)
            disclosurePress.reset();
        if (e.type == EventType::PointerDown && e.button == PointerButton::Primary)
            if (auto id = disclosureAt(e))
            {
                disclosurePress = id;
                pointer = e.pointer;
                list->setCurrentItem(*id);
                return EventReply{true, true, true};
            }
        if (disclosurePress && e.pointer == pointer)
        {
            if (e.type == EventType::PointerMove)
                return EventReply{true};
            if (e.type == EventType::PointerUp && e.button == PointerButton::Primary)
            {
                const auto original = disclosurePress;
                const auto target = disclosureAt(e);
                disclosurePress.reset();
                if (target && target == original)
                    change(*target, !expanded.contains(*target), true);
                return EventReply{true};
            }
        }
        if (e.type == EventType::KeyDown && e.key == Key::Escape && disclosurePress)
        {
            ui.cancelCapture();
            return EventReply{true};
        }
        if ((e.type == EventType::KeyDown || e.type == EventType::KeyRepeat) &&
            (e.key == Key::Left || e.key == Key::Right))
        {
            const auto id = list->currentItem();
            if (!id)
                return EventReply{true};
            const auto& node = tree->nodes[tree->indices.at(*id)];
            if (e.key == Key::Right)
            {
                if (!node.children.empty() && !expanded.contains(*id))
                    change(*id, true, true);
                else
                    for (auto child : node.children)
                        if (tree->nodes[child].enabled)
                        {
                            list->navigateToItem(tree->nodes[child].id, e.modifiers);
                            break;
                        }
            }
            else if (!node.children.empty() && expanded.contains(*id))
                change(*id, false, true);
            else
                for (auto parent = node.parent; parent != noParent; parent = tree->nodes[parent].parent)
                    if (tree->nodes[parent].enabled)
                    {
                        list->navigateToItem(tree->nodes[parent].id, e.modifiers);
                        break;
                    }
            return EventReply{true};
        }
        return {};
    }
};
TreeView::TreeView(Context& ui, WidgetId parent, std::shared_ptr<const TreeModel> model, TreeViewOptions options)
    : state_(std::make_shared<State>(ui, std::move(options)))
{
    auto state = state_;
    const std::weak_ptr<State> weak = state;
    ListViewOptions listOptions;
    listOptions.rowHeight = state->options.rowHeight;
    listOptions.preferredSize = state->options.preferredSize;
    listOptions.selectionMode = state->options.selectionMode;
    listOptions.rowPainter = std::make_shared<State::Painter>(weak);
    listOptions.onSelectionChanged = [weak](const auto& ids)
    {
        if (auto s = weak.lock(); s && s->options.onSelectionChanged)
        {
            auto callback = s->options.onSelectionChanged;
            callback(ids);
        }
    };
    listOptions.onActivated = [weak](ItemId id)
    {
        if (auto s = weak.lock(); s && s->options.onActivated)
        {
            auto callback = s->options.onActivated;
            callback(id);
        }
    };
    listOptions.eventFilter = [weak](const Event& e) -> std::optional<EventReply>
    {
        if (auto s = weak.lock())
            return s->event(e);
        return {};
    };
    state->list = std::make_unique<ListView>(ui, parent, nullptr, std::move(listOptions));
    setModel(std::move(model));
}
TreeView::~TreeView()
{
    detail::remove(state_->ui, state_->list->id());
}
WidgetId TreeView::id() const
{
    return state_->list->id();
}
WidgetId TreeView::viewportId() const
{
    return state_->list->viewportId();
}
void TreeView::setModel(std::shared_ptr<const TreeModel> model)
{
    auto next = std::make_shared<IndexedTree>(std::move(model));
    auto expanded = state_->expanded;
    std::erase_if(expanded,
                  [&](ItemId id)
                  {
                      const auto at = next->indices.find(id);
                      return at == next->indices.end() || next->nodes[at->second].children.empty();
                  });
    state_->apply(std::move(next), std::move(expanded));
}
std::shared_ptr<const TreeModel> TreeView::model() const
{
    return state_->tree->model;
}
bool TreeView::isExpanded(ItemId id) const
{
    return state_->expanded.contains(id);
}
bool TreeView::setExpanded(ItemId id, bool expanded)
{
    return state_->change(id, expanded, false);
}
void TreeView::setExpandedItems(std::vector<ItemId> ids)
{
    std::unordered_set<ItemId> next;
    for (auto id : ids)
    {
        const auto at = state_->tree->indices.find(id);
        if (at == state_->tree->indices.end() || state_->tree->nodes[at->second].children.empty())
            throw std::invalid_argument("Expanded items must identify tree branches");
        next.insert(id);
    }
    if (next != state_->expanded)
        state_->apply(state_->tree, std::move(next));
}
std::vector<ItemId> TreeView::expandedItems() const
{
    std::vector<ItemId> result(state_->expanded.begin(), state_->expanded.end());
    std::sort(result.begin(), result.end(),
              [&](ItemId a, ItemId b) { return state_->tree->indices.at(a) < state_->tree->indices.at(b); });
    return result;
}
void TreeView::setSelection(std::vector<ItemId> ids)
{
    state_->list->setSelection(std::move(ids));
}
std::vector<ItemId> TreeView::selection() const
{
    return state_->list->selection();
}
std::optional<ItemId> TreeView::currentItem() const
{
    return state_->list->currentItem();
}
std::size_t TreeView::visibleItemCount() const
{
    return state_->flat->rowCount();
}
ItemId TreeView::visibleItem(std::size_t row) const
{
    return state_->flat->itemId(row);
}
ListRowRange TreeView::visibleRows() const
{
    return state_->list->visibleRows();
}
bool TreeView::scrollToItem(ItemId id)
{
    return state_->list->scrollToItem(id);
}
bool TreeView::setScrollOffset(float offset)
{
    return state_->list->setScrollOffset(offset);
}
float TreeView::scrollOffset() const
{
    return state_->list->scrollOffset();
}
} // namespace vectorgl::ui
