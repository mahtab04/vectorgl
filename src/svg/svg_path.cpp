// ============================================================================
// svg_path.cpp — SVG path d-attribute → Canvas path commands
// ============================================================================

#include <algorithm>
#include <cmath>

#include "svg_common.hpp"
#include "vectorgl/canvas.hpp"
#include "vectorgl/detail/svg_parser.hpp"

namespace vectorgl
{
namespace detail
{
namespace svg
{

namespace
{

/// Convert an SVG elliptical arc to a sequence of cubic bezier curves
/// emitted as Canvas::bezierCurveTo calls.
void emitArcAsBeziers(Canvas& canvas, float x1, float y1, float x2, float y2, float rx, float ry, float phi,
                      bool largeArc, bool sweep)
{
    if (rx == 0 || ry == 0)
    {
        canvas.lineTo(x2, y2);
        return;
    }

    float cosPhi = std::cos(phi), sinPhi = std::sin(phi);

    // Step 1: Transform to unit-arc coordinates
    float dx = (x1 - x2) * 0.5f;
    float dy = (y1 - y2) * 0.5f;
    float x1p = cosPhi * dx + sinPhi * dy;
    float y1p = -sinPhi * dx + cosPhi * dy;

    // Step 2: Correct out-of-range radii
    float x1pSq = x1p * x1p, y1pSq = y1p * y1p;
    float rxSq = rx * rx, rySq = ry * ry;
    float lambda = x1pSq / rxSq + y1pSq / rySq;
    if (lambda > 1.0f)
    {
        float sqLambda = std::sqrt(lambda);
        rx *= sqLambda;
        ry *= sqLambda;
        rxSq = rx * rx;
        rySq = ry * ry;
    }

    // Step 3: Compute center point
    float num = rxSq * rySq - rxSq * y1pSq - rySq * x1pSq;
    float den = rxSq * y1pSq + rySq * x1pSq;
    float sq = (den > 0) ? std::sqrt(std::max(0.0f, num / den)) : 0.0f;
    if (largeArc == sweep)
        sq = -sq;

    float cxp = sq * rx * y1p / ry;
    float cyp = -sq * ry * x1p / rx;

    float cx = cosPhi * cxp - sinPhi * cyp + (x1 + x2) * 0.5f;
    float cy = sinPhi * cxp + cosPhi * cyp + (y1 + y2) * 0.5f;

    // Step 4: Compute sweep angles
    auto vectorAngle = [](float ux, float uy, float vx, float vy) -> float
    {
        float dot = ux * vx + uy * vy;
        float len = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
        float angle = (len > 0) ? std::acos(std::clamp(dot / len, -1.0f, 1.0f)) : 0.0f;
        if (ux * vy - uy * vx < 0)
            angle = -angle;
        return angle;
    };

    float theta1 = vectorAngle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    float dtheta = vectorAngle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);

    if (!sweep && dtheta > 0)
        dtheta -= 2.0f * kPi;
    if (sweep && dtheta < 0)
        dtheta += 2.0f * kPi;

    // Step 5: Emit cubic bezier segments (≤ 90° each)
    int segments = static_cast<int>(std::ceil(std::abs(dtheta) / (kPi * 0.5f)));
    if (segments < 1)
        segments = 1;
    float segAngle = dtheta / segments;
    float alpha = 4.0f / 3.0f * std::tan(segAngle * 0.25f);

