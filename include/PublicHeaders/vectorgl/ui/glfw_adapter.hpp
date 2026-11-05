#pragma once

#include <memory>
#include <optional>

#include "vectorgl/ui/context.hpp"

struct GLFWwindow;
namespace vectorgl::ui
{
/*! GLFW screen coordinates, framebuffer pixels and desktop content scale are
 * distinct. Logical size = framebuffer/contentScale; cursor conversion accounts
 * for both framebuffer/window ratio and content scale (including Retina).
 */
class WindowMetrics
{
public:
    WindowMetrics(int windowWidth = 0, int windowHeight = 0, int framebufferWidth = 0, int framebufferHeight = 0,
                  float contentScaleX = 1, float contentScaleY = 1);
    std::optional<Viewport> viewport() const;
    std::optional<Vec2> logicalPoint(double windowX, double windowY) const;
    bool operator==(const WindowMetrics&) const = default;

private:
    int width_, height_, framebufferWidth_, framebufferHeight_;
    float scaleX_, scaleY_;
};
enum class Cursor
{
    Arrow,
    Text,
    Crosshair,
    Hand,
    ResizeHorizontal,
    ResizeVertical
};

/*! Explicit native clipboard service. Window must outlive this service and its
 * clients; invoke on GLFW's main thread. Not installed automatically in Context.
 */
class GlfwClipboard final : public Clipboard
{
public:
    explicit GlfwClipboard(GLFWwindow* window);
    ClipboardResult read() override;
    ServiceResult write(std::string_view text) override;

private:
    GLFWwindow* window_;
};

/*! Non-owning GLFW host adapter. Installs no callbacks and never changes the
 * window user pointer. Forward events explicitly from application callbacks,
 * catching exceptions there. Window and Context must outlive this object.
 * Does not create/bind GL contexts, clear framebuffers, or run an event loop.
 */
class GlfwAdapter
{
public:
    GlfwAdapter(GLFWwindow* window, Context& context);
    ~GlfwAdapter();
    GlfwAdapter(const GlfwAdapter&) = delete;
    GlfwAdapter& operator=(const GlfwAdapter&) = delete;
    void refreshMetrics(); // Call on window/framebuffer/content-scale notifications.
    const WindowMetrics& metrics() const;
    bool cursorPosition(double x, double y);
    bool mouseButton(int button, int action, int mods);
    bool scroll(double x, double y);
    bool key(int key, int action, int mods);
    bool character(unsigned int codepoint);
    bool focusChanged(bool focused);
    bool cursorEntered(bool entered);
    ServiceResult setCursor(Cursor cursor);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace vectorgl::ui
