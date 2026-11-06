# Popups, menus, tooltips, and modal dialogs

Include `<vectorgl/ui/overlays.hpp>` and link `vectorgl::ui`. OverlayManager owns
transient widgets and uses Context's routing, modal stack, focus/capture cleanup,
painting, and timers. ComboBox composes the shared control interaction with a
scrollable popup list. Menus use the same list with disabled items and separators;
context menus additionally accept a root-coordinate position.

```cpp
vectorgl::ui::Context ui;
vectorgl::ui::OverlayManager overlays(ui);
overlays.update({0, 0, 1280, 800});
vectorgl::ui::ComboBoxOptions options;
options.items = {{"Filled"}, {"Outline"}, {"Unavailable", false}};
options.selected = 0;
options.onChanged = [](std::size_t index) { /* update application model */ };
vectorgl::ui::ComboBox choice(ui, overlays, {}, options);
ui.layout(choice.id(), {20, 20, 240, 44});
overlays.setTooltip(choice.id(), "Choose the appearance");
```

## Host boundary and ownership

Use one OverlayManager per Context. Context must outlive the manager; the manager
must outlive ComboBox controllers. Controllers own their registered subtrees.
Overlay IDs are independent of WidgetId and cannot accidentally address another
manager's entries. Do not remove or modify private overlay widgets directly.

Call `update(viewport)` before/after input, after application layout, and after
`pumpTimers`, outside Context callbacks. Initialize it before opening overlays.
The viewport is a positive, finite Bounds in root logical coordinates, independent
of framebuffer scaling. Opening/closing/selection may occur inside input callbacks;
the subsequent update arranges popup content. Paint once through `Context::paint`;
there is no separate overlay render pass to call.

Use this manager for the window's modal scopes while overlays are active. Mixing
external `pushModal`/`popModal` calls with the manager is unsupported; opening into
an unrelated external scope throws. Nested manager dialogs/popups are supported.
Only an owner inside the current modal scope may open an anchored popup.

Closing removes descendant overlays, releases capture, and restores eligible
previous focus. Closing an unknown/already closed ID is a no-op. `closeAll()` also
dismisses pending/visible tooltips. Call it before destroying application callback
targets. Destructors suppress callback exceptions; explicit close permits handling
errors. Cleanup completes before dismissal callbacks, and one throwing callback
does not leave other removed overlays registered. When called from dispatch,
Context settles focus notifications after the input callback unwinds.

## Placement and list behavior

Anchors use the owner's transformed root-coordinate bounding rectangle. Lists open
below the anchor, flip above when space permits, then clamp to the viewport with
a small inset. Context menu coordinates are also flipped/clamped. Popup height is
bounded by `maximumHeight` and viewport height; a ScrollView handles longer lists.
ComboBox matches its arranged trigger width. Dialogs center and shrink to fit.
Tiny windows may clip dialog text/actions; layout never produces negative sizes.

Owners must remain live, visible/enabled through their ancestry, and invertibly
transformed. Updates close popups whose owner becomes unavailable. Anchors track
layout/transform changes. Popup modal scopes paint above application content,
including widgets registered later, and escape the owner's ordinary clipping tree.
Window focus loss dismisses popups/tooltips; dialogs remain for explicit decisions.

Up/Down wrap between enabled list rows; Home/End choose the first/last enabled row.
Enter/Space activate on release using the shared Button behavior. Disabled rows and
separators cannot be selected. Focus navigation reveals the row; wheel scrolling
retains its offset during unchanged layout updates. Tab stays within the active
scope. Internal ScrollView focus is disabled so disabled-row clicks cannot steal
list navigation; the enclosing scope remains a focusable fallback for empty lists.
Escape closes the popup. An outside pointer press dismisses and is consumed,
so its matching release cannot activate an underlying button. Outside wheel/key
input stays isolated from application shortcuts through the modal barrier.

Popup `onSelected(index)` fires for an actual selection, after closing the popup.
`onClosed(reason)` fires once for any dismissal; descendant closures use
Programmatic. ComboBox emits `onChanged` only when the selected index changes.
Programmatic `setSelected` does not emit it. Empty/all-disabled lists retain a
focusable scope so Escape and outside dismissal remain available. Cascading menus,
typeahead search, and native OS menus are outside this slice.

## Dialog decisions

```cpp
vectorgl::ui::DialogOptions dialog;
dialog.title = "Save changes?";
dialog.message = "Save, discard, or continue editing.";
dialog.actions = {{"Save", 1}, {"Discard", 2}, {"Cancel", 0}};
dialog.defaultAction = 0; // Index of an enabled action, not its result value.
dialog.cancelAction = 2;
dialog.onFinished = [](std::optional<int> result) { /* handle decision */ };
auto id = overlays.openDialog(std::move(dialog));
```

Dialogs trap Tab and block outside clicks without dismissing. The default action
gets initial focus. Enter activates the focused button; when the dialog scope
itself receives Enter, it activates the configured default. Escape selects the
configured cancel action, or closes with no result if none is configured.
`onFinished` fires once: selected action value, or nullopt for programmatic/removal
cancellation. Nested scopes restore dialog focus before returning to application
focus. Application code owns saving/discarding data; these are in-process dialogs,
not native file pickers. Native dialog/platform services remain separate work.

## Tooltips and core integration

`setTooltip(owner, text, options)` binds delayed help to hover or keyboard focus.
The delay and visible duration use Context's injected monotonic Clock. Moving
within an owner does not restart the delay. Expiry suppresses repeated display
until the owner is left/revisited. Owner removal, opening an interactive overlay,
window focus loss, explicit removal/closeAll, and Escape dismiss help. Escape on a
visible tooltip is consumed before application shortcuts. Tooltips neither take
focus nor intercept pointer input. Bindings for removed widgets are pruned.
Interactive overlays suppress tooltips until their scope closes.

`Widget::hitTestVisible=false` skips pointer hits for that subtree.
`Widget::overlay=true` raises top-level painting before modal scopes; interactive
overlays still need a modal barrier for input ordering. Context forwards window
focus loss to registered overlay roots and offers Escape to noninteractive overlay
handlers when no capture/modal scope is active. `isAvailable` queries live
visible/enabled ancestry without applying modal eligibility. Modal push now retains
pending requested focus, allowing a selection callback to replace its popup with
a dialog and later restore the original trigger.

## Demo and validation

Build `vectorgl_ui_controls` from `example/ui_controls_demo.cpp`. It includes a
Style ComboBox, delayed help, File menu, right-click menu, and Save/Discard/Cancel.

```powershell
cmake --preset windows-debug -DVECTORGL_BUILD_UI=ON -DVECTORGL_BUILD_GLFW_ADAPTER=ON -DVECTORGL_BUILD_UI_EXAMPLES=ON
cmake --build build/windows-debug --config Debug --target vectorgl_ui_controls
.\build\windows-debug\example\Debug\vectorgl_ui_controls.exe
```

`--show-dialog` or `--show-menu` starts with an overlay open, useful with
`--screenshot FILE.bmp`. Existing font/size/smoke-test options remain available.
Headless tests exercise placement, navigation, disabled/separator rows, scrolling,
focus restoration, nested scopes, replacement from callbacks, tooltip deadlines,
owner loss, foreign IDs, and cleanup on callback failure. GPU checks verify popup
painting/picking above later content at 100%, 125%, 150%, and 200% scale. Installed
consumers instantiate and open a ComboBox through public APIs.
