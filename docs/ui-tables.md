# Virtualized tables

Include `<vectorgl/ui/table_view.hpp>` and link `vectorgl::ui`. TableView composes
ListView with fixed headers, shared column geometry, horizontal scrolling, sorting
requests, and constrained resizing. Models, selection, and input work without a
window or GL context; painting needs an initialized Canvas in an active frame.

```cpp
using namespace vectorgl::ui;
auto model = std::make_shared<VectorTableModel>(std::vector<TableItem>{
    {101, {{1, "Temperature"}, {2, "42.5"}}},
    {205, {{1, "Pressure"}, {2, "3.2"}}}
});
std::vector<TableColumn> columns{
    {1, "Name", 240, 100, 600},
    {2, "Value", 120, 60, 240, true, true, vectorgl::TextAlign::Right}
};
TableViewOptions options;
options.selectionMode = ListSelectionMode::Multiple;
options.onSortRequested = [](TableSort request) { /* provide a sorted snapshot */ };
TableView table(ui, parent, model, columns, options);
ui.layout(table.id(), {0, 0, 640, 400});
table.setSelection({205});
```

## Model, columns, and identity

TableModel is an immutable UI-thread snapshot interface: row count, stable nonzero
unique item IDs, per-row enabled state, and cell text indexed by row and ColumnId.
Column IDs are stable nonzero identifiers independent of column order. Model
methods/painters must not mutate the UI/model, including through a mutable alias;
returned text views remain valid while the snapshot is retained.

VectorTableModel owns TableItem records with `{column, text}` cells. It rejects
zero/duplicate row IDs and zero/duplicate column IDs within a row. Missing columns
return empty text. The convenience model supports up to 1,000,000 rows and 4,096
cells per row. Custom models do not need to allocate string cells; visible values
can be adapted from existing immutable application data.

TableColumn supplies label, width/minimum/maximum, resizable/sortable input flags,
and left/center/right text alignment. Widths/limits must be finite, minima positive,
and the requested width within its limits. The view supports up to 4,096 columns
with a total width at most 16,777,216 logical pixels. Column IDs must be unique.
Row count/content-height limits follow ListView. Invalid model metadata or column
definitions leave the previous model/columns/selection intact.

`setColumns()` replaces/reorders columns; supplied widths are authoritative. It
retains the active header and sort indicator by column ID when still valid, or
resets them when removed/non-sortable. Cell text follows ColumnId rather than a
positional cell array. `setColumnWidth()` silently clamps finite requests to the
column limits/total extent; it may also update columns marked nonresizable, whose
flag restricts input only. Unknown IDs/nonfinite requests throw.

`setModel()` retains selected/current IDs still present and enabled, using the
same removal/fallback policy as ListView. Removed/filtered selections are discarded;
sorting does not replace selection with row indices. Null is empty. Programmatic
setters are silent; model/column/sort updates cancel pending header actions. Call `Context::layout()`
after model/column changes and before painting/input.

## Sorting, selection, and keyboard input

Header activation records a requested sort and emits `onSortRequested(TableSort)`.
A different column starts ascending; repeating it toggles ascending/descending.
The model is not sorted by the view. The application supplies a reordered immutable
snapshot using the same item IDs; it owns comparison rules, numeric/locale sorting,
and any asynchronous request ordering. `setSort()` silently sets/clears the
indicator and can restore it when an application rejects a request. Sort requests
and explicit indicators require an existing sortable column.

The header has one Tab stop; the body has another. On the focused header:

- Left/Right choose a column, Home/End choose first/last, and navigation reveals
  that header through horizontal scrolling.
- Enter/Space request sorting once on release. Repeats and releases after focus
  loss/cancellation do not repeat the action. Nonsortable headers do not activate.
- Shift+Left/Right resize the active resizable column by `keyboardResizeStep`,
  constrained to its limits, and notify only when its width actually changes.

