#pragma once

#include <cstdint>
#include <string>

#include "vectorgl/canvas.hpp"
#include "vectorgl/color.hpp"

namespace vectorgl
{

/*! @brief Logical editing actions understood by @ref TextBox.
 *
 *  These are semantic keys rather than platform key codes.  Platform adapters
 *  such as @ref GlfwTextBoxController translate native keyboard input into
 *  this enum before forwarding it to the widget.
 */
enum class TextBoxKey
{
    Backspace,
    Delete,
    Left,
    Right,
    Home,
    End,
    Enter,
    Escape,
};

/*! @brief Visual configuration for @ref TextBox.
 *
 *  The defaults are suitable for a dark UI.  Most applications will override
 *  at least the font path and font size.
 */
struct TextBoxStyle
{
    /*! Filesystem path to the TrueType font used for rendering text. */
    std::string fontPath;
    /*! Font size in pixels. */
    float fontSize = 24.0f;
    /*! Corner radius for the outer box. */
    float cornerRadius = 14.0f;
    /*! Horizontal padding between the border and text content. */
    float paddingX = 14.0f;
    /*! Vertical padding between the border and text content. */
    float paddingY = 12.0f;
    /*! Border width when the textbox is not focused. */
    float borderWidth = 1.5f;
    /*! Border width when the textbox is focused. */
    float focusedBorderWidth = 2.5f;
    /*! Width of the blinking caret in pixels. */
    float caretWidth = 2.0f;
    /*! Background fill color. */
    Color backgroundColor = Color{0.09f, 0.12f, 0.18f, 0.98f};
    /*! Border color when the textbox is not focused. */
    Color borderColor = Color{0.28f, 0.38f, 0.58f, 1.0f};
    /*! Border color when the textbox is focused. */
    Color focusedBorderColor = Color{0.34f, 0.78f, 1.0f, 1.0f};
    /*! Fill color used behind selected text. */
    Color selectionColor = Color{0.22f, 0.48f, 0.82f, 0.85f};
    /*! Text color for normal content. */
    Color textColor = Color{0.82f, 0.88f, 0.96f, 1.0f};
    /*! Reserved color for selected text. */
    Color selectedTextColor = Color{1.0f, 1.0f, 1.0f, 1.0f};
    /*! Text color for placeholder content shown when the value is empty. */
    Color placeholderColor = Color{0.56f, 0.64f, 0.76f, 1.0f};
    /*! Caret color while focused. */
    Color caretColor = Color{0.38f, 0.86f, 1.0f, 1.0f};
};

/*! @brief Single-line editable text input widget.
 *
 *  TextBox owns the editing state for a single-line text field: caret
 *  position, selection, focus, scrolling, and submit state.  It is framework
 *  agnostic and can be driven either directly or through a platform adapter
 *  like @ref GlfwTextBoxController.
 *
 *  The widget currently accepts printable ASCII input and renders itself using
 *  the active @ref Canvas font API.
 *
 *  Typical usage:
 *  @code
 *  vectorgl::TextBox field;
 *  field.setBounds(40.0f, 40.0f, 320.0f, 44.0f);
 *  field.setFont("C:/Windows/Fonts/segoeui.ttf", 20.0f);
 *  field.setPlaceholder("Search");
 *
 *  // During input callbacks:
 *  field.handlePointerDown(mouseX, mouseY, canvas);
 *  field.handleCharInput(codepoint);
 *  field.handleKey(vectorgl::TextBoxKey::Backspace);
 *
 *  // Each frame:
 *  field.render(canvas, dt);
 *  @endcode
 */
class TextBox
{
public:
    TextBox() = default;

    /*! @brief Sets the widget rectangle in canvas coordinates.
     *  @param[in] x      Left edge.
     *  @param[in] y      Top edge.
     *  @param[in] width  Width in pixels.
     *  @param[in] height Height in pixels.
     */
    void setBounds(float x, float y, float width, float height);

    /*! @brief Sets the font used to render text, placeholder, and caret layout.
     *  @param[in] fontPath Filesystem path to a `.ttf` font file.
     *  @param[in] fontSize Font size in pixels.
     */
    void setFont(const std::string& fontPath, float fontSize);

    /*! @brief Replaces the current text value.
     *
     *  The caret moves to the end of the new string and any current selection
     *  is cleared.
     *
     *  @param[in] text New textbox contents.
     */
    void setText(const std::string& text);

    /*! @brief Sets the placeholder text shown when the value is empty.
     *  @param[in] placeholder Placeholder string.
     */
    void setPlaceholder(const std::string& placeholder);

    /*! @brief Explicitly changes focus state.
     *
     *  Focus controls whether keyboard input is accepted and whether the caret
     *  is rendered.
     *
     *  @param[in] focused `true` to focus the field.
     */
    void setFocused(bool focused);

    /*! @brief Returns the current text value. */
    [[nodiscard]] const std::string& text() const;

    /*! @brief Returns the configured placeholder string. */
    [[nodiscard]] const std::string& placeholder() const;

    /*! @brief Returns whether the textbox currently has focus. */
    [[nodiscard]] bool focused() const;

