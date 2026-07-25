#include "vectorgl/text_box.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace vectorgl
{

void TextBox::setBounds(float x, float y, float width, float height)
{
    x_ = x;
    y_ = y;
    width_ = width;
    height_ = height;
}

void TextBox::setFont(const std::string& fontPath, float fontSize)
{
    style_.fontPath = fontPath;
    style_.fontSize = fontSize;
}

void TextBox::setText(const std::string& text)
{
    text_ = text;
    caretIndex_ = text_.size();
    selectionAnchor_ = caretIndex_;
    viewStart_ = 0;
    resetBlink();
}

void TextBox::setPlaceholder(const std::string& placeholder)
{
    placeholder_ = placeholder;
}

void TextBox::setFocused(bool focused)
{
    focused_ = focused;
    resetBlink();
}

const std::string& TextBox::text() const
{
    return text_;
}

const std::string& TextBox::placeholder() const
{
    return placeholder_;
}

bool TextBox::focused() const
{
    return focused_;
}

bool TextBox::hitTest(float x, float y) const
{
    return x >= x_ && x <= x_ + width_ && y >= y_ && y <= y_ + height_;
}

bool TextBox::hasSelection() const
{
    // The anchor stays fixed at the point where selection started while the caret moves.
    return selectionAnchor_ != caretIndex_;
}

std::size_t TextBox::selectionStart() const
{
    return std::min(selectionAnchor_, caretIndex_);
}

std::size_t TextBox::selectionEnd() const
{
    return std::max(selectionAnchor_, caretIndex_);
}

std::string TextBox::selectedText() const
{
    if (!hasSelection())
        return {};

    return text_.substr(selectionStart(), selectionEnd() - selectionStart());
}

TextBoxStyle& TextBox::style()
{
    return style_;
}

const TextBoxStyle& TextBox::style() const
{
    return style_;
}

bool TextBox::handlePointerDown(float x, float y, Canvas& canvas)
{
    const bool inside = hitTest(x, y);
    focused_ = inside;
    resetBlink();
    if (!inside)
    {
        clearSelection();
        return false;
    }

    if (!ensureFont(canvas))
        return true;

    const float availableWidth = std::max(0.0f, width_ - (style_.paddingX * 2.0f));
    ensureCaretVisible(canvas, availableWidth);
    const float localX = std::max(0.0f, x - (x_ + style_.paddingX));
    const std::size_t hitIndex = caretIndexFromPosition(canvas, localX);
    caretIndex_ = hitIndex;
    selectionAnchor_ = hitIndex;
    submitted_ = false;
    return true;
}

bool TextBox::handlePointerDrag(float x, float /* y */, Canvas& canvas)
{
    if (!focused_ || !ensureFont(canvas))
        return false;

    // Clamp drag updates to the inner text area so selection can extend to the box edges.
    const float clampedX = std::clamp(x, x_ + style_.paddingX, x_ + width_ - style_.paddingX);
    const float localX = std::max(0.0f, clampedX - (x_ + style_.paddingX));
    caretIndex_ = caretIndexFromPosition(canvas, localX);
    submitted_ = false;
    resetBlink();
    return true;
}

bool TextBox::handleCharInput(uint32_t codepoint)
{
    if (!focused_ || !supportsCodepoint(codepoint))
        return false;

    deleteSelection();
    text_.insert(text_.begin() + static_cast<std::ptrdiff_t>(caretIndex_), static_cast<char>(codepoint));
    ++caretIndex_;
    selectionAnchor_ = caretIndex_;
    submitted_ = false;
    resetBlink();
    return true;
}

bool TextBox::handleKey(TextBoxKey key, bool extendSelection)
{
    if (!focused_)
        return false;

    submitted_ = false;
    switch (key)
    {
    case TextBoxKey::Backspace:
        if (deleteSelection())
        {
            resetBlink();
            return true;
        }
        if (caretIndex_ == 0 || text_.empty())
            return false;
        text_.erase(text_.begin() + static_cast<std::ptrdiff_t>(caretIndex_ - 1));
        --caretIndex_;
        selectionAnchor_ = caretIndex_;
        resetBlink();
        return true;
    case TextBoxKey::Delete:
        if (deleteSelection())
        {
            resetBlink();
            return true;
        }
        if (caretIndex_ >= text_.size())
            return false;
        text_.erase(text_.begin() + static_cast<std::ptrdiff_t>(caretIndex_));
        selectionAnchor_ = caretIndex_;
        resetBlink();
        return true;
    case TextBoxKey::Left:
        if (!extendSelection && hasSelection())
        {
            moveCaretTo(selectionStart(), false);
            resetBlink();
            return true;
        }
        if (caretIndex_ == 0)
            return false;
        moveCaretTo(caretIndex_ - 1, extendSelection);
        resetBlink();
        return true;
    case TextBoxKey::Right:
        if (!extendSelection && hasSelection())
        {
            moveCaretTo(selectionEnd(), false);
            resetBlink();
            return true;
        }
        if (caretIndex_ >= text_.size())
            return false;
        moveCaretTo(caretIndex_ + 1, extendSelection);
        resetBlink();
        return true;
    case TextBoxKey::Home:
        if (caretIndex_ == 0 && (!extendSelection || !hasSelection()))
            return false;
        moveCaretTo(0, extendSelection);
        resetBlink();
        return true;
    case TextBoxKey::End:
        if (caretIndex_ == text_.size() && (!extendSelection || !hasSelection()))
            return false;
        moveCaretTo(text_.size(), extendSelection);
        resetBlink();
        return true;
    case TextBoxKey::Enter:
        submitted_ = true;
        resetBlink();
        return true;
    case TextBoxKey::Escape:
        focused_ = false;
        resetBlink();
        return true;
    }

    return false;
}

bool TextBox::consumeSubmit()
{
    const bool submitted = submitted_;
    submitted_ = false;
    return submitted;
}

bool TextBox::pasteText(const std::string& text)
{
    if (!focused_)
        return false;

    std::string filtered;
    filtered.reserve(text.size());
    for (unsigned char c : text)
    {
        if (supportsCodepoint(static_cast<uint32_t>(c)))
            filtered.push_back(static_cast<char>(c));
    }

    if (filtered.empty() && !hasSelection())
        return false;

    deleteSelection();
    text_.insert(caretIndex_, filtered);
    caretIndex_ += filtered.size();
    selectionAnchor_ = caretIndex_;
    submitted_ = false;
    resetBlink();
    return true;
}

bool TextBox::copySelection(std::string& outText) const
{
    if (!hasSelection())
        return false;

    outText = selectedText();
    return true;
}

bool TextBox::cutSelection(std::string& outText)
{
    if (!copySelection(outText))
        return false;

    deleteSelection();
    resetBlink();
    return true;
}

void TextBox::selectAll()
{
    selectionAnchor_ = 0;
    caretIndex_ = text_.size();
    resetBlink();
}

void TextBox::clearSelection()
{
    selectionAnchor_ = caretIndex_;
}

bool TextBox::render(Canvas& canvas, float dt)
{
    if (!ensureFont(canvas))
        return false;

    // The widget scrolls horizontally by changing viewStart_ when the full string no longer fits.
    const float availableWidth = std::max(0.0f, width_ - (style_.paddingX * 2.0f));
    ensureCaretVisible(canvas, availableWidth);

    const std::string visible = visibleText(canvas, availableWidth);
    const std::size_t visibleStart = viewStart_;
    const std::size_t visibleEnd = viewStart_ + visible.size();
    const std::size_t visibleSelectionStart = std::max(selectionStart(), visibleStart);
    const std::size_t visibleSelectionEnd = std::min(selectionEnd(), visibleEnd);
    const std::size_t relativeCaret = caretIndex_ >= viewStart_ ? caretIndex_ - viewStart_ : 0;
    const float caretOffset = canvas.measureText(visible.substr(0, std::min(relativeCaret, visible.size())));
    const float textBaselineY = y_ + ((height_ - canvas.lineHeight()) * 0.5f);

    canvas.setFillColor(style_.backgroundColor);
    canvas.fillRoundedRect(x_, y_, width_, height_, style_.cornerRadius);

    canvas.setStrokeColor(focused_ ? style_.focusedBorderColor : style_.borderColor);
    canvas.setLineWidth(focused_ ? style_.focusedBorderWidth : style_.borderWidth);
    canvas.strokeRoundedRect(x_, y_, width_, height_, style_.cornerRadius);

    if (visibleSelectionStart < visibleSelectionEnd)
    {
        const std::size_t selectionOffsetStart = visibleSelectionStart - visibleStart;
        const std::size_t selectionOffsetEnd = visibleSelectionEnd - visibleStart;
        const float selectionX = x_ + style_.paddingX + canvas.measureText(visible.substr(0, selectionOffsetStart));
        const float selectionWidth =
            canvas.measureText(visible.substr(selectionOffsetStart, selectionOffsetEnd - selectionOffsetStart));
        canvas.setFillColor(style_.selectionColor);
        canvas.fillRoundedRect(selectionX - 1.0f, y_ + style_.paddingY, selectionWidth + 2.0f,
                               height_ - (style_.paddingY * 2.0f), 4.0f);
    }

    if (!visible.empty())
    {
        canvas.setFillColor(style_.textColor);
        canvas.fillText(visible, x_ + style_.paddingX, textBaselineY);
    }
    else if (!placeholder_.empty() && text_.empty())
    {
        canvas.setFillColor(style_.placeholderColor);
        canvas.fillText(placeholder_, x_ + style_.paddingX, textBaselineY);
    }

    if (focused_)
    {
        blinkTime_ += dt;
        if (std::fmod(blinkTime_, 1.0f) < 0.5f)
        {
            canvas.setFillColor(style_.caretColor);
            canvas.fillRect(x_ + style_.paddingX + caretOffset, y_ + style_.paddingY, style_.caretWidth,
                            height_ - (style_.paddingY * 2.0f));
        }
    }
    else
    {
        blinkTime_ = 0.0f;
    }

    return true;
}

bool TextBox::ensureFont(Canvas& canvas) const
{
    return !style_.fontPath.empty() && canvas.setFont(style_.fontPath, style_.fontSize);
}

bool TextBox::supportsCodepoint(uint32_t codepoint) const
{
    return codepoint >= 32 && codepoint < 127;
}

std::string TextBox::visibleText(const Canvas& canvas, float availableWidth) const
{
    if (viewStart_ >= text_.size())
        return {};

    // Build the visible span incrementally until the next character would overflow.
    std::string visible;
    for (std::size_t i = viewStart_; i < text_.size(); ++i)
    {
        std::string candidate = visible;
        candidate.push_back(text_[i]);
        if (!visible.empty() && canvas.measureText(candidate) > availableWidth)
            break;
        visible = std::move(candidate);
    }

    return visible;
}

std::size_t TextBox::caretIndexFromPosition(const Canvas& canvas, float localX) const
{
    if (viewStart_ >= text_.size())
        return text_.size();

    // Snap to the nearest character boundary in the currently visible slice.
    float bestDistance = std::numeric_limits<float>::max();
    std::size_t bestIndex = viewStart_;
    for (std::size_t index = viewStart_; index <= text_.size(); ++index)
    {
        const std::string span = text_.substr(viewStart_, index - viewStart_);
        const float candidateX = canvas.measureText(span);
        const float distance = std::abs(candidateX - localX);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    return bestIndex;
}

void TextBox::moveCaretTo(std::size_t index, bool extendSelection)
{
    caretIndex_ = std::min(index, text_.size());
    if (!extendSelection)
        selectionAnchor_ = caretIndex_;
}

void TextBox::ensureCaretVisible(const Canvas& canvas, float availableWidth)
{
    caretIndex_ = std::min(caretIndex_, text_.size());
    viewStart_ = std::min(viewStart_, caretIndex_);

    while (viewStart_ < caretIndex_)
    {
        const std::string span = text_.substr(viewStart_, caretIndex_ - viewStart_);
        if (canvas.measureText(span) <= availableWidth)
            break;
        ++viewStart_;
    }

    while (viewStart_ > 0)
    {
        const std::size_t newStart = viewStart_ - 1;
        const std::string span = text_.substr(newStart, caretIndex_ - newStart);
        if (canvas.measureText(span) > availableWidth)
            break;
        viewStart_ = newStart;
    }
}

bool TextBox::deleteSelection()
{
    if (!hasSelection())
        return false;

    const std::size_t start = selectionStart();
    const std::size_t end = selectionEnd();
    text_.erase(start, end - start);
    caretIndex_ = start;
    selectionAnchor_ = start;
    return true;
}

void TextBox::resetBlink()
{
    blinkTime_ = 0.0f;
}

} // namespace vectorgl
