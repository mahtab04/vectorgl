#pragma once

#include <string>

#include "vectorgl/canvas.hpp"
#include "vectorgl/color.hpp"
#include "vectorgl/text_box.hpp"

namespace vectorgl::ui
{

/*! @brief Alias for the core single-line textbox widget. */
using TextBox = vectorgl::TextBox;
/*! @brief Alias for logical textbox key actions. */
using TextBoxKey = vectorgl::TextBoxKey;
/*! @brief Alias for textbox visual configuration. */
using TextBoxStyle = vectorgl::TextBoxStyle;

/*! @brief Visual configuration for @ref Label. */
struct LabelStyle
{
    /*! Filesystem path to the font used for the label. */
    std::string fontPath;
    /*! Font size in pixels. */
    float fontSize = 18.0f;
    /*! Label text color. */
    Color textColor = Color::White;
};

/*! @brief Lightweight static text element.
 *
 *  Label is a convenience wrapper around @ref Canvas::fillText that stores a
 *  position, text string, and font styling.  It does not manage layout.
 */
class Label
{
public:
    /*! @brief Sets the label anchor position.
     *  @param[in] x Left edge of the text baseline.
     *  @param[in] y Baseline Y position.
     */
    void setPosition(float x, float y)
    {
        x_ = x;
        y_ = y;
    }

    /*! @brief Sets the label text.
     *  @param[in] text UTF-8 string to render; unsupported glyphs use `?`.
     */
    void setText(const std::string& text)
    {
        text_ = text;
    }

    /*! @brief Sets the font used to render the label.
     *  @param[in] fontPath Filesystem path to a `.ttf` font file.
     *  @param[in] fontSize Font size in pixels.
     */
    void setFont(const std::string& fontPath, float fontSize)
    {
        style_.fontPath = fontPath;
        style_.fontSize = fontSize;
    }

    /*! @brief Returns mutable style settings. */
    LabelStyle& style()
    {
        return style_;
    }

    /*! @brief Returns read-only style settings. */
    const LabelStyle& style() const
    {
        return style_;
    }

    /*! @brief Draws the label.
     *  @param[in] canvas Canvas used for font selection and rendering.
     *  @return `true` if a valid font was configured and the text was drawn.
     */
    bool render(Canvas& canvas) const
    {
        if (style_.fontPath.empty() || !canvas.setFont(style_.fontPath, style_.fontSize))
            return false;

        canvas.setFillColor(style_.textColor);
        canvas.fillText(text_, x_, y_);
        return true;
    }

private:
    float x_ = 0.0f;
    float y_ = 0.0f;
    std::string text_;
    LabelStyle style_{};
};

/*! @brief Visual configuration for @ref Button. */
struct ButtonStyle
{
    /*! Filesystem path to the font used for the label text. */
    std::string fontPath;
    /*! Font size in pixels. */
    float fontSize = 18.0f;
    /*! Corner radius for the button background. */
    float cornerRadius = 12.0f;
    /*! Fill color in the idle state. */
    Color fillColor = Color{0.17f, 0.24f, 0.34f, 1.0f};
    /*! Fill color while hovered. */
    Color hoverFillColor = Color{0.21f, 0.30f, 0.42f, 1.0f};
    /*! Fill color while pressed. */
    Color pressedFillColor = Color{0.13f, 0.20f, 0.29f, 1.0f};
    /*! Outline color. */
    Color borderColor = Color{0.31f, 0.45f, 0.64f, 1.0f};
    /*! Label text color. */
    Color textColor = Color::White;
};

/*! @brief Simple clickable button widget.
 *
 *  Button stores its own bounds and hover / pressed state but does not own any
 *  callback mechanism.  Applications are expected to call
 *  @ref handlePointerDown and @ref handlePointerUp from their event loop and
 *  act on the returned click result.
 */
class Button
{
public:
    /*! @brief Sets the button rectangle in canvas coordinates.
     *  @param[in] x      Left edge.
     *  @param[in] y      Top edge.
     *  @param[in] width  Width in pixels.
     *  @param[in] height Height in pixels.
     */
    void setBounds(float x, float y, float width, float height)
    {
        x_ = x;
        y_ = y;
        width_ = width;
        height_ = height;
    }

