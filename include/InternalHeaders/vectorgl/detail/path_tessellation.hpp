#pragma once

#include <array>
#include <vectorgl/path.hpp>

namespace vectorgl::detail
{
struct PathMesh
{
    std::vector<Vec2> triangles;
    // Directed edges with filled area on their left.
    std::vector<std::array<Vec2, 2>> boundary;
};

// Invalid/non-finite coordinates are rejected before entering the tessellator.
PathMesh tessellatePath(const std::vector<std::vector<Vec2>>& contours, FillRule rule);
} // namespace vectorgl::detail
