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
// Raw arrays treat a repeated last/first point as closed. Path2D preserves Close commands.
PathMesh tessellateStroke(const std::vector<std::vector<Vec2>>& contours, float width, const StrokeStyle& style);
PathMesh tessellateStroke(const Path2D& path, float width, const StrokeStyle& style);
bool meshContains(const PathMesh& mesh, Vec2 point);
void validateStrokeStyle(const StrokeStyle& style);
} // namespace vectorgl::detail