    /*! @brief Sets the button label text.
     *  @param[in] text UTF-8 string to display; unsupported glyphs use `?`.
     */
    void setText(const std::string& text)
    {
        text_ = text;
    }

    /*! @brief Sets the font used to render the button label.
     *  @param[in] fontPath Filesystem path to a `.ttf` font file.
     *  @param[in] fontSize Font size in pixels.
     */
    void setFont(const std::string& fontPath, float fontSize)
    {
        style_.fontPath = fontPath;
        style_.fontSize = fontSize;
    }

    /*! @brief Returns whether a point lies inside the button bounds.
     *  @param[in] x X coordinate in canvas space.
     *  @param[in] y Y coordinate in canvas space.
     */
    [[nodiscard]] bool hitTest(float x, float y) const
    {
        return x >= x_ && x <= x_ + width_ && y >= y_ && y <= y_ + height_;
    }

    /*! @brief Updates hover state.
     *  @param[in] hovered `true` when the pointer is above the button.
     */
    void setHovered(bool hovered)
    {
        hovered_ = hovered;
    }

    /*! @brief Handles a pointer press.
     *  @param[in] x Pointer X position in canvas space.
     *  @param[in] y Pointer Y position in canvas space.
     *  @return `true` if the press started inside the button.
     */
    bool handlePointerDown(float x, float y)
    {
        pressed_ = hitTest(x, y);
        hovered_ = pressed_;
        return pressed_;
    }

    /*! @brief Handles a pointer release.
     *
     *  A click is reported only if the press began inside the button and the
     *  release also ends inside the button.
     *
     *  @param[in] x Pointer X position in canvas space.
     *  @param[in] y Pointer Y position in canvas space.
     *  @return `true` if the button was clicked.
     */
    bool handlePointerUp(float x, float y)
    {
        const bool clicked = pressed_ && hitTest(x, y);
        pressed_ = false;
        hovered_ = hitTest(x, y);
        return clicked;
    }

    /*! @brief Returns mutable style settings. */
    ButtonStyle& style()
    {
        return style_;
    }

    /*! @brief Returns read-only style settings. */
    const ButtonStyle& style() const
    {
        return style_;
    }

    /*! @brief Draws the button.
     *  @param[in] canvas Canvas used for geometry and text rendering.
     *  @return Always returns `true` after drawing the button background.  Text
     *          is skipped if no valid font is configured.
     */
    bool render(Canvas& canvas) const
    {
        const Color fill = pressed_ ? style_.pressedFillColor : (hovered_ ? style_.hoverFillColor : style_.fillColor);
        canvas.setFillColor(fill);
        canvas.fillRoundedRect(x_, y_, width_, height_, style_.cornerRadius);
        canvas.setStrokeColor(style_.borderColor);
        canvas.setLineWidth(1.5f);
        canvas.strokeRoundedRect(x_, y_, width_, height_, style_.cornerRadius);

        if (!style_.fontPath.empty() && canvas.setFont(style_.fontPath, style_.fontSize))
        {
            // Center text using the current font metrics so different labels still align consistently.
            const float textWidth = canvas.measureText(text_);
            const float textX = x_ + (width_ - textWidth) * 0.5f;
            const float textY = y_ + (height_ - canvas.lineHeight()) * 0.5f;
            canvas.setFillColor(style_.textColor);
            canvas.fillText(text_, textX, textY);
        }
        return true;
    }

private:
    float x_ = 0.0f;
    float y_ = 0.0f;
    float width_ = 0.0f;
    float height_ = 0.0f;
    std::string text_;
    bool hovered_ = false;
    bool pressed_ = false;
    ButtonStyle style_{};
};

} // namespace vectorgl::ui
