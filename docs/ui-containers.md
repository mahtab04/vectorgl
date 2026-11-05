# Split panels and nested scrolling

Include `<vectorgl/ui/containers.hpp>` and link `vectorgl::ui`. The controllers
register ordinary widgets with Context; measurement, input, and focus work without
a window or OpenGL context. Painting uses the existing Canvas bridge.

```cpp
vectorgl::ui::Context ui;
vectorgl::ui::SplitOptions options;
options.fraction = 0.3f;
options.firstMinimum = 160;
options.secondMinimum = 240;
vectorgl::ui::SplitPanel split(ui, {}, options);
ui.setLayout(split.first(), std::make_shared<vectorgl::ui::ColumnLayout>());
vectorgl::ui::ScrollView list(ui, split.first());
vectorgl::ui::LayoutHints hints;
hints.flex = 1;
ui.setLayoutHints(list.id(), hints);
list.setContentLayout(std::make_shared<vectorgl::ui::ColumnLayout>());
// Register application rows beneath list.content().
ui.layout(split.id(), {0, 0, 1000, 700});
```

## Ownership and layout

Context must outlive the controllers. Each controller removes its entire registered
subtree on destruction, including application children. Destructors suppress client
callback exceptions; explicitly call `Context::remove(id())` if the application
needs to handle cancellation/focus callback errors. Removing a parent first is safe.
Keep the controllers' internal widgets visible, registered, and in their original
order. Add/remove/hide application content below the exposed pane/content IDs.

Give split panes their own application layouts. Nested splits and scroll views are
ordinary layout children; set `LayoutHints::flex = 1` to fill remaining space.
For manual placement, use zero-origin bounds plus the widget transform to position
a container and its descendants, as with other manually positioned UI subtrees.

`SplitAxis::Horizontal` places panes side by side; Vertical stacks them. The
requested `fraction()` is clamped by options and pane size hints during arrangement;
`firstExtent()` reports the actual logical size. Minimums take priority when the
window is too small, with overflow clipped by the split root. Maximums can leave
unused space when both panes are capped. Conflicting option/hint limits throw.
Cross-axis pane hints also apply. Fractions survive resize, subject to these limits.

Primary-pointer dragging captures the divider and completes once on release,
including release outside. Escape, focus/window leave, unavailable capture, removal,
or resizing during a drag cancel it and restore its starting fraction. Optional
`setOnDragFinished` reports completion or cancellation. Focus a divider with Tab;
axis arrows move it by `keyboardStep`, and Home/End request either extreme.
Changes invalidate arrangement; call `Context::layout` before subsequent painting
or geometry-dependent input, as for other layout changes.

## Scroll geometry and routing

`ScrollModel` contains pure bounded geometry: viewport/content dimensions, clamped
offsets, reveal, and minimum-size scrollbar thumbs. `ScrollView` adds clipped input,
painting, scrollbars, keyboard handling, and focus reveal. The content transform and
internal child layouts belong to the controller. Supply a layout with
`setContentLayout`, or a manual extent with `setContentSize` (which clears that
layout). Offsets and pixel wheel units are logical UI units. Geometry changes clamp
existing offsets. Scrolling only changes painting/hit transforms and does not
invalidate logical layout measurement.

Enabled axes reserve scrollbar lanes even when content fits. This keeps available
width stable for wrapped content and avoids scrollbar/measurement oscillation.
Thumbs disappear when there is no overflow. Primary-pointer thumb dragging supports
capture and release outside; track clicks page by 90% of the viewport. Cancellation
or resize stops thumb dragging and keeps the current clamped offset.

Wheel events support both axes and line/pixel units. A view consumes only movement
it can use, then bubbles the residual to UI ancestors. `ScrollChaining::Contain`
stops at that view. Unused movement stays consumed for application routing, so a
list at its edge does not accidentally zoom an artboard. Custom handlers can return
`EventReply::remainingWheel` in the original units, preserving sign and never
increasing magnitude; invalid residuals throw through Context's callback-failure
path. `blockApplication` reserves an otherwise unhandled wheel event while still
allowing ancestor routing.

Arrows scroll by `lineStep`; Page Up/Down move by 90% of the main viewport axis.
Home/End move to its start/end, and Ctrl includes both axes. Descendant editors get
the event first and can consume their own navigation keys. Focus changes notify
ancestors from inner to outer, revealing the focused bounds through nested views.
`scrollToVisible(id, margin)` supports explicit reveal. Oversized targets align
their leading edge; transformed target bounds use their axis-aligned enclosure.

Context publishes layout geometry before sending `LayoutChanged` notifications,
including to manually placed containers visited by layout. Focus changes send
`FocusWithin` with `Event::related` identifying the focused descendant. These are
Context notifications, not public input events; their replies are ignored.

## Workspace example

`example/ui_workspace_demo.cpp` demonstrates toolbox, artboard, inspector, and
activity regions with three nested splitters. The toolbox contains a nested scroll
view; the artboard scrolls on both axes. Rows are focusable demonstration data;
editing controls, virtualization, docking, and saved layouts remain later work.
Passive surfaces keep steady colors under pointer movement.

```powershell
cmake --preset windows-debug -DVECTORGL_BUILD_UI=ON -DVECTORGL_BUILD_GLFW_ADAPTER=ON -DVECTORGL_BUILD_UI_EXAMPLES=ON
cmake --build build/windows-debug --config Debug --target vectorgl_ui_workspace
.\build\windows-debug\example\Debug\vectorgl_ui_workspace.exe
```

Options: `--font PATH`, `--width N`, `--height N`, `--smoke-test`, and
`--screenshot FILE.bmp`. Other CMake generators may omit the `Debug` output folder.
Headless tests cover split constraints, drag lifecycle, nested residuals, keyboard
navigation, focus reveal, resizing, and removal. GPU regressions check matching
clipped painting/picking at fractional DPI. Both UI-only and GLFW installed-package
consumers instantiate the public controllers.
