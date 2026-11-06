# Typed in-process drag/drop

Drag/drop is routed by `ui::Context` and registered `Widget::onDrag` handlers. It
shares the existing pointer capture, clipping, transform, overlay, modal, and
generation-checked widget lifecycle. No native-window types or GL context are
needed by the state machine. Include `<vectorgl/ui/context.hpp>` and link
`vectorgl::ui`.

```cpp
struct Symbol { std::string name; };
vectorgl::ui::Widget source;
source.onEvent = [](auto& ui, auto id, const auto& event) {
    if (event.type == vectorgl::ui::EventType::PointerDown &&
        event.button == vectorgl::ui::PointerButton::Primary)
        ui.beginDrag(id, vectorgl::ui::DragPayload::make(Symbol{"Panel"}),
                     [](const auto& result) { /* dropped, rejected, or cancelled */ });
    return vectorgl::ui::EventReply{true};
};
vectorgl::ui::Widget artboard;
artboard.onDrag = [](auto&, auto, const vectorgl::ui::DragEvent& event) {
    const auto* symbol = event.payload.get<Symbol>();
    if (!symbol) return false;
    if (event.phase == vectorgl::ui::DragPhase::Drop) {
        // Apply the application operation at event.localX/localY.
    }
    return true;
};
```

## Payload and start contract

DragPayload owns a copyable C++ value through shared immutable `std::any` storage.
Copies/events/results share ownership; `get<T>()` returns a pointer only for the
exact stored type, and never transfers ownership. Direct raw-pointer payload types
are rejected; use owned values or explicit shared asset ownership. Referents inside
custom payloads remain the application's lifetime/thread responsibility. This is
an in-process C++ contract, not a serialization or OS MIME/clipboard format.

Call `beginDrag(source, payload, finished)` from that source's primary PointerDown,
or its captured matching PointerMove callback. The latter lets an application
implement a logical movement threshold, as the workspace example does. The source
must be registered, enabled/visible, nonsingular, and inside the active modal scope.
The method returns false outside eligible source input, when another widget owns
capture, or while another drag exists. An empty payload throws. A successful start
owns primary capture for that pointer. Cross-window/process and OS file drops are
outside this API.

## Targeting, feedback, and completion

The topmost normal hit target is found using Context's existing ancestor clips,
transforms, hit visibility, overlay order, and modal boundary. Its deepest ancestor
with `onDrag` is the drop target. Rejection does not fall through to a parent or a
widget hidden behind it. Notifications include root logical and target-local
coordinates, source/target IDs, modifiers, and owned payload.

Targets receive Enter on entry, Move for matching pointer updates, Leave on exit,
and Drop only if the current feedback reply accepts the payload. Enter/Move return
whether the drop is acceptable at that position; Drop returns whether the operation
was applied. Leave's return is ignored. `dragState()` supplies source, target,
payload, position, and current acceptance for painting feedback. Hover target
changes caused by clipping/layout/removal are reconciled at Context boundaries;
targets should not request repaint/layout repeatedly for unchanged feedback.

Matching moves/releases are consumed by the drag system instead of activating the
source control or clicking the target. Other pointer IDs cannot finish the session.
Keyboard/text events are consumed during a drag; Escape cancels before application
shortcuts. Wheel input continues through existing UI scroll routing. A release
without an accepting target completes as Rejected. A successful Drop completes as
Dropped; Leave clears feedback afterward. The source receives Cancel to reset its
ordinary pressed state, then the completion callback runs exactly once.

Escape, `cancelDrag`, capture cancellation, window exit/focus loss, source removal,
disabled/hidden/singular source ancestry, and modal interruption complete as Cancelled.
No later release can repeat a completion. Removal of a target sends Leave/clears
feedback while the drag may continue to another target. Removed registrations stay
alive for cleanup notifications until dispatch settles, without reusing stale IDs.

Callbacks may remove source/target widgets, open a modal, or cancel the session.
Each subsequent step revalidates the session and handles. Exceptions propagate
after capture/session cleanup under Context's normal host-boundary error contract.
Completion references are ephemeral; copy a result/payload to retain it. Destroying
the Context releases payload ownership but does not run client callbacks from its
destructor. No polling/timer is added by drag/drop itself.

## Integrated example

Build `vectorgl_ui_editor_workspace` from
`example/ui_editor_workspace_demo.cpp`. It starts drags after six logical pixels,
paints accepted/rejected previews on a separate scene artboard, converts positions
through artboard zoom, and supports keyboard placement through Enter/the toolbar.
The unsupported toolbox item demonstrates typed rejection. See the
[workspace validation guide](ui-editor-workspace.md) for the roadmap milestone and
its local/CI acceptance status.
