// ============================================================================
// svg_transform.cpp — SVG transform attribute parsing
// ============================================================================

#include <cmath>
#include <cstdio>
#include <cstring>

#include "svg_common.hpp"
#include "vectorgl/detail/svg_parser.hpp"

namespace vectorgl
{
namespace detail
{
namespace svg
{

Transform Transform::multiply(const Transform& lhs, const Transform& rhs)
{
    Transform result;
    result.a = lhs.a * rhs.a + lhs.c * rhs.b;
    result.b = lhs.b * rhs.a + lhs.d * rhs.b;
    result.c = lhs.a * rhs.c + lhs.c * rhs.d;
    result.d = lhs.b * rhs.c + lhs.d * rhs.d;
    result.e = lhs.a * rhs.e + lhs.c * rhs.f + lhs.e;
    result.f = lhs.b * rhs.e + lhs.d * rhs.f + lhs.f;
    return result;
}

namespace
{

void advancePastClosingParen(const char*& cursor)
{
    while (*cursor && *cursor != ')')
        ++cursor;
    if (*cursor)
        ++cursor;
}

void advanceToOpeningParen(const char*& cursor)
{
    while (*cursor && *cursor != '(')
        ++cursor;
    if (*cursor)
        ++cursor;
}

Transform parseSingleTransform(const char*& cursor)
{
    skipWhitespaceAndCommas(cursor);
    Transform t;

    if (std::strncmp(cursor, "translate", 9) == 0)
    {
        cursor += 9;
        advanceToOpeningParen(cursor);
        float tx = consumeNumber(cursor);
        float ty = consumeNumber(cursor);
        advancePastClosingParen(cursor);
        t.e = tx;
        t.f = ty;
    }
    else if (std::strncmp(cursor, "scale", 5) == 0)
    {
        cursor += 5;
        advanceToOpeningParen(cursor);
        float sx = consumeNumber(cursor);
        float sy = sx;
        const char* saved = cursor;
        float maybeY = consumeNumber(cursor);
        if (cursor != saved)
            sy = maybeY;
        advancePastClosingParen(cursor);
        t.a = sx;
        t.d = sy;
    }
    else if (std::strncmp(cursor, "rotate", 6) == 0)
    {
        cursor += 6;
        advanceToOpeningParen(cursor);
        float degrees = consumeNumber(cursor);
        advancePastClosingParen(cursor);
        float rad = degrees * kPi / 180.0f;
        float cs = std::cos(rad), sn = std::sin(rad);
        t.a = cs;
        t.c = -sn;
        t.b = sn;
        t.d = cs;
    }
    else if (std::strncmp(cursor, "matrix", 6) == 0)
    {
        cursor += 6;
        advanceToOpeningParen(cursor);
        t.a = consumeNumber(cursor);
        t.b = consumeNumber(cursor);
        t.c = consumeNumber(cursor);
        t.d = consumeNumber(cursor);
        t.e = consumeNumber(cursor);
        t.f = consumeNumber(cursor);
        advancePastClosingParen(cursor);
    }
    else if (std::strncmp(cursor, "skewX", 5) == 0)
    {
        cursor += 5;
        advanceToOpeningParen(cursor);
        float degrees = consumeNumber(cursor);
        advancePastClosingParen(cursor);
        t.c = std::tan(degrees * kPi / 180.0f);
    }
    else if (std::strncmp(cursor, "skewY", 5) == 0)
    {
        cursor += 5;
        advanceToOpeningParen(cursor);
        float degrees = consumeNumber(cursor);
        advancePastClosingParen(cursor);
        t.b = std::tan(degrees * kPi / 180.0f);
    }
    else
    {
        ++cursor; // Skip unknown character
        return Transform::identity();
    }

    return t;
}

} // anonymous namespace

Transform parseTransform(const std::string& str)
{
    if (str.empty())
        return Transform::identity();

    Transform accumulated = Transform::identity();
    const char* cursor = str.c_str();

    while (*cursor)
    {
        skipWhitespaceAndCommas(cursor);
        if (!*cursor)
            break;

        Transform single = parseSingleTransform(cursor);
        accumulated = Transform::multiply(accumulated, single);
    }
    return accumulated;
}

// ============================================================================
// ViewBox Parsing
// ============================================================================

ViewBox parseViewBox(const std::string& str)
{
    ViewBox vb;
    if (str.empty())
        return vb;
    if (std::sscanf(str.c_str(), "%f %f %f %f", &vb.x, &vb.y, &vb.w, &vb.h) == 4)
    {
        vb.valid = (vb.w > 0 && vb.h > 0);
    }
    return vb;
}

} // namespace svg
} // namespace detail
} // namespace vectorgl
