#include "vectorgl/path.hpp"

#include <algorithm>
#include <cmath>

namespace vectorgl
{

void Path2D::moveTo(float x, float y)
{
    cursor_ = {x, y};
    points_.push_back({PathCmd::MoveTo, cursor_});
}

void Path2D::lineTo(float x, float y)
{
    cursor_ = {x, y};
    points_.push_back({PathCmd::LineTo, cursor_});
}

void Path2D::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
{
    Vec2 p0 = cursor_;
    Vec2 p1{cp1x, cp1y}, p2{cp2x, cp2y}, p3{x, y};
    // Adaptive subdivision: more segments for larger/curvier arcs
    float d1 = std::hypot(p1.x - p0.x, p1.y - p0.y);
    float d2 = std::hypot(p2.x - p1.x, p2.y - p1.y);
    float d3 = std::hypot(p3.x - p2.x, p3.y - p2.y);
    float arcLen = d1 + d2 + d3;
    int segments = std::clamp(static_cast<int>(arcLen * 0.5f), 8, 128);
    for (int i = 1; i <= segments; ++i)
    {
        float t = static_cast<float>(i) / segments;
        float u = 1.0f - t;
        float tt = t * t, uu = u * u;
        float uuu = uu * u, ttt = tt * t;
        Vec2 pt{uuu * p0.x + 3 * uu * t * p1.x + 3 * u * tt * p2.x + ttt * p3.x,
                uuu * p0.y + 3 * uu * t * p1.y + 3 * u * tt * p2.y + ttt * p3.y};
        lineTo(pt.x, pt.y);
    }
}

void Path2D::quadraticCurveTo(float cpx, float cpy, float x, float y)
{
    Vec2 p0 = cursor_;
    Vec2 p1{cpx, cpy}, p2{x, y};
    // Adaptive subdivision based on control polygon length
    float arcLen = std::hypot(p1.x - p0.x, p1.y - p0.y) + std::hypot(p2.x - p1.x, p2.y - p1.y);
    int segments = std::clamp(static_cast<int>(arcLen * 0.5f), 6, 96);
    for (int i = 1; i <= segments; ++i)
    {
        float t = static_cast<float>(i) / segments;
        float u = 1.0f - t;
        Vec2 pt{u * u * p0.x + 2 * u * t * p1.x + t * t * p2.x, u * u * p0.y + 2 * u * t * p1.y + t * t * p2.y};
        lineTo(pt.x, pt.y);
    }
}

void Path2D::arc(float cx, float cy, float r, float startAngle, float endAngle, bool ccw)
{
    float diff = endAngle - startAngle;
    if (ccw)
    {
        if (diff > 0)
            diff -= 2.0f * std::numbers::pi_v<float>;
    }
    else
    {
        if (diff < 0)
            diff += 2.0f * std::numbers::pi_v<float>;
    }

    int segments = std::max(8, static_cast<int>(std::abs(diff) * r * 0.5f));
    segments = std::min(segments, 128);

    for (int i = 0; i <= segments; ++i)
    {
        float t = static_cast<float>(i) / segments;
        float angle = startAngle + diff * t;
        float px = cx + r * std::cos(angle);
        float py = cy + r * std::sin(angle);
        if (i == 0 && points_.empty())
            moveTo(px, py);
        else
            lineTo(px, py);
    }
}

void Path2D::ellipse(float cx, float cy, float rx, float ry, float rotation, float startAngle, float endAngle, bool ccw)
{
    float diff = endAngle - startAngle;
    if (ccw)
    {
        if (diff > 0)
            diff -= 2.0f * std::numbers::pi_v<float>;
    }
    else
    {
        if (diff < 0)
            diff += 2.0f * std::numbers::pi_v<float>;
    }

    int segments = std::max(8, static_cast<int>(std::abs(diff) * std::max(rx, ry) * 0.5f));
    segments = std::min(segments, 128);

    float cosR = std::cos(rotation), sinR = std::sin(rotation);
    for (int i = 0; i <= segments; ++i)
    {
        float t = static_cast<float>(i) / segments;
        float angle = startAngle + diff * t;
        float lx = rx * std::cos(angle);
        float ly = ry * std::sin(angle);
        float px = cx + lx * cosR - ly * sinR;
        float py = cy + lx * sinR + ly * cosR;
        if (i == 0 && points_.empty())
            moveTo(px, py);
        else
            lineTo(px, py);
    }
}

void Path2D::rect(float x, float y, float w, float h)
{
    moveTo(x, y);
    lineTo(x + w, y);
    lineTo(x + w, y + h);
    lineTo(x, y + h);
    closePath();
}

void Path2D::closePath()
{
    points_.push_back({PathCmd::Close, {}});
}

void Path2D::clear()
{
    points_.clear();
    cursor_ = {0, 0};
}

std::vector<std::vector<Vec2>> Path2D::getSubPaths() const
{
    std::vector<std::vector<Vec2>> subPaths;
    std::vector<Vec2> current;
    Vec2 subPathStart{0, 0};

    for (auto& pt : points_)
    {
        switch (pt.cmd)
        {
        case PathCmd::MoveTo:
            if (!current.empty())
                subPaths.push_back(std::move(current));
            current.clear();
            current.push_back(pt.pos);
            subPathStart = pt.pos;
            break;
        case PathCmd::LineTo:
            current.push_back(pt.pos);
            break;
        case PathCmd::Close:
            if (!current.empty())
            {
                current.push_back(subPathStart);
                subPaths.push_back(std::move(current));
                current.clear();
            }
            break;
        }
    }
    if (!current.empty())
        subPaths.push_back(std::move(current));
    return subPaths;
}

} // namespace vectorgl
