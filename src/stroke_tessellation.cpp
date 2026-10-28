#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include "vectorgl/detail/path_tessellation.hpp"

namespace vectorgl::detail
{
namespace
{
constexpr double pi = std::numbers::pi_v<double>;
struct Contour
{
    std::vector<Vec2> points;
    bool closed = false;
};

bool same(Vec2 a, Vec2 b)
{
    return a.x == b.x && a.y == b.y;
}
double cross(Vec2 a, Vec2 b)
{
    return static_cast<double>(a.x) * b.y - static_cast<double>(a.y) * b.x;
}
double dot(Vec2 a, Vec2 b)
{
    return static_cast<double>(a.x) * b.x + static_cast<double>(a.y) * b.y;
}

class StrokeBuilder
{
public:
    StrokeBuilder(float radius, const StrokeStyle& style) : radius_(radius), style_(style) {}

    void add(std::vector<Vec2> polygon)
    {
        double area = 0;
        for (size_t i = 0; i < polygon.size(); ++i)
            area += cross(polygon[i], polygon[(i + 1) % polygon.size()]);
        if (area == 0)
            return;
        // Union all pieces with nonzero winding, independent of path direction.
        if (area < 0)
            std::reverse(polygon.begin(), polygon.end());
        polygons_.push_back(std::move(polygon));
    }

    void arc(Vec2 center, double start, double sweep, bool sector)
    {
        // Bound curve error in local coordinates and cap subdivision work.
        const double step = std::min(pi / 8, 2 * std::acos(std::clamp(1.0 - 0.125 / radius_, 0.0, 1.0)));
        const int count = static_cast<int>(std::clamp(std::ceil(std::abs(sweep) / std::max(step, 1e-6)), 1.0, 512.0));
        std::vector<Vec2> polygon;
        polygon.reserve(static_cast<size_t>(count) + 2);
        if (sector)
            polygon.push_back(center);
        for (int i = 0; i <= count; ++i)
        {
            const double angle = start + sweep * i / count;
            polygon.push_back({center.x + radius_ * static_cast<float>(std::cos(angle)),
                               center.y + radius_ * static_cast<float>(std::sin(angle))});
        }
        add(std::move(polygon));
    }

    void join(Vec2 p, Vec2 before, Vec2 after)
    {
        const double turn = cross(before, after), alignment = dot(before, after);
        if (std::abs(turn) < 1e-8)
        {
            if (alignment < 0 && style_.join == LineJoin::Round)
                arc(p, 0, 2 * pi, false);
            return;
        }
        const float side = turn > 0 ? -1.0f : 1.0f;
        const Vec2 n0 = before.perp() * side, n1 = after.perp() * side;
        const Vec2 a = p + n0 * radius_, b = p + n1 * radius_;
        if (style_.join == LineJoin::Round)
        {
            arc(p, std::atan2(n0.y, n0.x), std::atan2(cross(n0, n1), dot(n0, n1)), true);
            return;
        }
        if (style_.join == LineJoin::Miter && 1 + alignment > 1e-12)
        {
            const Vec2 offset = (n0 + n1) * static_cast<float>(radius_ / (1 + alignment));
            if (std::hypot(offset.x, offset.y) <= static_cast<double>(radius_) * style_.miterLimit)
            {
                add({p, a, p + offset, b});
                return;
            }
        }
        add({p, a, b}); // Bevel or miter-limit fallback.
    }

