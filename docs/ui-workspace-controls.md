# Workspace controls

Include `<vectorgl/ui/workspace_controls.hpp>` and link `vectorgl::ui`. PR 7 adds
TabBar/TabView, CollapsibleSection, Label, Separator, and SearchField. These compose
the existing layout, control painting, editing, and Context routing APIs.

## Tabs and pages

```cpp
vectorgl::ui::TabBarOptions options;
options.tabs = {{"Properties"}, {"Metadata"}, {"Unavailable", false}};
options.selected = 0;
options.onChanged = [](std::size_t index) { /* update application selection */ };
vectorgl::ui::TabView inspector(ui, parent, options);
// Register controls/layouts below inspector.page(0) and inspector.page(1).
ui.layout(inspector.id(), {0, 0, 320, 600});
```

TabBar owns only its row/buttons. TabView composes a TabBar and owned page
containers, keeping only the selected page visible. Context must outlive these
controllers. As with other controls, destruction removes the registered subtree
and suppresses callback errors; explicit `Context::remove(id())` permits handling
cleanup errors. Destroy application callback targets after their UI subtrees.

Left/Right wrap through enabled tabs; Home/End select the first/last enabled tab.
Keyboard navigation selects immediately. Pointer, Enter, and Space activation use
the shared Button capture/release/cancel behavior. Disabled tabs remain visible
but cannot activate. There is one sequential Tab stop in the bar; Tab then moves
to page content or the next application control. Explicit/pointer focus on other
enabled tabs remains possible. Tabs share available width; long labels clip.

Use `setSelected` and `setTabEnabled` for programmatic changes; these are silent.
The constructor defaults to the first enabled tab when no selection is supplied.
`setSelected(nullopt)` explicitly clears selection; TabView then shows no page.
Disabling the selected tab chooses the next enabled tab, wrapping if needed; an
all-disabled bar has no selection. Invalid indices/disabled selections throw.
`onChanged` reports actual user selection changes, not reactivation of the current
tab. A callback may remove its control or select another page.

Switching pages moves focus from disappearing content to the selected tab and lets
Context cancel unavailable capture/timers. Hidden pages leave measurement, painting,
picking, and focus traversal. Existing TextField blur policy applies when its page
is hidden. State tracks actual page visibility so callback-driven redirection during
focus/capture cleanup cannot leave stale pages visible. Context may deliver those
cleanup notifications after an input callback unwinds.

`tabId(index)` supports focus/tooltip integration; `page(index)` supports application
children/layouts. Let the controller own tab enabled state and page visibility;
do not independently mutate those private parts. Hiding/disabling the whole control
through Context is supported. Horizontal tab bars, not docking or draggable/reordered
tabs, are provided in this slice.

## Collapsible sections

```cpp
vectorgl::ui::CollapsibleSection section(ui, parent, {"Appearance"});
// Register content beneath section.content(); it has a default column layout.
section.setExpanded(false); // Silent programmatic collapse.
```

The header activates with pointer/Enter/Space. Left collapses and Right expands
while the header is focused. Expansion changes from user input call `onChanged`;
repeating an already-applied state does not repeat it. `setExpanded` is silent.
Focus within disappearing content returns to `headerId()` before hiding the body.
Context then cancels capture and excludes the hidden subtree from Tab, picking,
painting, and intrinsic layout height. Expanding preserves control values and keeps
focus on the header. Call `Context::layout` after changes as for other containers.

## Labels and separators

Label uses the theme font/text color with optional font scale, padding, and
left/center/right alignment. Choose Clip or Ellipsis for a single line, or Wrap
for width-dependent multiline measurement. Single-line modes repair UTF-8 and
filter line breaks/control characters; wrapping retains line breaks. Ellipsis uses
U+2026 and clips to the content rectangle. If even the ellipsis does not fit, it
draws no text. Truncation respects UTF-8 codepoint boundaries, matching the current
engine's codepoint rather than grapheme/shaping contract.

```cpp
vectorgl::ui::LabelOptions label;
label.text = "A source name longer than the inspector";
label.overflow = vectorgl::ui::TextOverflow::Ellipsis;
vectorgl::ui::Label source(ui, canvas, parent, label);
```

The supplied Canvas is used for intrinsic font measurement; keep it and Context
alive. With a configured font, measurement/painting need an initialized Canvas and
current GL context. Without a configured/available font, measurement uses explicit
`fallbackSize` for headless layouts. Painting selects the theme font on its target
Canvas. Setters invalidate measurement; cached ellipsis text is keyed by text,
width, font path, and size. Clip/wrap painters use the same geometry as layout.

`ellipsizeText(text, width, measure)` exposes the font-independent fitting helper.
The callback must return finite nonnegative widths and monotonic prefix widths.
It measures a bounded logarithmic number of candidate prefixes after decoding the
UTF-8 boundaries. The full original value remains available from `Label::text()`.

Separator draws a horizontal/vertical themed line with thickness and end inset.
Its intrinsic length is configurable; normal layout stretch fills the available
cross dimension. Labels and separators are nonfocusable and pointer-transparent;
they do not intercept input intended for an enclosing control.

## Live search

SearchField composes TextField with an always-reserved clear button. It emits
`onChanged` for actual query changes, including restoration/cancellation. The
callback's string_view is valid only during that call; copy it to retain it.
`setQuery` silently replaces buffer/committed query. `clear()` emits once when
nonempty and returns focus to `fieldId()`; the clear button is disabled for an empty
query. Its fixed reserved space avoids shifting the editor as text changes.

Unicode codepoint editing, selection, clipboard results, Enter commit, and Escape
restore follow TextField's existing contracts. Escape restores the last committed
query; explicit clear is available via the button, `clear()`, or selecting/deleting
the query. `onServiceError` reports injected clipboard failures. The field emits a
query, not a collection/filtering policy; the application decides matching and
which widgets to hide. Filtering hidden results removes them from Context traversal.

Context adds `Widget::tabStop` and `setTabStop` for composite controls. These affect
sequential Tab traversal and fallback focus only; `focusable` still governs explicit
and pointer focus. Defaults preserve existing behavior.

## Workspace demo

`example/ui_workspace_demo.cpp` adds a searchable toolbox with result counts,
no-results feedback, collapsible groups, selectable symbol buttons, and inspector
tabs/sections. Its Metadata page demonstrates ellipsis, wrapping, and a separator.
The demo filters its English catalog using ASCII case-insensitive substring
matching, while the search editor itself supports UTF-8. Native clipboard service
is wired at the GLFW host boundary; automated smoke tests do not read/write it.

```powershell
cmake --preset windows-debug -DVECTORGL_BUILD_UI=ON -DVECTORGL_BUILD_GLFW_ADAPTER=ON -DVECTORGL_BUILD_UI_EXAMPLES=ON
cmake --build build/windows-debug --config Debug --target vectorgl_ui_workspace
.\build\windows-debug\example\Debug\vectorgl_ui_workspace.exe
```

`--metadata` starts on the Metadata page; existing font/size/smoke/screenshot flags
remain available. Headless tests cover UTF-8 fitting, roving navigation, disabled
tabs, callback-driven page changes, hidden/collapsed focus/capture/timers, and search
notification/clear behavior. GPU tests compare ellipsized label output against
reference Canvas rendering and separator pixels at 100%, 125%, 150%, and 200% scale.
Installed consumers exercise all new controls through public headers.
