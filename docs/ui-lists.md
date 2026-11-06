# Virtualized lists

Include `<vectorgl/ui/list_view.hpp>` and link `vectorgl::ui`. ListView composes
Context routing, ScrollView, and a row painter without allocating a widget per
item. No window or GL context is required for model/selection/input operations;
painting requires an initialized Canvas in an active frame.

```cpp
using namespace vectorgl::ui;
auto model = std::make_shared<VectorListModel>(std::vector<ListItem>{
    {101, "Main screen"}, {205, "Temperature sensor"}, {309, "Unavailable", false}
});
ListViewOptions options;
options.selectionMode = ListSelectionMode::Multiple;
options.onSelectionChanged = [](const std::vector<ItemId>& ids) { /* copy if retained */ };
options.onActivated = [](ItemId id) { /* open the current item */ };
ListView list(ui, parent, model, options);
ui.layout(list.id(), {0, 0, 320, 480});
list.setSelection({205}); // Silent, stable item identity.
```

## Models and identity

ListModel is an immutable UI-thread snapshot interface: row count, nonzero unique
ItemId, text, and enabled state. VectorListModel owns a vector for simple clients;
applications can implement ListModel to adapt their own storage. Returned text
views must remain valid while the snapshot is retained. Model methods and painters
must not mutate the UI or snapshot, including through another mutable alias.

Keep the same ID when sorting/filtering a surviving item, and allocate a different
ID for a replacement entity. Do not use the displayed row index as identity.
`setModel()` validates IDs/enabled metadata and constructs an index before publishing
the new snapshot. Invalid metadata leaves the old snapshot and selection intact.
Null denotes an empty model. Each snapshot is limited to 1,000,000 rows and
16,777,216 logical pixels of content height. Row height is fixed, finite, positive,
and supplied when constructing the view.

Replacing a model preserves selected IDs still present and enabled. Missing or
disabled selections are removed; hidden selections are not retained for later
filter restoration. The current row and range anchor retain their IDs when possible.
When current disappears, it moves to the first enabled row at/after its former
index, or the last enabled row before that index. An all-disabled/empty list has
no current item. Selection may be empty independently of current. The initial
current item is the first enabled row; there is no initial selection.

Model replacement cancels a pending row press/keyboard activation. It leaves the
scroll offset to ScrollView's normal clamping on the next layout; call
`scrollToItem()` after layout to reveal a retained ID explicitly. Call
`Context::layout()` before painting/input after replacing the model.

`setSelection()` accepts existing enabled IDs, collapses duplicates, and sets
current/anchor to the first supplied ID. Single mode rejects more than one distinct
ID. Empty input clears selection while retaining current/anchor. Invalid selection
leaves the previous state intact. Selection queries return copies in current row
order, safe to retain through model replacement.

## Input and notifications

There is one sequential Tab stop. Ancestor clipping, transforms, enabled/visible
state, capture cancellation, and modal barriers use Context's existing contracts.
The scrollbars remain interactive without adding another Tab stop.

- Primary press/release on the same enabled row selects it; release outside the
  viewport, cancellation, and stale releases after model replacement do not select.
- Up/Down skip disabled rows. Home/End select the first/last enabled item.
  Page Up/Down move approximately one viewport in row coordinates, skipping disabled
  regions in the movement direction. Navigation reveals the current row.
- In Multiple mode, Ctrl/Command-click toggles membership, Shift extends from the
  range anchor, and Ctrl/Command+Shift adds a range. Ctrl/Command+A selects all
  enabled rows. Ctrl/Command+arrows move current without changing selection.
- Space applies selection at current, using the same modifiers. Enter activates
  current on release, once; key repeats and a release after focus loss do not activate.
  Pointer double-click activation is not provided by this slice.

Programmatic setters/model replacement are silent. `onSelectionChanged` reports
actual membership changes caused by user input, including changes to empty selection.
The vector reference is valid only during the call. A callback may replace the model,
remove the subtree, or destroy the controller; event handling does not subsequently
access a stale row. Callback exceptions follow Context's propagation/capture cleanup
rules. Context must outlive the controller; its destructor removes the owned subtree.

## Painting and work bounds

The default painter uses Theme text/surface/selection/focus colors and a clipped,
single-line UTF-8 label. A custom ListRowPainter receives the snapshot, row ID/index,
logical row bounds, and enabled/selected/current/focus flags. Each invocation has
scoped Canvas state and an enforced row clip in addition to the viewport/ancestor
clips. It must not change UI/model state or begin/end a frame.

`visibleRows()` and the headless `visibleListRows()` helper return a half-open range
including partial rows. Zero-height viewports yield an empty range. Only that range
is painted and only painted rows need text. Snapshot indexing costs O(N) time/memory;
selection storage costs O(selected items). Ordinary scrolling/layout/hit testing do
not enumerate the model. Keyboard movement uses a cached enabled-row index; range
selection and select-all naturally touch the selected range/all enabled items.
Tree hierarchy, variable row heights, inline editors, drag/drop, table headers, and
sorting policy are outside this slice; filtering/sorting remain application policy.

Tree hierarchy is provided by the separate [TreeView](ui-trees.md), which reuses
this list's selection, scrolling, and visible-row painting. Composite controls can
use `ListViewOptions::eventFilter` to handle input before normal list behavior:
return nullopt to continue, or EventReply to replace handling. Notifications must
return nullopt; cancellation/focus cleanup always runs regardless of the reply.
Filters must not mutate state during paint/layout. `setCurrentItem()` silently
sets current/range anchor without changing selection; `navigateToItem()` applies
user-navigation modifier semantics and selection notifications. It returns false
for unknown/disabled items, unavailable controls, or controls behind a modal scope.

## Catalog example

`example/ui_list_demo.cpp` shows a searchable 2,000-item catalog, multiple selection,
disabled rows, reordering/removal with stable IDs, custom row painting, and visible
row counts. Search uses ASCII case-insensitive matching; the editor supports UTF-8.

```powershell
cmake --preset windows-debug -DVECTORGL_BUILD_UI=ON -DVECTORGL_BUILD_GLFW_ADAPTER=ON -DVECTORGL_BUILD_UI_EXAMPLES=ON
cmake --build build/windows-debug --config Debug --target vectorgl_ui_list
.\build\windows-debug\example\Debug\vectorgl_ui_list.exe
```

The example accepts `--font PATH`, `--width`, `--height`, `--smoke-test`, and
`--screenshot FILE.bmp`. Its smoke test counts visible-row paint calls and checks
stable selection/reordering and keyboard reveal. Headless tests cover selection,
replacement, cancellation, callback destruction, transforms/modals, and bounded
model/layout work. GPU tests cover row/viewport clipping and picking agreement at
100%, 125%, 150%, and 200% logical-to-framebuffer scale. Installed-package consumers
exercise the public model/view API without example-private includes.
