# Routed controls

Include `<vectorgl/ui/controls.hpp>` and link `vectorgl::ui`. Button, CheckBox,
TextField, and NumericEditor use Context's focus, capture, modal scopes, clipping,
layout, and invalidation. The existing flat `ui.hpp` and direct TextBox API remain
unchanged.

Also include `<vectorgl/canvas.hpp>` when constructing the Canvas itself.

```cpp
vectorgl::Canvas canvas; // Initialize with a current GL context before painting.
vectorgl::ui::Context ui;
vectorgl::ui::TextFieldOptions options;
options.value = "Orbit card";
options.validate = [](std::string_view text) {
    return text.empty() ? "Name is required" : "";
};
options.onEdit = [](const vectorgl::ui::EditEvent& event) {
    // Changed carries the buffer/error; Committed carries the accepted value.
};
vectorgl::ui::TextField name(ui, canvas, {}, options);
ui.layout(name.id(), {20, 20, 300, 44});
```

Context and the supplied Canvas must outlive the controllers. Use that same Canvas
for painting and pointer text measurement. Destroy UI/Canvas resources before the
native GL context. Keyboard editing, validation, number policies, activation, and
timers work headlessly with an uninitialized Canvas and no font. Pointer caret
measurement/rendering require a font and a current GL context.

Controllers remove their registered subtree on destruction. Destructors suppress
callback exceptions; explicit `Context::remove(id())` permits handling those
errors. Clear/remove the UI root while application callback targets are still
alive during application shutdown. Callback-driven removal is supported.

## Activation and styles

Button activates once on primary-pointer release inside, including dragging out
and back in. Release outside or cancellation does not activate. Enter/Space
activate on matching key release; repeats do not repeat the action. Escape/lost
focus disarm a keyboard press. Disabled/hidden controls and descendants leave
input and focus traversal through Context.

CheckBox flips its checked value before `onChanged`. `CheckStyle::Toggle` changes
appearance, not interaction. Programmatic setters do not emit user actions.
`controlColors(theme, state)` resolves enabled, hover, focus, press, check, and
invalid state. Disabled colors take precedence; invalid borders override focus.
`Theme::pressed` and `Theme::error` supplement existing surface/hover/accent tokens.
Check marks, switch position, focus outlines, and application validation text
provide cues beyond color. This is not a native accessibility bridge.

Preferred sizes are explicit logical dimensions constrained by layout hints. Long
button labels clip to the control; choose sizes appropriate to labels/font scale.
NumericEditor contains a flexible field and two step buttons; `fieldId()` selects
its editable focus target.

## Editing and completion

TextField wraps the existing single-line UTF-8 codepoint editor. Arrows, Home/End,
Shift selection, Backspace/Delete, pointer selection, and Ctrl/Cmd+A/C/X/V route
through Context. No-op editing keys remain consumed so Delete cannot accidentally
delete an artboard item. Tab remains Context focus traversal. Unsupported undo/redo
shortcuts are consumed to isolate them from document undo. Local undo, word
navigation, grapheme segmentation, IME, shaping/bidi, and multiline editing are
outside this slice.

`text()` is the buffer; `committedText()` is the last accepted value. Validation
returns empty for valid input or an explanation exposed by `error()`. Validators
must be deterministic and side-effect-free. Initial committed values must be valid;
invalid intermediate buffers remain editable. Validation is not a text filter.

An actual text change starts an edit session and emits Changed. Enter commits a
valid buffer and emits one Committed event. Invalid Enter leaves the session
active. Escape restores the committed value and emits Cancelled. Repeated
Enter/Escape or subsequent blur do not repeat completion. A new text change starts
the next session even if focus never left. Pointer cancellation ends dragging;
explicit cancellation also cancels the edit session.

Blur policy is explicit: `CommitOrCancel` (default) commits valid buffers and
restores invalid ones; `Cancel` always restores. This includes blur from disabling,
removing, or hiding a focused field. Applications should display Changed errors
in visible text as the example does. `setText` validates and silently replaces
buffer/committed value; `editText` replaces the buffer and emits Changed, allowing
intermediate text. `commit()` returns false for invalid input; `cancel()` restores.
Caret timers run only while focused, preserving event-driven host waiting.

Clipboard comes from `Context::services()`. Copy never mutates. Cut deletes only
after a successful write. Failed/unavailable paste preserves buffer and selection;
successful paste repairs UTF-8 and filters single-line control characters as
TextBox does. `onServiceError` reports failure/cancellation/unavailability. Automated
tests inject clipboard services and never touch the system clipboard.

## Numeric policy

NumericEditor separates committed numeric `value()` from editable `text()`. Empty
text, a leading minus, and out-of-range values can be typed but cannot commit.
Enter/Escape/blur use the same session rules as TextField. `commit()`, `cancel()`,
`editing()`, and `error()` also support explicit application actions. NumberEditEvent carries
the last committed value for Changed/Cancelled and the new value for Committed.
Plus/minus buttons and Up/Down step within the range. Invalid buffers cannot step
with the keyboard; focusing a step button first applies blur policy. No-op edge
steps emit no completion. `step(-1/+1)` offers the same operation to applications.
Programmatic setters/step calls are application operations; disabled-state native
input filtering belongs to Context.

Default InvariantNumberFormat consumes the entire input, using a decimal point,
optional minus/exponent, and no grouping, whitespace, or leading plus. Nonfinite
values are rejected. Formatting uses fixed precision from 0 to 12. Range/value/step
must be finite and step positive. Formatted initial/set/step values are parsed back
and stored at configured precision. A rounded result outside the range is rejected
instead of silently storing an out-of-range number.

Inject NumberFormat for application locale policy. Parse/format must agree and
must not change global locale. Tests exercise a comma-decimal policy. The interface
does not automatically select the user's locale or claim universal locale support.

## Inspector demo

`example/ui_controls_demo.cpp` shows a required name, numeric editor, checkbox,
toggle, disabled action, Apply/Reset buttons, visible validation, and a committed
value preview. Narrow windows stack panels. Library controls supply editing and
activation; the example supplies its property model/callbacks.

```powershell
cmake --preset windows-debug -DVECTORGL_BUILD_UI=ON -DVECTORGL_BUILD_GLFW_ADAPTER=ON -DVECTORGL_BUILD_UI_EXAMPLES=ON
cmake --build build/windows-debug --config Debug --target vectorgl_ui_controls
.\build\windows-debug\example\Debug\vectorgl_ui_controls.exe
```

Options: `--font PATH`, `--width N`, `--height N`, `--smoke-test`, and
`--screenshot FILE.bmp`. Headless regressions cover activation/cancellation,
disabled state, callback removal, Unicode editing, clipboard failures, completion,
blur, caret deadlines, numeric bounds/stepping, and injected policies. GPU tests
check state colors and pointer caret placement at 100%, 125%, 150%, and 200% scale.
Installed consumers exercise the public API without example-private headers.
