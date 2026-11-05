#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

#include "vectorgl/node.hpp"

namespace vectorgl::ui::detail
{
inline bool finiteAffine(const Mat3x3& m)
{
    return std::all_of(m.m.begin(), m.m.end(), [](float f) { return std::isfinite(f); }) && m.m[2] == 0 &&
           m.m[5] == 0 && m.m[8] == 1;
}
inline std::optional<Vec2> unmap(const Mat3x3& m, Vec2 p)
{
    if (!finiteAffine(m))
        return {};
    const double a = m.m[0], b = m.m[1], c = m.m[3], d = m.m[4];
    const double det = a * d - b * c;
    if (det == 0)
        return {};
    const double x = static_cast<double>(p.x) - m.m[6], y = static_cast<double>(p.y) - m.m[7];
    Vec2 result{static_cast<float>((d * x - c * y) / det), static_cast<float>((a * y - b * x) / det)};
    if (!std::isfinite(result.x) || !std::isfinite(result.y))
        return {};
    return result;
}
} // namespace vectorgl::ui::detail
