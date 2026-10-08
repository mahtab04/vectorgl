#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "vectorgl/detail/utf8.hpp"
#include "vectorgl/font.hpp"

namespace vectorgl
{
namespace
{
struct Token
{
    uint32_t cp;
    size_t start, end;
};
bool breakSpace(uint32_t cp)
{
    return cp == ' ' || cp == '\t' || (cp >= 0x2000 && cp <= 0x200A && cp != 0x2007) || cp == 0x3000;
}
} // namespace

TextLayout Font::layoutText(std::string_view text, const TextLayoutOptions& options) const
{
    if (!std::isfinite(options.maxWidth) || !(options.lineSpacing > 0) || !std::isfinite(options.lineSpacing))
        throw std::invalid_argument("Text layout width must be finite and line spacing must be finite and positive");
    TextLayout result;
    if (lineHeight() <= 0)
        return result;
    std::vector<Token> tokens;
    tokens.reserve(text.size());
    for (size_t offset = 0; offset < text.size();)
    {
        const size_t start = offset;
        auto cp = detail::nextCodepoint(text, offset);
        if (cp == '\r')
        {
            if (offset < text.size() && text[offset] == '\n')
                ++offset;
            cp = '\n';
        }
        if (cp == 0x2028 || cp == 0x2029)
            cp = '\n';
        tokens.push_back({cp, start, offset});
    }
    const float step = lineHeight() * options.lineSpacing;
    if (!(step > 0) || !std::isfinite(step))
        throw std::invalid_argument("Text line spacing is outside the supported range");
    auto tokenAdvance = [&](uint32_t cp) { return cp == '\t' ? advance(' ') * 4 : cp < 32 ? 0 : advance(cp); };
    auto pairKern = [&](uint32_t left, uint32_t right)
    { return left == '\t' || right == '\t' ? 0 : kerning(left, right); };
    auto addLine = [&](size_t begin, size_t end)
    {
        TextLine line;
        line.byteStart = begin < tokens.size() ? tokens[begin].start : text.size();
        line.byteEnd = end > begin ? tokens[end - 1].end : line.byteStart;
        line.y = result.lines.size() * step;
        line.carets.push_back({line.byteStart, 0});
        uint32_t previous = 0;
        for (size_t i = begin; i < end; ++i)
        {
            const auto& token = tokens[i];
            line.width += pairKern(previous, token.cp);
            // Shared boundaries include kerning toward the following glyph.
            line.carets.back().x = line.width;
            if (token.cp >= 32)
                result.glyphs.push_back({token.cp, line.width, line.y});
            line.width += tokenAdvance(token.cp);
            line.carets.push_back({token.end, line.width});
            previous = token.cp;
        }
        result.width = std::max(result.width, line.width);
        result.lines.push_back(std::move(line));
    };
    const bool wrapping = options.maxWidth > 0 && options.wrap != TextWrap::None;
    size_t start = 0;
    while (true)
    {
        size_t paragraphEnd = start;
        while (paragraphEnd < tokens.size() && tokens[paragraphEnd].cp != '\n')
            ++paragraphEnd;
        if (start == paragraphEnd)
            addLine(start, start);
        while (start < paragraphEnd)
        {
            float width = 0;
            uint32_t previous = 0;
            size_t end = start, lastBreak = start;
            for (; end < paragraphEnd; ++end)
            {
                const auto cp = tokens[end].cp;
                const float next = width + pairKern(previous, cp) + tokenAdvance(cp);
                if (wrapping && next > options.maxWidth && end > start)
                    break;
                width = next;
                previous = cp;
                if (breakSpace(cp))
                    lastBreak = end + 1;
            }
            size_t nextStart = end;
            if (end < paragraphEnd && options.wrap == TextWrap::Word)
            {
                if (lastBreak > start)
                    end = nextStart = lastBreak;
                while (end > start && breakSpace(tokens[end - 1].cp))
                    --end;
                while (nextStart < paragraphEnd && breakSpace(tokens[nextStart].cp))
                    ++nextStart;
            }
            // A single oversized codepoint occupies a line of its own.
            addLine(start, end);
            start = nextStart;
        }
        if (paragraphEnd == tokens.size())
            break;
        start = paragraphEnd + 1;
    }
    result.height = lineHeight() + (result.lines.size() - 1) * step;
    const float alignmentWidth = options.maxWidth > 0 ? options.maxWidth : result.width;
    size_t glyphIndex = 0;
    for (auto& line : result.lines)
    {
        const float remaining = alignmentWidth - line.width;
        line.x = options.align == TextAlign::Center  ? remaining * 0.5f
                 : options.align == TextAlign::Right ? remaining
                                                     : 0;
        for (auto& caret : line.carets)
            caret.x += line.x;
        while (glyphIndex < result.glyphs.size() && result.glyphs[glyphIndex].y == line.y)
            result.glyphs[glyphIndex++].x += line.x;
    }
    return result;
}
} // namespace vectorgl