    /*! @brief Tests whether a point lies inside the textbox bounds.
     *  @param[in] x X coordinate in canvas space.
     *  @param[in] y Y coordinate in canvas space.
     *  @return `true` if the point is inside the widget rectangle.
     */
    [[nodiscard]] bool hitTest(float x, float y) const;

    /*! @brief Returns whether a text range is currently selected. */
    [[nodiscard]] bool hasSelection() const;

    /*! @brief Returns the first selected character index. */
    [[nodiscard]] std::size_t selectionStart() const;

    /*! @brief Returns the index one past the last selected character. */
    [[nodiscard]] std::size_t selectionEnd() const;

    /*! @brief Returns a copy of the currently selected substring.
     *  @return Empty string if nothing is selected.
     */
    [[nodiscard]] std::string selectedText() const;

    /*! @brief Returns mutable style settings.
     *
     *  This can be used to customize colors, padding, border widths, and other
     *  presentation details after construction.
     */
    TextBoxStyle& style();

    /*! @brief Returns read-only style settings. */
    const TextBoxStyle& style() const;

    /*! @brief Handles a pointer press.
     *
     *  Clicking inside the widget focuses it and places the caret near the hit
     *  position.  Clicking outside clears focus and selection.
     *
     *  @param[in] x      Pointer X position in canvas space.
     *  @param[in] y      Pointer Y position in canvas space.
     *  @param[in] canvas Canvas used for glyph measurement.
     *  @return `true` if the event was consumed.
     */
    bool handlePointerDown(float x, float y, Canvas& canvas);

    /*! @brief Extends the current selection using a pointer drag.
     *
     *  This is only effective while the textbox is focused.
     *
     *  @param[in] x      Pointer X position in canvas space.
     *  @param[in] y      Pointer Y position in canvas space.
     *  @param[in] canvas Canvas used for glyph measurement.
     *  @return `true` if the drag updated the caret or selection.
     */
    bool handlePointerDrag(float x, float y, Canvas& canvas);

    /*! @brief Inserts a typed character at the caret.
     *
     *  If a selection exists, it is replaced by the new character.
     *
     *  @param[in] codepoint Unicode code point from a character input event.
     *  @return `true` if the character was accepted.
     */
    bool handleCharInput(uint32_t codepoint);

    /*! @brief Handles a logical key action.
     *
     *  Arrow keys move the caret, Backspace/Delete remove content, Enter sets a
     *  submit flag, and Escape clears focus.
     *
     *  @param[in] key             Logical key action.
     *  @param[in] extendSelection If `true`, caret motion extends selection
     *                             instead of collapsing it.
     *  @return `true` if the key changed widget state.
     */
    bool handleKey(TextBoxKey key, bool extendSelection = false);

    /*! @brief Inserts text as if it came from the clipboard.
     *
     *  Unsupported characters are filtered out.  If a selection exists, it is
     *  replaced.
     *
     *  @param[in] text Text to insert.
     *  @return `true` if the value changed.
     */
    bool pasteText(const std::string& text);

    /*! @brief Copies the current selection into an output string.
     *  @param[out] outText Receives the selected substring on success.
     *  @return `true` if a selection exists.
     */
    bool copySelection(std::string& outText) const;

    /*! @brief Copies and removes the current selection.
     *  @param[out] outText Receives the selected substring on success.
     *  @return `true` if a selection existed and was removed.
     */
    bool cutSelection(std::string& outText);

    /*! @brief Selects the entire text value. */
    void selectAll();

    /*! @brief Collapses any selection to the current caret position. */
    void clearSelection();

    /*! @brief Returns and clears the submit flag set by Enter.
     *
     *  This is intended for polling once per frame after forwarding input.
     *
     *  @return `true` exactly once after Enter is pressed while focused.
     */
    bool consumeSubmit();

    /*! @brief Draws the textbox.
     *
     *  Call once per frame between @ref Canvas::beginFrame and
     *  @ref Canvas::endFrame.
     *
     *  @param[in] canvas Canvas used for measurement and rendering.
     *  @param[in] dt     Frame delta time in seconds, used for caret blinking.
     *  @return `true` if a font was available and rendering succeeded.
     */
    bool render(Canvas& canvas, float dt = 0.0f);

private:
    [[nodiscard]] bool ensureFont(Canvas& canvas) const;
    [[nodiscard]] bool supportsCodepoint(uint32_t codepoint) const;
    [[nodiscard]] std::string visibleText(const Canvas& canvas, float availableWidth) const;
    [[nodiscard]] std::size_t caretIndexFromPosition(const Canvas& canvas, float localX) const;
    void moveCaretTo(std::size_t index, bool extendSelection);
    void ensureCaretVisible(const Canvas& canvas, float availableWidth);
    bool deleteSelection();
    void resetBlink();

    float x_ = 0.0f;
    float y_ = 0.0f;
    float width_ = 0.0f;
    float height_ = 0.0f;
    std::string text_;
    std::string placeholder_;
    std::size_t caretIndex_ = 0;
    std::size_t selectionAnchor_ = 0;
    std::size_t viewStart_ = 0;
    bool focused_ = false;
    bool submitted_ = false;
    float blinkTime_ = 0.0f;
    TextBoxStyle style_{};
};

} // namespace vectorgl
