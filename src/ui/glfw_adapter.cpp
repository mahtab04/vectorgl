#define GLFW_INCLUDE_NONE
#include "vectorgl/ui/glfw_adapter.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "vectorgl/detail/utf8.hpp"

namespace vectorgl::ui
{
namespace
{
Modifiers modifiers(int mods)
{
    return {(mods & GLFW_MOD_SHIFT) != 0, (mods & GLFW_MOD_CONTROL) != 0, (mods & GLFW_MOD_ALT) != 0,
            (mods & GLFW_MOD_SUPER) != 0};
}
Key semanticKey(int key)
{
    switch (key)
    {
    case GLFW_KEY_TAB:
        return Key::Tab;
    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER:
        return Key::Enter;
    case GLFW_KEY_ESCAPE:
        return Key::Escape;
    case GLFW_KEY_SPACE:
        return Key::Space;
    case GLFW_KEY_BACKSPACE:
        return Key::Backspace;
    case GLFW_KEY_DELETE:
        return Key::Delete;
    case GLFW_KEY_LEFT:
        return Key::Left;
    case GLFW_KEY_RIGHT:
        return Key::Right;
    case GLFW_KEY_UP:
        return Key::Up;
    case GLFW_KEY_DOWN:
        return Key::Down;
    case GLFW_KEY_HOME:
        return Key::Home;
    case GLFW_KEY_END:
        return Key::End;
    case GLFW_KEY_PAGE_UP:
        return Key::PageUp;
    case GLFW_KEY_PAGE_DOWN:
        return Key::PageDown;
    case GLFW_KEY_A:
        return Key::A;
    case GLFW_KEY_C:
        return Key::C;
    case GLFW_KEY_V:
        return Key::V;
    case GLFW_KEY_X:
        return Key::X;
    case GLFW_KEY_Y:
        return Key::Y;
    case GLFW_KEY_Z:
        return Key::Z;
    default:
        return Key::None;
    }
}
ServiceResult lastError()
{
    const char* description = nullptr;
    const int error = glfwGetError(&description);
    return {error == GLFW_NO_ERROR             ? ServiceStatus::Success
            : error == GLFW_FORMAT_UNAVAILABLE ? ServiceStatus::Unavailable
                                               : ServiceStatus::Error,
            description ? description : ""};
}
} // namespace

WindowMetrics::WindowMetrics(int w, int h, int fw, int fh, float sx, float sy)
    : width_(w), height_(h), framebufferWidth_(fw), framebufferHeight_(fh), scaleX_(sx), scaleY_(sy)
{
    if (w < 0 || h < 0 || fw < 0 || fh < 0 || !std::isfinite(sx) || !std::isfinite(sy) || sx <= 0 || sy <= 0)
        throw std::invalid_argument("GLFW metrics require nonnegative sizes and positive finite scale");
}
std::optional<Viewport> WindowMetrics::viewport() const
{
    if (!width_ || !height_ || !framebufferWidth_ || !framebufferHeight_)
        return {};
    return Viewport(static_cast<float>(framebufferWidth_) / scaleX_, static_cast<float>(framebufferHeight_) / scaleY_,
                    framebufferWidth_, framebufferHeight_);
}
std::optional<Vec2> WindowMetrics::logicalPoint(double x, double y) const
{
    if (!viewport() || !std::isfinite(x) || !std::isfinite(y))
        return {};
    Vec2 result{static_cast<float>(x * framebufferWidth_ / width_ / scaleX_),
                static_cast<float>(y * framebufferHeight_ / height_ / scaleY_)};
    if (!std::isfinite(result.x) || !std::isfinite(result.y))
        return {};
    return result;
}
GlfwClipboard::GlfwClipboard(GLFWwindow* window) : window_(window)
{
    if (!window)
        throw std::invalid_argument("GLFW clipboard requires a live window");
}
ClipboardResult GlfwClipboard::read()
{
    glfwGetError(nullptr);
    const char* text = glfwGetClipboardString(window_);
    auto result = lastError();
    if (!text && result.status == ServiceStatus::Success)
        result.status = ServiceStatus::Unavailable;
    return {result, text ? text : ""};
}
ServiceResult GlfwClipboard::write(std::string_view text)
{
    if (text.find('\0') != std::string_view::npos)
        return {ServiceStatus::Error, "Clipboard text cannot contain embedded NUL"};
    const std::string terminated(text);
    glfwGetError(nullptr);
    glfwSetClipboardString(window_, terminated.c_str());
    return lastError();
}
struct GlfwAdapter::Impl
{
    GLFWwindow* window;
    Context& context;
    WindowMetrics metrics;
    std::array<GLFWcursor*, 6> cursors{};
    Impl(GLFWwindow* w, Context& c) : window(w), context(c) {}
    ~Impl()
    {
        for (auto* cursor : cursors)
            if (cursor)
                glfwDestroyCursor(cursor);
    }
    Modifiers currentModifiers() const
    {
        auto down = [&](int a, int b)
        { return glfwGetKey(window, a) == GLFW_PRESS || glfwGetKey(window, b) == GLFW_PRESS; };
        return {down(GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT), down(GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL),
                down(GLFW_KEY_LEFT_ALT, GLFW_KEY_RIGHT_ALT), down(GLFW_KEY_LEFT_SUPER, GLFW_KEY_RIGHT_SUPER)};
    }
    std::optional<Event> pointer(EventType type)
    {
        double x = 0, y = 0;
        glfwGetCursorPos(window, &x, &y);
        auto point = metrics.logicalPoint(x, y);
        if (!point)
            return {};
        Event event;
        event.type = type;
        event.x = point->x;
        event.y = point->y;
        event.modifiers = currentModifiers();
        return event;
    }
};
GlfwAdapter::GlfwAdapter(GLFWwindow* window, Context& context)
{
    if (!window)
        throw std::invalid_argument("GLFW adapter requires a live window");
    impl_ = std::make_unique<Impl>(window, context);
    refreshMetrics();
}
GlfwAdapter::~GlfwAdapter() = default;
void GlfwAdapter::refreshMetrics()
{
    int w = 0, h = 0, fw = 0, fh = 0;
    float sx = 1, sy = 1;
    glfwGetWindowSize(impl_->window, &w, &h);
    glfwGetFramebufferSize(impl_->window, &fw, &fh);
    glfwGetWindowContentScale(impl_->window, &sx, &sy);
    WindowMetrics next(w, h, fw, fh, sx, sy);
    if (next != impl_->metrics)
    {
        impl_->metrics = next;
        impl_->context.invalidate(Invalidation::All);
    }
}
const WindowMetrics& GlfwAdapter::metrics() const
{
    return impl_->metrics;
}
bool GlfwAdapter::cursorPosition(double x, double y)
{
    refreshMetrics();
    const auto point = impl_->metrics.logicalPoint(x, y);
    if (!point)
        return false;
    Event event;
    event.type = EventType::PointerMove;
    event.x = point->x;
    event.y = point->y;
    event.modifiers = impl_->currentModifiers();
    return impl_->context.dispatch(event);
}
bool GlfwAdapter::mouseButton(int button, int action, int mods)
{
    if ((action != GLFW_PRESS && action != GLFW_RELEASE) ||
        (button != GLFW_MOUSE_BUTTON_LEFT && button != GLFW_MOUSE_BUTTON_RIGHT && button != GLFW_MOUSE_BUTTON_MIDDLE))
        return false;
    refreshMetrics();
    auto event = impl_->pointer(action == GLFW_PRESS ? EventType::PointerDown : EventType::PointerUp);
    if (!event)
        return false;
    event->button = button == GLFW_MOUSE_BUTTON_LEFT    ? PointerButton::Primary
                    : button == GLFW_MOUSE_BUTTON_RIGHT ? PointerButton::Secondary
                                                        : PointerButton::Middle;
    event->modifiers = modifiers(mods);
    return impl_->context.dispatch(*event);
}
bool GlfwAdapter::scroll(double x, double y)
{
    if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > std::numeric_limits<float>::max() ||
        std::abs(y) > std::numeric_limits<float>::max())
        return false;
    refreshMetrics();
    auto event = impl_->pointer(EventType::Wheel);
    if (!event)
        return false;
    event->wheelX = static_cast<float>(x);
    event->wheelY = static_cast<float>(y);
    return impl_->context.dispatch(*event);
}
bool GlfwAdapter::key(int code, int action, int mods)
{
    if (action != GLFW_PRESS && action != GLFW_RELEASE && action != GLFW_REPEAT)
        return false;
    Event event;
    event.type = action == GLFW_PRESS    ? EventType::KeyDown
                 : action == GLFW_REPEAT ? EventType::KeyRepeat
                                         : EventType::KeyUp;
    event.key = semanticKey(code);
    if (event.key == Key::None)
        return false;
    event.modifiers = modifiers(mods);
    return impl_->context.dispatch(event);
}
bool GlfwAdapter::character(unsigned int codepoint)
{
    if (codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint < 32 || codepoint == 127)
        return false;
    Event event;
    event.type = EventType::TextInput;
    vectorgl::detail::appendCodepoint(event.text, codepoint);
    return impl_->context.dispatch(event);
}
bool GlfwAdapter::focusChanged(bool focused)
{
    if (focused)
    {
        refreshMetrics();
        return false;
    }
    Event event;
    event.type = EventType::WindowFocusLost;
    return impl_->context.dispatch(event);
}
bool GlfwAdapter::cursorEntered(bool entered)
{
    if (entered)
        return false;
    Event event;
    event.type = EventType::PointerLeave;
    return impl_->context.dispatch(event);
}
ServiceResult GlfwAdapter::setCursor(Cursor cursor)
{
    const auto index = static_cast<std::size_t>(cursor);
    if (index >= impl_->cursors.size())
        return {ServiceStatus::Error, "Invalid cursor"};
    constexpr std::array<int, 6> shapes{GLFW_ARROW_CURSOR, GLFW_IBEAM_CURSOR,   GLFW_CROSSHAIR_CURSOR,
                                        GLFW_HAND_CURSOR,  GLFW_HRESIZE_CURSOR, GLFW_VRESIZE_CURSOR};
    glfwGetError(nullptr);
    if (!impl_->cursors[index])
        impl_->cursors[index] = glfwCreateStandardCursor(shapes[index]);
    if (!impl_->cursors[index])
    {
        auto result = lastError();
        if (result.status == ServiceStatus::Success)
            result = {ServiceStatus::Unavailable, "Standard cursor unavailable"};
        return result;
    }
    glfwSetCursor(impl_->window, impl_->cursors[index]);
    return lastError();
}
} // namespace vectorgl::ui