The body inherits ListView's row selection, Ctrl/Command/Shift ranges, disabled-row
navigation, Home/End/Page Up/Page Down, and Enter activation. `onSelectionChanged`
receives a reference valid only for that callback; queries return copies in current
row order. Selection reveal/vertical scrolling preserve horizontal column offset.
This is a row selection table; individual editable/focusable spreadsheet cells are
not provided.

## Pointer resizing and cancellation

Primary press/release in the same header requests sorting. A press within
`resizeGrip` logical pixels of a resizable column's trailing edge starts a captured
drag. Width updates while dragging; release completes once and emits
`onColumnResized(id, width)` only for an actual change. Releasing outside the
original header still completes while capture is available.

Escape, window focus loss, pointer leave, subtree unavailability/removal, changed
header geometry, and programmatic model/column/sort updates cancel pending header
actions. Cancelled drags restore the original width and do not notify completion.
Keyboard resizing notifies per actual step. A resize never also requests sorting.

Callbacks can replace models/columns, remove the subtree, destroy the controller,
or throw under Context's normal callback/capture cleanup contract. Cancellation
restores old geometry before publishing replacement columns, including when
Context defers its Cancel notification until a callback returns. Context must
outlive TableView; destruction immediately removes its registered subtree.

## Geometry, painting, and work bounds

Headers are outside the vertical scroll area. Their column edges, body cell edges,
and horizontal scroll offset share one geometry cache. The header reserves the
body's border and vertical scrollbar lane rather than estimating their sizes.
`headerCellBounds(id)` returns the full, unclipped header-local rectangle; map it
through `headerId()`. Header painting/hits intersect it with the actual body viewport.
The horizontal scrollbar and two-axis wheel input are supplied by ScrollView.

The default cell painter clips single-line UTF-8 text and applies column alignment.
TableCellPainter can replace cell contents; it receives the item/column IDs,
indices, bounds, alignment, and row selection/current/enabled/focus flags. The view
draws row backgrounds/focus and column separators. Each custom cell call has scoped
Canvas state and enforced cell/row/viewport/ancestor clips. No UI/model mutation or
frame begin/end is allowed during painting.

The widget subtree has constant size. Snapshot indexing takes O(rows) work/storage;
column geometry takes O(columns). Rendering visits only viewport rows and intersecting
columns, with cell text requested only for those cells. Scrolling/column resizing
do not reindex model IDs. Programmatic column resizing rebuilds column geometry;
it does not inspect the table's row data. Preferred size describes the body; header
height is added by the containing column layout.

Variable-height rows, inline editing, column drag/reordering gestures, pinned
columns, multi-column sorting, and spreadsheet formulas are separate features.
Applications can reorder columns programmatically with `setColumns()`.

## Tag browser example

`example/ui_table_demo.cpp` shows a searchable 2,000-row tag browser with application
numeric/text sorting, right/center-aligned values, colored status cells, disabled
rows, stable multiple selection, resize/reset controls, and both scroll axes.

```powershell
cmake --preset windows-debug -DVECTORGL_BUILD_UI=ON -DVECTORGL_BUILD_GLFW_ADAPTER=ON -DVECTORGL_BUILD_UI_EXAMPLES=ON
cmake --build build/windows-debug --config Debug --target vectorgl_ui_table
.\build\windows-debug\example\Debug\vectorgl_ui_table.exe
```

The example accepts `--font PATH`, `--width`, `--height`, `--smoke-test`, and
`--screenshot FILE.bmp`. Search uses ASCII case-insensitive matching, while the
editor accepts UTF-8. Tests cover sorting/selection, constrained/cancelled resizing,
keyboard controls, model/column validation and replacement, deferred cancellation,
transforms/modal blocking, bounded work, and installed-package consumption. GPU
tests verify shared header/cell edges, scrolling/resizing, per-cell clipping,
picking, and visible-cell text queries at 100%, 125%, 150%, and 200% scale.
