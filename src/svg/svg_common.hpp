// ============================================================================
// svg_common.hpp — Shared character-level helpers for SVG parsing
// ============================================================================
#pragma once

#include <cstdlib>
#include <cstring>
#include <string>

namespace vectorgl
{
namespace detail
{
namespace svg
{

constexpr float kPi = 3.14159265358979323846f;

inline bool isWhitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

inline bool isWhitespaceOrComma(char c)
{
    return c == ' ' || c == ',' || c == '\t' || c == '\n' || c == '\r';
}

inline bool isNumberStart(char c)
{
    return (c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.';
}

inline void skipWhitespace(const char*& cursor)
{
    while (*cursor && isWhitespace(*cursor))
        ++cursor;
}

inline void skipWhitespaceAndCommas(const char*& cursor)
{
    while (*cursor && isWhitespaceOrComma(*cursor))
        ++cursor;
}

inline float consumeNumber(const char*& cursor)
{
    skipWhitespaceAndCommas(cursor);
    char* end = nullptr;
    float value = std::strtof(cursor, &end);
    if (end == cursor)
        return 0.0f;
    cursor = end;
    return value;
}

inline bool hasMoreNumbers(const char* cursor)
{
    while (*cursor && isWhitespaceOrComma(*cursor))
        ++cursor;
    return *cursor && isNumberStart(*cursor);
}

inline std::string trimWhitespace(const std::string& str)
{
    size_t start = 0;
    while (start < str.size() && isWhitespace(str[start]))
        ++start;
    size_t end = str.size();
    while (end > start && isWhitespace(str[end - 1]))
        --end;
    return str.substr(start, end - start);
}

} // namespace svg
} // namespace detail
} // namespace vectorgl