    void contour(const Contour& input)
    {
        std::vector<Vec2> points;
        points.reserve(input.points.size());
        for (const auto point : input.points)
        {
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || std::abs(point.x) > 8388608 ||
                std::abs(point.y) > 8388608)
                throw std::invalid_argument("Stroke coordinates must be finite and within +/-8388608");
            if (points.empty() || !same(points.back(), point))
                points.push_back(point);
        }
        if (input.closed && points.size() > 1 && same(points.front(), points.back()))
            points.pop_back();
        if (points.empty())
            return;
        if (points.size() == 1)
        {
            // A moveto alone is not a stroked segment; explicit zero-length lines are.
            if (input.closed || input.points.size() > 1)
            {
                const Vec2 p = points.front();
                if (style_.cap == LineCap::Round)
                    arc(p, 0, 2 * pi, false);
                else if (style_.cap == LineCap::Square)
                    add({{p.x - radius_, p.y - radius_},
                         {p.x + radius_, p.y - radius_},
                         {p.x + radius_, p.y + radius_},
                         {p.x - radius_, p.y + radius_}});
            }
            return;
        }
        const size_t segments = input.closed ? points.size() : points.size() - 1;
        std::vector<Vec2> directions;
        directions.reserve(segments);
        for (size_t i = 0; i < segments; ++i)
        {
            Vec2 a = points[i], b = points[(i + 1) % points.size()];
            const Vec2 delta = b - a;
            const double length = std::hypot(static_cast<double>(delta.x), static_cast<double>(delta.y));
            const Vec2 direction{static_cast<float>(delta.x / length), static_cast<float>(delta.y / length)};
            const Vec2 normal = direction.perp() * radius_;
            directions.push_back(direction);
            if (!input.closed && style_.cap == LineCap::Square)
            {
                if (i == 0)
                    a = a - direction * radius_;
                if (i + 1 == segments)
                    b = b + direction * radius_;
            }
            add({a + normal, a - normal, b - normal, b + normal});
        }
        if (input.closed)
        {
            for (size_t i = 0; i < points.size(); ++i)
                join(points[i], directions[(i + segments - 1) % segments], directions[i]);
        }
        else
        {
            for (size_t i = 1; i + 1 < points.size(); ++i)
                join(points[i], directions[i - 1], directions[i]);
            if (style_.cap == LineCap::Round)
            {
                arc(points.front(), 0, 2 * pi, false);
                arc(points.back(), 0, 2 * pi, false);
            }
        }
    }

    PathMesh finish() const
    {
        return tessellatePath(polygons_, FillRule::NonZero);
    }

private:
    float radius_;
    const StrokeStyle& style_;
    std::vector<std::vector<Vec2>> polygons_;
};

PathMesh build(const std::vector<Contour>& contours, float width, const StrokeStyle& style)
{
    validateStrokeStyle(style);
    if (!std::isfinite(width))
        throw std::invalid_argument("Stroke width must be finite");
    if (!(width * 0.5f > 0))
        return {};
    StrokeBuilder builder(width * 0.5f, style);
    for (const auto& contour : contours)
        builder.contour(contour);
    return builder.finish();
}
} // namespace

void validateStrokeStyle(const StrokeStyle& style)
{
    if (!std::isfinite(style.miterLimit) || style.miterLimit < 1 ||
        (style.join != LineJoin::Miter && style.join != LineJoin::Round && style.join != LineJoin::Bevel) ||
        (style.cap != LineCap::Butt && style.cap != LineCap::Round && style.cap != LineCap::Square))
        throw std::invalid_argument("Invalid stroke style (miter limit must be finite and at least 1)");
}

PathMesh tessellateStroke(const std::vector<std::vector<Vec2>>& paths, float width, const StrokeStyle& style)
{
    std::vector<Contour> contours;
    contours.reserve(paths.size());
    for (const auto& points : paths)
        contours.push_back({points, points.size() > 2 && same(points.front(), points.back())});
    return build(contours, width, style);
}

PathMesh tessellateStroke(const Path2D& path, float width, const StrokeStyle& style)
{
    std::vector<Contour> contours;
    std::vector<Vec2> current;
    Vec2 start{}, cursor{};
    bool havePoint = false;
    auto flush = [&](bool closed)
    {
        if (!current.empty())
            contours.push_back({std::move(current), closed});
        current.clear();
    };
    for (const auto& point : path.points())
        switch (point.cmd)
        {
        case PathCmd::MoveTo:
            flush(false);
            current.push_back(point.pos);
            start = cursor = point.pos;
            havePoint = true;
            break;
        case PathCmd::LineTo:
            if (current.empty() && havePoint)
                current.push_back(cursor);
            current.push_back(point.pos);
            if (!havePoint)
                start = point.pos;
            cursor = point.pos;
            havePoint = true;
            break;
        case PathCmd::Close:
            flush(true);
            cursor = start;
            break;
        }
    flush(false);
    return build(contours, width, style);
}

bool meshContains(const PathMesh& mesh, Vec2 point)
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y))
        return false;
    auto side = [&](Vec2 a, Vec2 b)
    {
        return (static_cast<double>(b.x) - a.x) * (point.y - static_cast<double>(a.y)) -
               (static_cast<double>(b.y) - a.y) * (point.x - static_cast<double>(a.x));
    };
    for (size_t i = 0; i < mesh.triangles.size(); i += 3)
    {
        const Vec2 a = mesh.triangles[i], b = mesh.triangles[i + 1], c = mesh.triangles[i + 2];
        const double ab = side(a, b), bc = side(b, c), ca = side(c, a);
        if (cross(b - a, c - a) != 0 && !((ab < 0 || bc < 0 || ca < 0) && (ab > 0 || bc > 0 || ca > 0)))
            return true;
    }
    return false;
}
} // namespace vectorgl::detail