    for (int i = 0; i < segments; ++i)
    {
        float a1 = theta1 + i * segAngle;
        float a2 = theta1 + (i + 1) * segAngle;

        float cos1 = std::cos(a1), sin1 = std::sin(a1);
        float cos2 = std::cos(a2), sin2 = std::sin(a2);

        float p2x = rx * cos2, p2y = ry * sin2;
        float cp1x = rx * cos1 - alpha * rx * sin1;
        float cp1y = ry * sin1 + alpha * ry * cos1;
        float cp2x = p2x + alpha * rx * sin2;
        float cp2y = p2y - alpha * ry * cos2;

        auto xform = [&](float px, float py, float& ox, float& oy)
        {
            ox = cosPhi * px - sinPhi * py + cx;
            oy = sinPhi * px + cosPhi * py + cy;
        };

        float bx1, by1, bx2, by2, bx3, by3;
        xform(cp1x, cp1y, bx1, by1);
        xform(cp2x, cp2y, bx2, by2);
        xform(p2x, p2y, bx3, by3);
        canvas.bezierCurveTo(bx1, by1, bx2, by2, bx3, by3);
    }
}

} // anonymous namespace

void emitPathCommands(const std::string& pathData, Canvas& canvas)
{
    const char* cursor = pathData.c_str();
    float curX = 0, curY = 0;     // Current point
    float cpX = 0, cpY = 0;       // Last control point (for S/T)
    float startX = 0, startY = 0; // Subpath start (for Z)

    auto moreNumbers = [&]() { return hasMoreNumbers(cursor); };

    while (*cursor)
    {
        skipWhitespaceAndCommas(cursor);
        if (!*cursor)
            break;

        char cmd = *cursor;
        if ((cmd >= 'A' && cmd <= 'Z') || (cmd >= 'a' && cmd <= 'z'))
        {
            ++cursor;
        }
        else if (isNumberStart(*cursor))
        {
            cmd = 'L'; // Implicit lineTo after moveTo
        }
        else
        {
            ++cursor;
            continue;
        }

        switch (cmd)
        {
        // ── Move ─────────────────────────────────────────────────────────
        case 'M':
            while (moreNumbers())
            {
                float x = consumeNumber(cursor), y = consumeNumber(cursor);
                canvas.moveTo(x, y);
                curX = x;
                curY = y;
                startX = x;
                startY = y;
                while (moreNumbers())
                {
                    x = consumeNumber(cursor);
                    y = consumeNumber(cursor);
                    canvas.lineTo(x, y);
                    curX = x;
                    curY = y;
                }
            }
            break;
        case 'm':
            while (moreNumbers())
            {
                float dx = consumeNumber(cursor), dy = consumeNumber(cursor);
                curX += dx;
                curY += dy;
                canvas.moveTo(curX, curY);
                startX = curX;
                startY = curY;
                while (moreNumbers())
                {
                    dx = consumeNumber(cursor);
                    dy = consumeNumber(cursor);
                    curX += dx;
                    curY += dy;
                    canvas.lineTo(curX, curY);
                }
            }
            break;

        // ── Line ─────────────────────────────────────────────────────────
        case 'L':
            while (moreNumbers())
            {
                curX = consumeNumber(cursor);
                curY = consumeNumber(cursor);
                canvas.lineTo(curX, curY);
            }
            break;
        case 'l':
            while (moreNumbers())
            {
                curX += consumeNumber(cursor);
                curY += consumeNumber(cursor);
                canvas.lineTo(curX, curY);
            }
            break;
        case 'H':
            while (moreNumbers())
            {
                curX = consumeNumber(cursor);
                canvas.lineTo(curX, curY);
            }
            break;
        case 'h':
            while (moreNumbers())
            {
                curX += consumeNumber(cursor);
                canvas.lineTo(curX, curY);
            }
            break;
        case 'V':
            while (moreNumbers())
            {
                curY = consumeNumber(cursor);
                canvas.lineTo(curX, curY);
            }
            break;
        case 'v':
            while (moreNumbers())
            {
                curY += consumeNumber(cursor);
                canvas.lineTo(curX, curY);
            }
            break;

        // ── Cubic Bezier ─────────────────────────────────────────────────
        case 'C':
            while (moreNumbers())
            {
                float x1 = consumeNumber(cursor), y1 = consumeNumber(cursor);
                float x2 = consumeNumber(cursor), y2 = consumeNumber(cursor);
                float x = consumeNumber(cursor), y = consumeNumber(cursor);
                canvas.bezierCurveTo(x1, y1, x2, y2, x, y);
                cpX = x2;
                cpY = y2;
                curX = x;
                curY = y;
            }
            break;
        case 'c':
            while (moreNumbers())
            {
                float dx1 = consumeNumber(cursor), dy1 = consumeNumber(cursor);
                float dx2 = consumeNumber(cursor), dy2 = consumeNumber(cursor);
                float dx = consumeNumber(cursor), dy = consumeNumber(cursor);
                float x1 = curX + dx1, y1 = curY + dy1;
                float x2 = curX + dx2, y2 = curY + dy2;
                float x = curX + dx, y = curY + dy;
                canvas.bezierCurveTo(x1, y1, x2, y2, x, y);
                cpX = x2;
                cpY = y2;
                curX = x;
                curY = y;
            }
            break;

        // ── Smooth Cubic ─────────────────────────────────────────────────
        case 'S':
            while (moreNumbers())
            {
                float x1 = 2 * curX - cpX, y1 = 2 * curY - cpY;
                float x2 = consumeNumber(cursor), y2 = consumeNumber(cursor);
                float x = consumeNumber(cursor), y = consumeNumber(cursor);
                canvas.bezierCurveTo(x1, y1, x2, y2, x, y);
                cpX = x2;
                cpY = y2;
                curX = x;
                curY = y;
            }
            break;
        case 's':
            while (moreNumbers())
            {
                float x1 = 2 * curX - cpX, y1 = 2 * curY - cpY;
                float dx2 = consumeNumber(cursor), dy2 = consumeNumber(cursor);
                float dx = consumeNumber(cursor), dy = consumeNumber(cursor);
                float x2 = curX + dx2, y2 = curY + dy2;
                float x = curX + dx, y = curY + dy;
                canvas.bezierCurveTo(x1, y1, x2, y2, x, y);
                cpX = x2;
                cpY = y2;
                curX = x;
                curY = y;
            }
            break;

        // ── Quadratic Bezier ─────────────────────────────────────────────
        case 'Q':
            while (moreNumbers())
            {
                cpX = consumeNumber(cursor);
                cpY = consumeNumber(cursor);
                curX = consumeNumber(cursor);
                curY = consumeNumber(cursor);
                canvas.quadraticCurveTo(cpX, cpY, curX, curY);
            }
            break;
        case 'q':
            while (moreNumbers())
            {
                float dcx = consumeNumber(cursor), dcy = consumeNumber(cursor);
                float dx = consumeNumber(cursor), dy = consumeNumber(cursor);
                cpX = curX + dcx;
                cpY = curY + dcy;
                curX += dx;
                curY += dy;
                canvas.quadraticCurveTo(cpX, cpY, curX, curY);
            }
            break;

        // ── Smooth Quadratic ─────────────────────────────────────────────
        case 'T':
            while (moreNumbers())
            {
                cpX = 2 * curX - cpX;
                cpY = 2 * curY - cpY;
                curX = consumeNumber(cursor);
                curY = consumeNumber(cursor);
                canvas.quadraticCurveTo(cpX, cpY, curX, curY);
            }
            break;
        case 't':
            while (moreNumbers())
            {
                cpX = 2 * curX - cpX;
                cpY = 2 * curY - cpY;
                curX += consumeNumber(cursor);
                curY += consumeNumber(cursor);
                canvas.quadraticCurveTo(cpX, cpY, curX, curY);
            }
            break;

        // ── Elliptical Arc ───────────────────────────────────────────────
        case 'A':
        case 'a':
        {
            bool relative = (cmd == 'a');
            while (moreNumbers())
            {
                float arcRx = std::abs(consumeNumber(cursor));
                float arcRy = std::abs(consumeNumber(cursor));
                float rotate = consumeNumber(cursor) * kPi / 180.0f;
                float large = consumeNumber(cursor);
                float sweepF = consumeNumber(cursor);
                float x = consumeNumber(cursor), y = consumeNumber(cursor);
                if (relative)
                {
                    x += curX;
                    y += curY;
                }
                emitArcAsBeziers(canvas, curX, curY, x, y, arcRx, arcRy, rotate, large > 0.5f, sweepF > 0.5f);
                curX = x;
                curY = y;
            }
            break;
        }

        // ── Close Path ───────────────────────────────────────────────────
        case 'Z':
        case 'z':
            canvas.closePath();
            curX = startX;
            curY = startY;
            break;

        default:
            break;
        }
    }
}

} // namespace svg
} // namespace detail
} // namespace vectorgl
