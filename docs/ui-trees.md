# Virtualized trees

Include `<vectorgl/ui/tree_view.hpp>` and link `vectorgl::ui`. TreeView composes the
virtualized ListView with a hierarchy index, expansion state, disclosure input, and
tree navigation. Model/selection/input tests run without a window or OpenGL;
painting uses an initialized Canvas in an active frame.

```cpp
using namespace vectorgl::ui;
auto model = std::make_shared<VectorTreeModel>(std::vector<TreeItem>{
    {10, 0, "Project"},
    {20, 10, "Screens"},
    {21, 20, "Main screen"},
    {30, 10, "Resources"}
});
TreeViewOptions options;
options.selectionMode = ListSelectionMode::Multiple;
options.onActivated = [](ItemId id) { /* open item */ };
TreeView tree(ui, parent, model, options);
tree.setExpandedItems({10, 20});
ui.layout(tree.id(), {0, 0, 320, 480});
tree.setSelection({21});
```

## Models and validation

TreeModel is an immutable UI-thread forest adapter: root IDs, ordered child IDs,
text, and per-node enabled state. IDs must be nonzero, globally unique within the
reachable forest, and stable when items are reordered or a snapshot is replaced.
Returned text views must remain valid while the snapshot is retained. Methods and
painters must not mutate the UI/model, including through another mutable alias.

VectorTreeModel owns a flat vector of TreeItem records. Parent ID zero denotes a
root; sibling order follows input order. Parents may appear before or after their
children. Construction rejects duplicate/zero IDs, missing parents, and parent
cycles, including components unreachable from any root.

TreeView indexes all reachable metadata once, including collapsed descendants,
using iterative traversal. Custom models with repeated/shared child IDs, cycles,
or zero IDs are rejected. Only the forest reachable from the supplied roots is
part of a custom model's contract. The index supports up to 1,000,000 nodes and
depth 512 (root depth zero). The visible projection also obeys ListView's
16,777,216-logical-pixel content-height limit. Invalid snapshots or expansions
that exceed these limits leave the previous model/expansion/selection intact.

Text is requested only for painted rows; expanding, scrolling, and keyboard
movement do not reread model metadata. Disabled state is per node: a disabled
branch cannot be expanded by input, but programmatic expansion is allowed and
enabled descendants remain navigable.

## Expansion, selection, and current row

The tree starts collapsed with no selection. The first enabled root is current.
`setExpanded(id, bool)` changes a known branch; a known leaf is a no-op, and an
unknown ID throws. `setExpandedItems(ids)` replaces the complete expansion set,
accepts hidden branches, collapses duplicate IDs, and rejects leaves/unknown IDs.
Collapsing an ancestor retains descendant expansion flags for a later reopening.

Selection contains existing enabled nodes, including collapsed descendants.
`setSelection()` follows ListView's single/multiple/duplicate rules. Collapsing a
branch retains hidden selection until the model removes/disables those nodes.
If current becomes hidden/disabled,
the view prefers its closest surviving visible enabled ancestor. Otherwise it
uses ListView's next/previous enabled-row fallback. Selection can remain empty
independently of current. A projection change resets the range anchor to current.

`setModel()` preserves expansion for surviving branch IDs and selection for
surviving enabled IDs, even after sibling reordering. Null is empty.
Expansion and selection queries return copies; `visibleItemCount()` includes all
expanded hierarchy rows, while `visibleRows()` returns only the viewport range.
`visibleItem(row)` maps a projection row to its stable ID and throws out of range.

Call `Context::layout()` before painting/input after projection changes. Scroll
offsets clamp through the normal layout boundary. `scrollToItem()` reveals a
visible item; it does not expand hidden ancestors. Programmatic setters are silent.

## Input and callbacks

The tree has one Tab stop and inherits ListView's Up/Down, Home/End, Page Up/Down,
single/multiple selection, Ctrl/Command+A, Space, Enter activation, and focus reveal.
Ancestor transforms/clipping, disabled/hidden widgets, and modal scopes continue to
use Context. In addition:

- Right expands a collapsed branch, or moves to its first enabled direct child
  when expanded. Left collapses an expanded branch, or moves to its nearest enabled
  ancestor. Ctrl/Command moves current without changing selection; Shift extends
  selection across visible rows when navigation moves to another node.
- Primary press/release in the same enabled disclosure region toggles expansion
  without selecting the row. The press makes that branch current. A text/row click
  uses normal list selection. Disclosure bounds span the row height and use the
  configured indentation and disclosure width.
- Outside release, Escape, window focus loss, pointer leave, subtree removal, and
  model/expansion changes cancel pending disclosure presses. Stale releases do not
  toggle replacement nodes. Enter activates current once on release; it does not
  implicitly expand branches.

Input expansion emits `onExpansionChanged(id, expanded)` once for an actual change.
Collapse does not emit a selection change merely because selected nodes become
hidden. Selection callback vector references are valid only for that call. Callbacks may replace the model,
remove the subtree, destroy the controller, or throw under Context's normal cleanup
rules. If the selection callback changes the projection/removes the control, the
obsolete expansion notification is suppressed. Context must outlive TreeView;
destruction immediately removes its owned subtree.

## Rendering and work bounds

The default painter draws themed selection/focus, triangular disclosures, and
single-line UTF-8 text. TreeRowPainter can replace the complete row appearance;
TreeRow supplies depth, branch/expansion state, ListRow flags/bounds, and the exact
disclosure/text bounds. Keep disclosure decoration aligned with those bounds;
TreeView retains its hit testing. Each call has scoped Canvas state and enforced
row/ancestor clips. Painting visits viewport rows only.

Snapshot indexing uses O(N) work/storage. Expansion changes rebuild the expanded
preorder projection and its selection-universe index in O(N) work, using cached
metadata. Ordinary scrolling, input targeting, and painting do not traverse the
hierarchy. The widget subtree has constant size. Rows have fixed height and the
view scrolls vertically; indentation/text clip when the viewport is too narrow.
Variable heights, lazy/asynchronous child loading, drag/drop/reparenting, inline
editing, horizontal scrolling, and table columns remain separate features.

## Project hierarchy example

`example/ui_tree_demo.cpp` shows 2,024 nodes with expandable screens/resources,
multiple selection, disabled leaves, Unicode labels, expand/collapse-all, sibling
reordering with stable IDs, and current/selection/viewport counts.

```powershell
cmake --preset windows-debug -DVECTORGL_BUILD_UI=ON -DVECTORGL_BUILD_GLFW_ADAPTER=ON -DVECTORGL_BUILD_UI_EXAMPLES=ON
cmake --build build/windows-debug --config Debug --target vectorgl_ui_tree
.\build\windows-debug\example\Debug\vectorgl_ui_tree.exe
```

The example accepts `--font PATH`, `--width`, `--height`, `--smoke-test`, and
`--screenshot FILE.bmp`. Tests cover validation/limits, hierarchy navigation,
selection/expansion reconciliation, cancellation, callback destruction/replacement,
bounded layout/model work, visible-row painting and clipping at 100%, 125%, 150%,
and 200% scale, and consumption through installed public headers.
