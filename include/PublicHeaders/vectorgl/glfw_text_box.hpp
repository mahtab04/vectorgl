#pragma once

#include <GLFW/glfw3.h>

#include <string>

#include "vectorgl/text_box.hpp"

namespace vectorgl
{

/*! @brief GLFW input adapter for @ref TextBox.
 *
 *  This helper translates GLFW character, key, mouse, and clipboard events
 *  into calls on a bound @ref TextBox.  It keeps the core widget independent
 *  of any windowing system while still providing an easy integration path for
 *  GLFW-based applications.
 *
 *  Typical usage:
 *  @code
 *  vectorgl::GlfwTextBoxController controller;
 *  controller.bind(field, canvas, window);
 *
 *  glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int cp) {
 *      auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(w));
 *      app->controller.handleChar(cp);
 *  });
 *  @endcode
 */
class GlfwTextBoxController
{
public:
    /*! @brief Constructs an unbound controller. */
    GlfwTextBoxController() = default;

    /*! @brief Constructs and binds the controller.
     *  @param[in] textBox Textbox that will receive translated events.
     *  @param[in] canvas  Canvas used for pointer hit-testing and measurement.
     *  @param[in] window  Optional GLFW window used for clipboard integration.
     */
    GlfwTextBoxController(TextBox& textBox, Canvas& canvas, GLFWwindow* window = nullptr)
        : textBox_(&textBox), canvas_(&canvas), window_(window)
    {
    }

    /*! @brief Binds the controller to a textbox, canvas, and optional window.
     *  @param[in] textBox Textbox that will receive translated events.
     *  @param[in] canvas  Canvas used for pointer hit-testing and measurement.
     *  @param[in] window  Optional GLFW window used for clipboard integration.
     */
    void bind(TextBox& textBox, Canvas& canvas, GLFWwindow* window = nullptr)
    {
        textBox_ = &textBox;
        canvas_ = &canvas;
        window_ = window;
    }

    /*! @brief Forwards a GLFW character callback event.
     *  @param[in] codepoint Unicode code point received from GLFW.
     */
    void handleChar(unsigned int codepoint)
    {
        if (textBox_)
            textBox_->handleCharInput(codepoint);
    }

    /*! @brief Forwards a GLFW key callback event.
     *
     *  Native key codes are converted into @ref TextBoxKey actions.  When
     *  Control is held, common clipboard shortcuts are handled automatically.
     *
     *  @param[in] key    GLFW key code.
     *  @param[in] action GLFW action (`GLFW_PRESS`, `GLFW_REPEAT`, etc.).
     *  @param[in] mods   GLFW modifier flags.
     */
    void handleKey(int key, int action, int mods = 0)
    {
        if (!textBox_ || (action != GLFW_PRESS && action != GLFW_REPEAT))
            return;

        if ((mods & GLFW_MOD_CONTROL) != 0)
        {
            handleClipboardShortcut(key);
            return;
        }

        TextBoxKey mappedKey{};
        if (mapKey(key, mappedKey))
            textBox_->handleKey(mappedKey, (mods & GLFW_MOD_SHIFT) != 0);
    }

    /*! @brief Forwards a GLFW cursor-position callback event.
     *
     *  The current cursor position is stored even when no drag is active so a
     *  subsequent button event has an up-to-date hit point.
     *
     *  @param[in] x Cursor X position in window content coordinates.
     *  @param[in] y Cursor Y position in window content coordinates.
     */
    void handleCursorPos(double x, double y)
    {
        cursorX_ = static_cast<float>(x);
        cursorY_ = static_cast<float>(y);

        // Drag selection lives here so the core TextBox stays framework-agnostic.
        if (mouseDown_ && textBox_ && canvas_)
            textBox_->handlePointerDrag(cursorX_, cursorY_, *canvas_);
    }

    /*! @brief Forwards a GLFW mouse-button callback event.
     *
     *  Only the left mouse button is used.  Press events start hit-testing and
     *  release events end drag selection.
     *
     *  @param[in] button GLFW mouse button.
     *  @param[in] action GLFW action (`GLFW_PRESS` or `GLFW_RELEASE`).
     */
    void handleMouseButton(int button, int action)
    {
        if (!textBox_ || !canvas_)
            return;

        if (button != GLFW_MOUSE_BUTTON_LEFT)
            return;

        if (action == GLFW_PRESS)
        {
            mouseDown_ = true;
            textBox_->handlePointerDown(cursorX_, cursorY_, *canvas_);
        }
        else if (action == GLFW_RELEASE)
        {
            mouseDown_ = false;
        }
    }

    /*! @brief Returns the last cursor X position seen by the controller. */
    [[nodiscard]] float cursorX() const
    {
        return cursorX_;
    }

    /*! @brief Returns the last cursor Y position seen by the controller. */
    [[nodiscard]] float cursorY() const
    {
        return cursorY_;
    }

private:
    void handleClipboardShortcut(int key)
    {
        if (!textBox_)
            return;

        // Ctrl+A does not require a native clipboard handle, unlike copy/cut/paste.
        if (key == GLFW_KEY_A)
        {
            textBox_->selectAll();
            return;
        }

        if (!window_)
            return;

        if (key == GLFW_KEY_C)
        {
            std::string clipboardText;
            if (textBox_->copySelection(clipboardText))
                glfwSetClipboardString(window_, clipboardText.c_str());
        }
        else if (key == GLFW_KEY_X)
        {
            std::string clipboardText;
            if (textBox_->cutSelection(clipboardText))
                glfwSetClipboardString(window_, clipboardText.c_str());
        }
        else if (key == GLFW_KEY_V)
        {
            const char* clipboardText = glfwGetClipboardString(window_);
            if (clipboardText != nullptr)
                textBox_->pasteText(clipboardText);
        }
    }

    static bool mapKey(int key, TextBoxKey& mappedKey)
    {
        switch (key)
        {
        case GLFW_KEY_BACKSPACE:
            mappedKey = TextBoxKey::Backspace;
            return true;
        case GLFW_KEY_DELETE:
            mappedKey = TextBoxKey::Delete;
            return true;
        case GLFW_KEY_LEFT:
            mappedKey = TextBoxKey::Left;
            return true;
        case GLFW_KEY_RIGHT:
            mappedKey = TextBoxKey::Right;
            return true;
        case GLFW_KEY_HOME:
            mappedKey = TextBoxKey::Home;
            return true;
        case GLFW_KEY_END:
            mappedKey = TextBoxKey::End;
            return true;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER:
            mappedKey = TextBoxKey::Enter;
            return true;
        case GLFW_KEY_ESCAPE:
            mappedKey = TextBoxKey::Escape;
            return true;
        default:
            return false;
        }
    }

    TextBox* textBox_ = nullptr;
    Canvas* canvas_ = nullptr;
    GLFWwindow* window_ = nullptr;
    float cursorX_ = 0.0f;
    float cursorY_ = 0.0f;
    bool mouseDown_ = false;
};

} // namespace vectorgl
