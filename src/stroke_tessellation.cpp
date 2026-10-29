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
    Vec2 tangent{1, 0}; // Orientation of a generated zero-length square dash.
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
                {
                    const Vec2 along = input.tangent * radius_, across = along.perp();
                    add({p - along - across, p + along - across, p + along + across, p - along + across});
                }
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

// Limit generated work, including zero entries, rather than imposing a minimum
// dash size. A caller can otherwise request billions of subpixel intervals.
constexpr size_t maxDashSteps = 65536;

std::vector<Contour> dashed(const Contour& input, const StrokeStyle& style, double period, size_t& steps)
{
    std::vector<Vec2> points;
    for (const auto p : input.points)
    {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || std::abs(p.x) > 8388608 || std::abs(p.y) > 8388608)
            throw std::invalid_argument("Stroke coordinates must be finite and within +/-8388608");
        if (points.empty() || !same(points.back(), p))
            points.push_back(p);
    }
    if (points.empty() || (!input.closed && input.points.size() < 2))
        return {};
    if (input.closed && !same(points.front(), points.back()))
        points.push_back(points.front());
    std::vector<double> distances(points.size(), 0);
    for (size_t i = 1; i < points.size(); ++i)
        distances[i] = distances[i - 1] + std::hypot(static_cast<double>(points[i].x) - points[i - 1].x,
                                                     static_cast<double>(points[i].y) - points[i - 1].y);
    const double total = distances.back();
    if (total / period > maxDashSteps)
        throw std::length_error("Dash pattern exceeds 65536 interval steps per stroke");
    auto tick = [&]
    {
        if (++steps > maxDashSteps)
            throw std::length_error("Dash pattern exceeds 65536 interval steps per stroke");
    };
    const size_t count = style.dashArray.size() * (style.dashArray.size() % 2 ? 2 : 1);
    auto length = [&](size_t index) { return static_cast<double>(style.dashArray[index % style.dashArray.size()]); };
    double phase = std::fmod(static_cast<double>(style.dashOffset), period);
    if (phase < 0)
        phase += period;
    if (phase >= period)
        phase = 0;
    size_t index = 0;
    double remaining = length(index);
    while (phase > remaining)
    {
        tick();
        phase -= remaining;
        index = (index + 1) % count;
        remaining = length(index);
    }
    remaining -= phase;

    auto segmentAt = [&](double position)
    {
        if (points.size() < 2)
            return size_t{0};
        const auto upper = std::upper_bound(distances.begin(), distances.end(), position);
        const size_t next = static_cast<size_t>(upper - distances.begin());
        return std::min(next ? next - 1 : 0, points.size() - 2);
    };
    auto pointAt = [&](double position)
    {
        if (position >= total)
            return points.back();
        const size_t segment = segmentAt(position);
        const double t = (position - distances[segment]) / (distances[segment + 1] - distances[segment]);
        const Vec2 a = points[segment], b = points[segment + 1];
        return Vec2{static_cast<float>(a.x + (static_cast<double>(b.x) - a.x) * t),
                    static_cast<float>(a.y + (static_cast<double>(b.y) - a.y) * t)};
    };
    struct Run
    {
        double begin, end;
        Contour contour;
    };
    std::vector<Run> runs;
    double position = 0;
    do
    {
        tick();
        const double end = std::min(total, position + remaining);
        if (index % 2 == 0)
        {
            Contour run;
            run.points.push_back(pointAt(position));
            auto vertex = std::upper_bound(distances.begin(), distances.end(), position);
            while (vertex != distances.end() && *vertex < end)
            {
                run.points.push_back(points[static_cast<size_t>(vertex - distances.begin())]);
                ++vertex;
            }
            run.points.push_back(pointAt(end)); // Two equal points deliberately produce a cap dot.
            if (position == end && points.size() > 1)
            {
                const size_t segment = segmentAt(position);
                const Vec2 a = points[segment], b = points[segment + 1];
                const double distance = distances[segment + 1] - distances[segment];
                run.tangent = {static_cast<float>((static_cast<double>(b.x) - a.x) / distance),
                               static_cast<float>((static_cast<double>(b.y) - a.y) / distance)};
            }
            runs.push_back({position, end, std::move(run)});
        }
        if (end >= total)
            break;
        position = end;
        index = (index + 1) % count;
        remaining = length(index);
    } while (position < total);

    // A visible run crossing the closure gets a join at the seam, with caps
    // only at its true dash endpoints. An entirely covered loop stays closed.
    if (input.closed && !runs.empty() && runs.front().begin == 0 && runs.back().end == total)
    {
        if (runs.size() == 1)
            runs.front().contour.closed = true;
        else if (runs.front().end > 0 && runs.back().begin < total)
        {
            auto& last = runs.back().contour.points;
            const auto& first = runs.front().contour.points;
            last.insert(last.end(), first.begin() + 1, first.end());
            runs.erase(runs.begin());
        }
    }
    std::vector<Contour> result;
    result.reserve(runs.size());
    for (auto& run : runs)
        result.push_back(std::move(run.contour));
    return result;
}

PathMesh build(const std::vector<Contour>& contours, float width, const StrokeStyle& style)
{
    validateStrokeStyle(style);
    if (!std::isfinite(width))
        throw std::invalid_argument("Stroke width must be finite");
    if (!(width * 0.5f > 0))
        return {};
    StrokeBuilder builder(width * 0.5f, style);
    double period = 0;
    for (const auto length : style.dashArray)
        period += length;
    if (style.dashArray.size() % 2)
        period *= 2;
    size_t steps = 0;
    for (const auto& contour : contours)
    {
        if (period == 0)
            builder.contour(contour);
        else
            for (const auto& run : dashed(contour, style, period, steps))
                builder.contour(run);
    }
    return builder.finish();
}
} // namespace

void validateStrokeStyle(const StrokeStyle& style)
{
    if (!std::isfinite(style.miterLimit) || style.miterLimit < 1 ||
        (style.join != LineJoin::Miter && style.join != LineJoin::Round && style.join != LineJoin::Bevel) ||
        (style.cap != LineCap::Butt && style.cap != LineCap::Round && style.cap != LineCap::Square))
        throw std::invalid_argument("Invalid stroke style (miter limit must be finite and at least 1)");
    if (!std::isfinite(style.dashOffset))
        throw std::invalid_argument("Dash offset must be finite");
    for (const auto length : style.dashArray)
        if (!std::isfinite(length) || length < 0)
            throw std::invalid_argument("Dash lengths must be finite and nonnegative");
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
