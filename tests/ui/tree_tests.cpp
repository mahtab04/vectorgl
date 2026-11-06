#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vectorgl/ui/tree_view.hpp>

using namespace vectorgl;
using namespace vectorgl::ui;
namespace
{
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <class Exception = std::invalid_argument, class F> void rejects(F&& f)
{
    try
    {
        f();
    }
    catch (const Exception&)
    {
        return;
    }
    throw std::runtime_error("Invalid tree data was accepted");
}
Event key(Key value, Modifiers modifiers = {}, EventType type = EventType::KeyDown)
{
    Event e;
    e.type = type;
    e.key = value;
    e.modifiers = modifiers;
    return e;
}
std::shared_ptr<const TreeModel> sample()
{
    return std::make_shared<VectorTreeModel>(std::vector<TreeItem>{{10, 0, "Project"},
                                                                   {20, 10, "Screens"},
                                                                   {21, 20, "Main"},
                                                                   {22, 20, "Disabled", false},
                                                                   {30, 10, "Resources"},
                                                                   {31, 30, "Symbol"},
                                                                   {40, 0, "Other"}});
}
std::vector<ItemId> visible(const TreeView& tree)
{
    std::vector<ItemId> ids;
    for (std::size_t i = 0; i < tree.visibleItemCount(); ++i)
        ids.push_back(tree.visibleItem(i));
    return ids;
}
Event point(Context& ui, TreeView& tree, std::size_t row, float x = 12)
{
    const auto b = ui.bounds(tree.viewportId()).value();
    const auto p = ui.mapToRoot(tree.viewportId(), {b.x + x, b.y + float(row) * 30 + 15 - tree.scrollOffset()}).value();
    Event e;
    e.type = EventType::PointerDown;
    e.x = p.x;
    e.y = p.y;
    e.button = PointerButton::Primary;
    return e;
}
void click(Context& ui, TreeView& tree, std::size_t row, float x = 12, bool outside = false)
{
    auto e = point(ui, tree, row, x);
    require(ui.dispatch(e), "Tree press escaped");
    e.type = EventType::PointerUp;
    if (outside)
        e.x = -100;
    require(ui.dispatch(e), "Tree release escaped");
}
TreeViewOptions options()
{
    TreeViewOptions o;
    o.rowHeight = 30;
    o.selectionMode = ListSelectionMode::Multiple;
    return o;
}
void hierarchyAndNavigation()
{
    Context ui;
    int selections = 0, expansions = 0, activations = 0;
    auto o = options();
    o.onSelectionChanged = [&](const auto&) { ++selections; };
    o.onExpansionChanged = [&](ItemId, bool) { ++expansions; };
    o.onActivated = [&](ItemId) { ++activations; };
    TreeView tree(ui, {}, sample(), o);
    ui.layout(tree.id(), {20, 30, 250, 160});
    require(visible(tree) == std::vector<ItemId>({10, 40}), "Tree did not start collapsed");
    ui.focus(tree.id());
    ui.dispatch(key(Key::Right));
    require(visible(tree) == std::vector<ItemId>({10, 20, 30, 40}) && tree.currentItem() == 10 && expansions == 1,
            "Right did not expand");
    ui.layout(tree.id(), {20, 30, 250, 160});
    ui.dispatch(key(Key::Right));
    require(tree.currentItem() == 20 && tree.selection() == std::vector<ItemId>{20}, "Right did not enter first child");
    ui.dispatch(key(Key::Right));
    ui.layout(tree.id(), {20, 30, 250, 160});
    ui.dispatch(key(Key::Right));
    require(tree.currentItem() == 21, "Right did not enter nested child");
    ui.dispatch(key(Key::Down));
    require(tree.currentItem() == 30, "Down did not skip disabled visible row");
    ui.dispatch(key(Key::Left));
    require(tree.currentItem() == 10, "Left did not find parent");
    ui.dispatch(key(Key::Left));
    require(!tree.isExpanded(10) && tree.isExpanded(20) && tree.currentItem() == 10,
            "Left did not collapse/retain descendant expansion");
    require(expansions == 3, "Expansion callback count incorrect");
    ui.dispatch(key(Key::Enter));
    ui.dispatch(key(Key::Enter, {}, EventType::KeyRepeat));
    ui.dispatch(key(Key::Enter, {}, EventType::KeyUp));
    require(activations == 1, "Tree activation repeated");
    const int before = selections;
    tree.setExpanded(10, true);
    tree.setSelection({21, 30});
    tree.setExpanded(10, false);
    require(tree.selection() == std::vector<ItemId>({21, 30}) && tree.currentItem() == 10 && selections == before,
            "Collapse did not retain hidden selection/separate current");
    require(!tree.setExpanded(40, true), "Leaf expansion changed state");
    rejects([&] { tree.setExpanded(999, true); });
    tree.setSelection({21});
    require(tree.selection() == std::vector<ItemId>{21}, "Hidden enabled selection was rejected");
    rejects([&] { tree.setExpandedItems({40}); });
}
void pointerAndCancellation()
{
    Context ui;
    auto o = options();
    int changes = 0, expanded = 0;
    o.onSelectionChanged = [&](const auto&) { ++changes; };
    o.onExpansionChanged = [&](ItemId, bool) { ++expanded; };
    TreeView tree(ui, {}, sample(), o);
    ui.layout(tree.id(), {0, 0, 240, 160});
    click(ui, tree, 0);
    require(tree.isExpanded(10) && tree.selection().empty() && ui.focused() == tree.id(),
            "Disclosure selected a row or failed to expand");
    ui.layout(tree.id(), {0, 0, 240, 160});
    click(ui, tree, 1, 32);
    ui.layout(tree.id(), {0, 0, 240, 160});
    click(ui, tree, 2, 80);
    require(tree.selection() == std::vector<ItemId>{21}, "Text click did not select descendant");
    click(ui, tree, 0);
    require(tree.selection() == std::vector<ItemId>{21} && tree.currentItem() == 10 && changes == 1 && expanded == 3,
            "Collapse discarded hidden selection or incorrectly notified");
    ui.layout(tree.id(), {0, 0, 240, 160});
    click(ui, tree, 0, 12, true);
    require(!tree.isExpanded(10) && !ui.captured(), "Outside disclosure release toggled branch");
    auto e = point(ui, tree, 0);
    ui.dispatch(e);
    ui.dispatch(Event{EventType::WindowFocusLost});
    e.type = EventType::PointerUp;
    ui.dispatch(e);
    require(!tree.isExpanded(10) && !ui.captured(), "Disclosure survived focus loss");
    ui.dispatch(point(ui, tree, 0));
    tree.setModel(sample());
    e.type = EventType::PointerUp;
    ui.dispatch(e);
    require(!tree.isExpanded(10), "Stale release toggled replacement model");
    ui.setTransform(tree.id(), Mat3x3::translation(50, 20) * Mat3x3::scaling(1.5f, 1.5f));
    click(ui, tree, 0);
    require(tree.isExpanded(10), "Transformed disclosure picking failed");
    ui.setEnabled(tree.id(), false);
    require(!ui.focus(tree.id()), "Disabled tree accepted focus");
}
class InvalidModel final : public TreeModel
{
public:
    std::size_t rootCount() const override
    {
        return 1;
    }
    ItemId rootId(std::size_t) const override
    {
        return 1;
    }
    std::size_t childCount(ItemId) const override
    {
        return 1;
    }
    ItemId childId(ItemId, std::size_t) const override
    {
        return 1;
    }
    std::string_view text(ItemId) const override
    {
        return "Cycle";
    }
};
class SharedChildModel final : public TreeModel
{
public:
    std::size_t rootCount() const override
    {
        return 2;
    }
    ItemId rootId(std::size_t row) const override
    {
        return row + 1;
    }
    std::size_t childCount(ItemId id) const override
    {
        return id < 3 ? 1 : 0;
    }
    ItemId childId(ItemId, std::size_t) const override
    {
        return 3;
    }
    std::string_view text(ItemId) const override
    {
        return "Shared";
    }
};
void replacementAndValidation()
{
    Context ui;
    TreeView tree(ui, {}, sample(), options());
    tree.setExpandedItems({10, 20, 30, 10});
    ui.layout(tree.id(), {0, 0, 240, 160});
    tree.setSelection({21, 31});
    tree.setModel(std::make_shared<VectorTreeModel>(std::vector<TreeItem>{{40, 0, "Other"},
                                                                          {10, 0, "Project"},
                                                                          {30, 10, "Resources"},
                                                                          {31, 30, "Symbol"},
                                                                          {20, 10, "Screens"},
                                                                          {21, 20, "Main"}}));
    require(tree.selection() == std::vector<ItemId>({31, 21}) && tree.currentItem() == 21 &&
                tree.expandedItems() == std::vector<ItemId>({10, 30, 20}),
            "Reorder lost stable selection/expansion");
    const auto old = tree.model();
    rejects([&] { tree.setModel(std::make_shared<InvalidModel>()); });
    rejects([&] { tree.setModel(std::make_shared<SharedChildModel>()); });
    require(tree.model() == old && tree.isExpanded(10) && tree.selection().size() == 2,
            "Invalid hierarchy replaced good state");
    rejects([] { VectorTreeModel bad({{1, 999, "Orphan"}}); });
    rejects([] { VectorTreeModel bad({{1, 2, "A"}, {2, 1, "B"}}); });
    rejects([] { VectorTreeModel bad({{1, 1, "Self"}}); });
    rejects([] { VectorTreeModel bad({{0, 0, "Zero"}}); });
    rejects([] { VectorTreeModel bad({{1, 0, "A"}, {1, 0, "B"}}); });
    std::vector<TreeItem> deep;
    for (ItemId id = 1; id <= 514; ++id)
        deep.push_back({id, id - 1, "Deep"});
    rejects<std::length_error>([&] { tree.setModel(std::make_shared<VectorTreeModel>(std::move(deep))); });
    require(tree.model() == old, "Depth failure replaced tree snapshot");
    tree.setModel(std::make_shared<VectorTreeModel>(
        std::vector<TreeItem>{{10, 0, "Project"}, {20, 10, "Screens"}, {21, 20, "Main", false}}));
    require(tree.selection().empty() && !tree.isExpanded(30) && tree.currentItem() == 20,
            "Disabled current or removed expansion survived reset");
    tree.setModel({});
    ui.layout(tree.id(), {0, 0, 240, 160});
    require(tree.visibleItemCount() == 0 && tree.selection().empty() && !tree.currentItem(),
            "Null model retained state");
    auto bad = options();
    bad.indentation = 0;
    rejects([&] { TreeView invalid(ui, {}, sample(), bad); });
}
class LargeModel final : public TreeModel
{
public:
    mutable std::size_t metadata = 0, texts = 0;
    std::size_t rootCount() const override
    {
        ++metadata;
        return 1;
    }
    ItemId rootId(std::size_t) const override
    {
        ++metadata;
        return 1;
    }
    std::size_t childCount(ItemId id) const override
    {
        ++metadata;
        return id == 1 ? 2000 : 0;
    }
    ItemId childId(ItemId, std::size_t row) const override
    {
        ++metadata;
        return row + 2;
    }
    std::string_view text(ItemId) const override
    {
        ++texts;
        return "Node";
    }
};
void workAndCallbacks()
{
    Context ui;
    auto model = std::make_shared<LargeModel>();
    TreeView tree(ui, {}, model, options());
    const auto indexed = model->metadata;
    tree.setExpanded(1, true);
    ui.layout(tree.id(), {0, 0, 240, 300});
    require(tree.visibleItemCount() == 2001 && ui.layoutStats().arrangedWidgets < 12,
            "Tree allocated per-node widgets");
    tree.setScrollOffset(45000);
    require(tree.visibleRows() == ListRowRange{1500, 1510}, "Far tree range incorrect");
    ui.focus(tree.id());
    ui.dispatch(key(Key::End));
    ui.dispatch(key(Key::Left));
    require(model->metadata == indexed && model->texts == 0, "Steady navigation rescanned model");
    std::unique_ptr<TreeView> control;
    int expansionCallbacks = 0;
    auto o = options();
    o.onSelectionChanged = [&](const auto&) { control.reset(); };
    o.onExpansionChanged = [&](ItemId, bool) { ++expansionCallbacks; };
    control = std::make_unique<TreeView>(ui, WidgetId{}, sample(), o);
    control->setExpandedItems({10, 20});
    control->setSelection({21});
    ui.layout(control->id(), {0, 0, 240, 200});
    click(ui, *control, 0, 80);
    require(!control && !ui.captured() && expansionCallbacks == 0,
            "Destruction callback left capture or stale expansion notification");
    o.onSelectionChanged = {};
    o.onExpansionChanged = [&](ItemId, bool) { control->setModel({}); };
    control = std::make_unique<TreeView>(ui, WidgetId{}, sample(), o);
    ui.layout(control->id(), {0, 0, 240, 200});
    ui.focus(control->id());
    ui.dispatch(key(Key::Right));
    require(!control->model() && control->visibleItemCount() == 0, "Expansion callback model reset failed");
}
void disabledHierarchyAndModal()
{
    Context ui;
    auto model = std::make_shared<VectorTreeModel>(
        std::vector<TreeItem>{{1, 0, "Root"}, {2, 1, "Disabled", false}, {3, 2, "Child"}, {4, 1, "Sibling"}});
    TreeView tree(ui, {}, model, options());
    tree.setExpandedItems({1, 2});
    ui.layout(tree.id(), {0, 0, 240, 200});
    tree.setSelection({3});
    ui.focus(tree.id());
    ui.dispatch(key(Key::Left));
    require(tree.currentItem() == 1, "Left focused disabled ancestor");
    ui.dispatch(key(Key::Right));
    require(tree.currentItem() == 4, "Right focused disabled direct child");
    Widget modal;
    modal.bounds = {300, 0, 100, 100};
    modal.focusable = true;
    const auto id = ui.add(modal);
    ui.pushModal(id);
    click(ui, tree, 0);
    require(tree.isExpanded(1) && tree.currentItem() == 4, "Modal allowed background disclosure input");
    ui.popModal();
    ui.remove(id);
    ui.setVisible(tree.id(), false);
    require(!ui.focus(tree.id()), "Hidden tree accepted focus");
}
} // namespace
int main()
{
    try
    {
        hierarchyAndNavigation();
        pointerAndCancellation();
        replacementAndValidation();
        workAndCallbacks();
        disabledHierarchyAndModal();
        std::cout << "Tree tests passed\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
