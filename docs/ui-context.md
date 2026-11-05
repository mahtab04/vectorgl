# Optional application UI context

VectorGL's optional `vectorgl::ui` target supplies widget registration, input routing,
keyboard focus, pointer capture, modal scopes, and repaint/wakeup scheduling.
It does not create a window or initialize OpenGL. This module supplies the input
foundation and automatic layout for application controls.

The existing `vectorgl/ui.hpp` Label/Button/TextBox helpers remain unchanged.
Include `vectorgl/ui/context.hpp` to use the new module. `Widget` is a registration
descriptor with an event callback and an optional WidgetPainter strategy. The app
supplies local rectangles and affine transforms; Context paints the tree with shared
themes and matching clip-aware hit tests. The optional GLFW adapter translates host
events and sizing. See [painting and host integration](ui-glfw.md). Optional row,
column, and grid strategies provide [automatic layout](ui-layout.md); richer controls
remain later slices.

## Build and run the routing example

```sh
cmake -S . -B build-ui -DVECTORGL_BUILD_UI=ON -DVECTORGL_BUILD_UI_EXAMPLES=ON -DVECTORGL_BUILD_EXAMPLES=OFF -DVECTORGL_BUILD_APPS=OFF -DVECTORGL_BUILD_GPU_TESTS=OFF
cmake --build build-ui --config Debug --target vectorgl_ui_routing vectorgl_ui_tests
ctest --test-dir build-ui -C Debug -R vectorgl_ui --output-on-failure
```

`vectorgl_ui_routing` is a deterministic console example, not a graphical showcase.
It exercises two existing TextBoxes, committed UTF-8 input, a keyboard-activated
button, a captured drag, a modal barrier, and focused widget removal. It prints the
resulting values and exits nonzero on incorrect routing. It needs neither GLFW,
a font file, a display server, nor an OpenGL context.

The executable is under `build-ui/example/` for single-configuration generators;
Visual Studio adds the configuration subdirectory, such as `example/Debug/`.

Both the module and its example are optional. `VECTORGL_BUILD_UI` defaults OFF.
`VECTORGL_BUILD_UI_EXAMPLES` follows the existing examples setting by default, but
can be enabled independently. UI-only headless builds do not fetch GLFW; the normal
examples/apps/GPU-test options still request GLFW when enabled.

For an installed package built with UI enabled:

```cmake
find_package(vectorgl CONFIG REQUIRED COMPONENTS ui)
target_link_libraries(my_editor PRIVATE vectorgl::ui)
```

Base package discovery imports only the graphics target; requesting a missing `ui`
component fails. New nested UI headers are installed only with the optional module.
`tests/ui_package_consumer` exercises that public installed contract.

## Register widgets and dispatch input

```cpp
#include <vectorgl/ui/context.hpp>
#include <utility>

using namespace vectorgl::ui;
Context ui;
Widget field;
field.bounds = {20, 20, 240, 40};
field.focusable = true;
field.onEvent = [](Context& context, WidgetId self, const Event& event)
{
    if (event.type == EventType::PointerDown)
        return EventReply{true, true}; // handled + request focus
    if (event.type == EventType::KeyDown && event.key == Key::Delete)
        return EventReply{true}; // consumed, even if no text changed
    return EventReply{};
};
auto fieldId = ui.add(std::move(field));

Event event;
event.type = EventType::KeyDown;
event.key = Key::Tab;
bool consumed = ui.dispatch(event);
// Offer unconsumed events to application/artboard commands only afterward.
```

The host supplies root logical pointer coordinates, button/modifier state, pointer
IDs, semantic key actions, and committed UTF-8 text. Wheel events carry both axes
and explicit line/pixel units. The host owns UTF-8 validation, key mapping, and
physical-to-logical conversion; the Context transports text without decoding it.
Existing TextBox APIs can validate/filter it. Preedit/IME is not implemented here.

- Pointer routing targets the topmost eligible widget; later siblings are above
  earlier siblings, children above parents. Bounds use half-open edges.
- `clipChildren` excludes descendants outside an ancestor's rectangle. Hidden or
  disabled ancestors exclude their entire subtree from hits/focus/capture.
- Unhandled events bubble through parents. Keys and text start at the focus owner.
  Only events left unconsumed should reach app shortcuts. A handler must explicitly
  consume its editing keys, including no-op edits; a callback's `handled` result
  is not a "value changed" result.
- Tab/Shift+Tab traverse eligible focusable registrations in registration order,
  wrapping inside the active modal scope. Key release does not traverse again.
  Modified Ctrl/Alt/Meta+Tab is left to normal routing.
