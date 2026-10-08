#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace vectorgl
{
enum class TextAlign
{
    Left,
    Center,
    Right
};
enum class TextWrap
{
    None,
    Word,
    Character
};
/*! UTF-8 layout settings. Nonpositive maxWidth disables wrapping. Word wrapping
 * uses whitespace breaks, falling back to codepoint breaks for long words.
 * Layout is left-to-right; shaping, bidi and grapheme segmentation are excluded.
 */
struct TextLayoutOptions
{
    float maxWidth = 0;
    TextAlign align = TextAlign::Left;
    TextWrap wrap = TextWrap::Word;
    float lineSpacing = 1;
};
/*! UTF-8 byte boundary and horizontal offset from the layout origin. */
struct TextCaret
{
    std::size_t byteOffset = 0;
    float x = 0;
};
struct TextLine
{
    std::size_t byteStart = 0, byteEnd = 0;
    float width = 0, x = 0, y = 0;
    std::vector<TextCaret> carets;
};
/*! Positioned codepoint; x/y are offsets from the layout's top-left origin. */
struct TextGlyph
{
    uint32_t codepoint = 0;
    float x = 0, y = 0;
};
struct TextLayout
{
    float width = 0, height = 0;
    std::vector<TextLine> lines;
    std::vector<TextGlyph> glyphs;
};
} // namespace vectorgl
