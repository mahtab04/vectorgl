#include "vectorgl/detail/path_tessellation.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>

#include "tesselator.h"

namespace vectorgl::detail
{
PathMesh tessellatePath(const std::vector<std::vector<Vec2>>& contours, FillRule rule)
{
    // Validate the entire path so a bad contour cannot silently change its holes.
    size_t total = 0;
    for (const auto& contour : contours)
    {
        if (contour.size() > static_cast<size_t>(std::numeric_limits<int>::max()) - total)
            throw std::length_error("Too many path vertices");
        total += contour.size();
        for (const auto point : contour)
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || std::abs(point.x) > TESS_MAX_VALID_INPUT_VALUE ||
                std::abs(point.y) > TESS_MAX_VALID_INPUT_VALUE)
                throw std::invalid_argument("Path coordinates must be finite and within +/-8388608");
    }
    bool hasContour = false;
    for (const auto& contour : contours)
        hasContour = hasContour || contour.size() >= 3;
    if (!hasContour)
        return {};

    std::unique_ptr<TESStesselator, decltype(&tessDeleteTess)> tess(tessNewTess(nullptr), tessDeleteTess);
    if (!tess)
        throw std::bad_alloc();
    for (const auto& contour : contours)
        if (contour.size() >= 3)
            tessAddContour(tess.get(), 2, contour.data(), sizeof(Vec2), static_cast<int>(contour.size()));
    const TESSreal normal[] = {0, 0, 1};
    if (!tessTesselate(tess.get(), rule == FillRule::EvenOdd ? TESS_WINDING_ODD : TESS_WINDING_NONZERO,
                       TESS_CONNECTED_POLYGONS, 3, 2, normal))
    {
        if (tessGetStatus(tess.get()) == TESS_STATUS_OUT_OF_MEMORY)
            throw std::bad_alloc();
        throw std::runtime_error("Path tessellation failed");
    }
    const auto* vertices = tessGetVertices(tess.get());
    const auto* elements = tessGetElements(tess.get());
    PathMesh mesh;
    mesh.triangles.reserve(static_cast<size_t>(tessGetElementCount(tess.get())) * 3);
    for (int i = 0; i < tessGetElementCount(tess.get()); ++i)
    {
        const auto* triangle = elements + static_cast<size_t>(i) * 6;
        if (triangle[0] == TESS_UNDEF || triangle[1] == TESS_UNDEF || triangle[2] == TESS_UNDEF)
            continue;
        std::array<Vec2, 3> points;
        for (int j = 0; j < 3; ++j)
        {
            const auto index = static_cast<size_t>(triangle[j]) * 2;
            points[j] = {vertices[index], vertices[index + 1]};
            mesh.triangles.push_back(points[j]);
        }
        for (int j = 0; j < 3; ++j)
            if (triangle[3 + j] == TESS_UNDEF)
                mesh.boundary.push_back({points[j], points[(j + 1) % 3]});
    }
    return mesh;
}
} // namespace vectorgl::detail
