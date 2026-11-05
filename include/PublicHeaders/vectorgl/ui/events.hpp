#pragma once

#include <cstdint>
#include <string>

namespace vectorgl::ui
{
/*! Logical coordinates and semantic keys; no GLFW or native window types. */
enum class EventType
{
    PointerDown,
    PointerUp,
    PointerMove,
    Wheel,
    KeyDown,
    KeyRepeat,
    KeyUp,
    TextInput,
    WindowFocusLost,
    PointerLeave,
    Cancel,
    // Context-generated notifications, not accepted by dispatch().
    FocusGained,
    FocusLost,
    Timer
};
enum class Key
{
    None,
    Tab,
    Enter,
    Escape,
    Space,
    Backspace,
    Delete,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    PageUp,
    PageDown,
    A,
    C,
    V,
    X,
    Y,
    Z
};
enum class PointerButton
{
    None,
    Primary,
    Secondary,
    Middle
};
enum class WheelUnit
{
    Lines,
    Pixels
};
enum class CancelReason
{
    Explicit,
    WindowFocusLost,
    PointerLeftWindow,
    Unavailable,
    ModalChanged,
    HandlerFailed
};
struct Modifiers
{
    bool shift = false, control = false, alt = false, meta = false;
};
struct Event
{
    EventType type = EventType::PointerMove;
    float x = 0, y = 0; // Root logical coordinates, also during capture.
    std::uint64_t pointer = 0;
    PointerButton button = PointerButton::None;
    float wheelX = 0, wheelY = 0;
    WheelUnit wheelUnit = WheelUnit::Lines;
    Key key = Key::None;
    Modifiers modifiers;
    std::string text; // Committed UTF-8; no IME preedit contract in this slice.
    CancelReason reason = CancelReason::Explicit;
    float localX = 0, localY = 0; // Pointer coordinates mapped for each recipient.
};
/*! Handling is independent of value changes. No-op editing keys can be consumed.
 * Capture is granted only on PointerDown; it ends on the matching release.
 * Focus/capture requests also consume the event. Notifications ignore replies.
 */
struct EventReply
{
    bool handled = false;
    bool requestFocus = false;
    bool capturePointer = false;
};
} // namespace vectorgl::ui
