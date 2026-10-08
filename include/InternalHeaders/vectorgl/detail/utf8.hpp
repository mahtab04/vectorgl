#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace vectorgl::detail
{
inline bool isScalar(uint32_t cp)
{
    return cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF);
}
// Invalid sequences consume one byte and yield U+FFFD, ensuring progress.
inline uint32_t nextCodepoint(std::string_view text, size_t& offset)
{
    if (offset >= text.size())
        return 0;
    const auto first = static_cast<uint8_t>(text[offset]);
    if (first < 0x80)
    {
        ++offset;
        return first;
    }
    int count = first >= 0xC2 && first <= 0xDF   ? 2
                : first >= 0xE0 && first <= 0xEF ? 3
                : first >= 0xF0 && first <= 0xF4 ? 4
                                                 : 0;
    uint32_t cp = count ? first & (0x7F >> count) : 0;
    if (count && offset + count <= text.size())
    {
        bool valid = true;
        for (int i = 1; i < count; ++i)
        {
            const auto byte = static_cast<uint8_t>(text[offset + i]);
            valid = valid && (byte & 0xC0) == 0x80;
            cp = (cp << 6) | (byte & 0x3F);
        }
        const uint32_t minimum = count == 2 ? 0x80 : count == 3 ? 0x800 : 0x10000;
        if (valid && cp >= minimum && isScalar(cp))
        {
            offset += count;
            return cp;
        }
    }
    ++offset;
    return 0xFFFD;
}
inline void appendCodepoint(std::string& text, uint32_t cp)
{
    if (!isScalar(cp))
        cp = 0xFFFD;
    if (cp < 0x80)
        text.push_back(static_cast<char>(cp));
    else
    {
        if (cp < 0x800)
            text.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        else if (cp < 0x10000)
        {
            text.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            text.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        }
        else
        {
            text.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            text.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            text.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        }
        text.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}
inline size_t previousBoundary(std::string_view text, size_t offset)
{
    if (!offset)
        return 0;
    --offset;
    while (offset && (static_cast<uint8_t>(text[offset]) & 0xC0) == 0x80)
        --offset;
    return offset;
}
inline size_t nextBoundary(std::string_view text, size_t offset)
{
    nextCodepoint(text, offset);
    return offset;
}
inline bool isSingleLineCodepoint(uint32_t cp)
{
    return isScalar(cp) && cp >= 32 && !(cp >= 0x7F && cp <= 0x9F) && cp != 0x2028 && cp != 0x2029;
}
inline std::string singleLine(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    for (size_t offset = 0; offset < text.size();)
    {
        const uint32_t cp = nextCodepoint(text, offset);
        if (isSingleLineCodepoint(cp))
            appendCodepoint(result, cp);
    }
    return result;
}
} // namespace vectorgl::detail