- Focus changes emit `FocusLost` then `FocusGained`. Pointer press chooses the
  nearest focusable target/ancestor; explicit `requestFocus` can override it.
  Background clicks without a focusable target retain focus in this slice.
- Handled input requests paint invalidation automatically. Unhandled callbacks
  that mutate presentation should call `invalidate()` themselves.

## Capture and modal scopes

Return `{true, false, true}` from PointerDown to capture its pointer and initiating
button. The Context supports one capture owner at a time. Matching pointer motion
and release reach the owner outside its rectangle; another pointer does not steal
capture. Matching button release ends capture once. Wheel input uses ordinary hits.

Forward `PointerLeave` for leaving the *window*, not each control. Window leave,
`WindowFocusLost`, Escape during a capture, explicit cancellation, removal,
disabling/hiding, or modal changes send one `Cancel` notification with a reason.
The Context clears capture before that notification, and notification replies cannot
recapture. Window focus loss also clears keyboard focus. Controls must clear their
own pressed/dragging visuals on Cancel and keyboard pressed state on FocusLost.

This is logical routing, not native OS capture or cross-window drag-and-drop.
The host must deliver leave/focus/cancel notifications when native input is interrupted.

`pushModal(root)` restricts hits/focus/input to a registered subtree, cancels capture,
and focuses its first eligible control. Even an empty modal consumes input outside
its subtree. Nested modals must belong to the current modal subtree. `popModal()`
restores the previous eligible focus owner, falling back to the first eligible
control or no focus. Removing/hiding/disabling a modal root unwinds it and its nested
scopes. Modal widgets are still ordinary registrations after pop: hide/remove them
if their content should no longer accept ordinary input.

## Lifetime and failure rules

Context owns descriptors/callbacks, not application data captured by callbacks.
Captured data must outlive registration. All access belongs to one UI thread.
The Context is noncopyable and nonmovable so callbacks can safely refer to it.

Widget IDs carry a Context identity and slot generation. Removing a widget removes
its subtree, immediately invalidates those IDs, and defers storage destruction until
callbacks unwind. Lost-focus/cancel notifications still reach the removed owner.
Stale removal is harmless and cannot remove a replacement that reuses the slot.
Setters ignore stale IDs; registration rejects a stale or foreign parent. Tree depth
is bounded at 256 levels to bound traversal stack usage.

Callbacks may add/remove controls, request focus, and open/close modal scopes.
New registrations become routable when the current callback transaction settles.
Focus notifications and cancellation settle before returning from dispatch. Calling
`dispatch()` or `pumpTimers()` recursively from a callback throws `logic_error`.

Exceptions from widget handlers are rethrown after deferred changes settle and
capture is cancelled. Hosts must catch exceptions at native callback boundaries.
Event dispatch touches no render target or GL state. Painting requires an active Canvas
frame and scopes Canvas styling/transforms/clips. Pathological notification cycles are capped
at 1,024 transitions, clear focus/capture, and throw `logic_error`.
Destruction does not invoke application callbacks; tear down application operations
explicitly if they need cancellation notifications before Context destruction.

## Invalidation, clocks, and test services

`takeInvalidation()` returns/clears dirty flags. Measure implies Arrange and Paint;
Arrange implies Paint. Semantics is reserved for future accessibility-tree work,
not an accessibility implementation. Initially all flags are dirty.

Inject `Clock` through `Services` for deterministic tests; otherwise Context uses
`steady_clock`. Deadlines use the same finite monotonic seconds as `now()`.
`requestWakeup(id, deadline)` replaces the widget's deadline, `cancelWakeup(id)`
clears it. `nextWakeup()` returns the earliest deadline among currently eligible
widgets; no deadline means no timer-driven wakeup is needed. The host should wait
until input/invalidation or that deadline, using a zero timeout for overdue timers.

`pumpTimers()` delivers each currently due Timer once and requests paint. Handlers
can reschedule; the function does not busy-loop if they reschedule immediately.
Hidden/disabled/out-of-modal-scope deadlines are suspended rather than lost.
Removing a widget discards its deadline. Timer callbacks may change eligibility or
reschedule other timers before they are delivered.

`Services::clipboard` is an optional injectable interface with explicit results.
Null means unavailable. No system provider is installed and no user clipboard is
touched by the tests/example. Clipboard clients must write successfully before
deleting selected text on Cut. The optional GLFW adapter provides native clipboard/cursor services;
dialog services remain a later slice.

Behavior tests cover ID reuse and foreign IDs, focus traversal, no-op key consumption,
ancestor clipping, bubbling and wheel units, capture identity/cancellation, modal
isolation/restoration, callback-driven mutation/failure, mutable callback state,
deadlines, invalidation, and injected services. Existing graphics regressions remain
separate from these display-independent behavior tests.
