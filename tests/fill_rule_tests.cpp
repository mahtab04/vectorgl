#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vectorgl/scene.hpp>

#include "test_utils.hpp"
#include "vectorgl/detail/path_tessellation.hpp"
#include "vectorgl/detail/svg_parser.hpp"

using namespace vectorgl;

namespace
{
float meshArea(const Path2D& path, FillRule rule)
{
    const auto mesh = detail::tessellatePath(path.getSubPaths(), rule);
    float area = 0;
    for (size_t i = 0; i < mesh.triangles.size(); i += 3)
    {
        const Vec2 a = mesh.triangles[i], b = mesh.triangles[i + 1], c = mesh.triangles[i + 2];
        area += std::abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) * 0.5f;
    }
    return area;
}
} // namespace

int main()
{
    Path2D nested;
    nested.rect(0, 0, 100, 100);
    nested.rect(20, 20, 60, 60);
    expect(nested.contains(50, 50), "default nonzero fills same-direction nested contours");
    expect(!nested.contains(50, 50, FillRule::EvenOdd), "evenodd creates a hole");
    expect(nested.contains(20, 50, FillRule::EvenOdd), "hole boundary is inclusive");
    expect(nested.contains(0, 0), "outer boundary is inclusive");
    expectNear(meshArea(nested, FillRule::NonZero), 10000, 0.01f, "nonzero nested area filled once");
    expectNear(meshArea(nested, FillRule::EvenOdd), 6400, 0.01f, "evenodd mesh excludes hole");
    nested.rect(40, 40, 20, 20);
    expect(nested.contains(50, 50, FillRule::EvenOdd), "nested island is filled");
    expectNear(meshArea(nested, FillRule::EvenOdd), 6800, 0.01f, "nested island mesh");

    Path2D reversed;
    reversed.rect(0, 0, 100, 100);
    reversed.rect(80, 20, -60, 60);
    expect(!reversed.contains(50, 50), "opposite winding creates nonzero hole");
    expectNear(meshArea(reversed, FillRule::NonZero), 6400, 0.01f, "opposite winding mesh");

    Path2D overlap;
    overlap.rect(0, 0, 60, 60);
    overlap.rect(30, 0, 60, 60);
    expect(overlap.contains(45, 30), "nonzero union includes overlap");
    expect(!overlap.contains(45, 30, FillRule::EvenOdd), "evenodd excludes overlap");
    expectNear(meshArea(overlap, FillRule::NonZero), 5400, 0.01f, "overlap is tessellated once");
    expectNear(meshArea(overlap, FillRule::EvenOdd), 3600, 0.01f, "overlap cancels under evenodd");
    const auto unionMesh = detail::tessellatePath(overlap.getSubPaths(), FillRule::NonZero);
    for (const auto& edge : unionMesh.boundary)
        expect(!(edge[0].x == edge[1].x && (edge[0].x == 30 || edge[0].x == 60)),
               "internal overlap edges have no AA fringe");

    Path2D duplicate;
    duplicate.rect(0, 0, 10, 10);
    duplicate.rect(0, 0, 10, 10);
    expect(!duplicate.contains(0, 5, FillRule::EvenOdd), "cancelled boundary is not picked");
    expectNear(meshArea(duplicate, FillRule::EvenOdd), 0, 0.01f, "duplicate contours cancel");

    Path2D bow;
    bow.moveTo(0, 0);
    bow.lineTo(100, 100);
    bow.lineTo(0, 100);
    bow.lineTo(100, 0); // Deliberately open: filling closes it, stroking does not.
    expect(bow.contains(50, 10), "zero signed area self-intersection has a filled lobe");
    expect(!bow.contains(10, 50), "bow tie side is empty");
    expect(bow.getSubPaths().front().size() == 4, "fill does not close stroke geometry");
    expectNear(meshArea(bow, FillRule::NonZero), 5000, 0.01f, "self-intersection tessellation");
    expectNear(meshArea(bow, FillRule::EvenOdd), 5000, 0.01f, "self-intersection evenodd");

    Path2D degenerate;
    degenerate.moveTo(0, 0);
    degenerate.lineTo(10, 0);
    expectNear(meshArea(degenerate, FillRule::NonZero), 0, 0, "two-point contour is empty");
    degenerate.lineTo(20, 0);
    expect(!degenerate.contains(5, 0), "degenerate boundary is not selectable");
    expectNear(meshArea(degenerate, FillRule::NonZero), 0, 0, "collinear contour is empty");
    degenerate.lineTo(std::numeric_limits<float>::quiet_NaN(), 0);
    expect(!degenerate.contains(5, 0), "invalid path cannot be picked");
    bool rejected = false;
    try
    {
        (void)meshArea(degenerate, FillRule::NonZero);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    expect(rejected, "nonfinite path rejected before tessellation");
    Path2D empty;
    expect(!empty.contains(0, 0), "empty path has no filled boundary");
    expect(!nested.contains(std::numeric_limits<float>::infinity(), 0), "invalid query misses");
    empty.rect(9000000, 0, 10, 10);
    expect(!empty.contains(9000001, 1), "out-of-range coordinates cannot be picked");

    Scene scene;
    auto node = scene.path(reversed);
    node->setFill(Color::Red);
    node->setPosition(200, 100);
    node->setScale(-2, 3);
    expect(!scene.pick(100, 250), "transformed hole misses");
    expect(scene.pick(190, 115) == node, "transformed rim picks");
    node->setPath(overlap);
    expect(scene.pick(110, 190) == node, "nonzero overlap picks");
    node->setFillRule(FillRule::EvenOdd);
    expect(!scene.pick(110, 190), "fill rule setter immediately updates picking");
    node->setStroke(Color::Blue, 4);
    expect(scene.pick(140, 190) == node, "original contour stroke remains pickable");

    using namespace vectorgl::detail::svg;
    // Reuse and invalidation must preserve ordinary value semantics.
    Path2D cached;
    cached.rect(0, 0, 100, 100);
    cached.rect(20, 20, 60, 60);
    bool hit = true;
    const auto* firstMesh = &detail::pathMesh(cached, FillRule::NonZero, &hit);
    expect(!hit, "first access builds path mesh");
    expect(&detail::pathMesh(cached, FillRule::NonZero, &hit) == firstMesh && hit, "unchanged geometry reuses mesh");
    (void)detail::pathMesh(cached, FillRule::EvenOdd, &hit);
    expect(!hit, "fill rules have separate mesh entries");
    (void)detail::pathMesh(cached, FillRule::EvenOdd, &hit);
    expect(hit, "alternating rules retain both meshes");
    Path2D copied = cached;
    copied.rect(200, 0, 10, 10);
    (void)detail::pathMesh(copied, FillRule::NonZero, &hit);
    expect(!hit && copied.contains(205, 5), "editing a copied path builds new geometry");
    expect(!cached.contains(205, 5) && &detail::pathMesh(cached, FillRule::NonZero) == firstMesh,
           "editing a copy preserves original cache and geometry");
    auto exported = cached.getSubPaths();
    exported.clear();
    expect(cached.contains(5, 5), "public contour getter still returns an independent copy");
    copied.clear();
    expect(!copied.contains(205, 5) && detail::pathMesh(copied, FillRule::NonZero).triangles.empty(),
           "clear invalidates all derived geometry");
    copied.moveTo(0, 0);
    copied.lineTo(100, 0);
    (void)detail::pathMesh(copied, FillRule::NonZero);
    copied.lineTo(0, 100);
    expect(copied.contains(10, 10), "lineTo invalidates cached contours");
    (void)detail::pathMesh(copied, FillRule::NonZero);
    copied.closePath();
    (void)detail::pathMesh(copied, FillRule::NonZero, &hit);
    expect(!hit, "closePath invalidates cached mesh");

    Style parent;
    XmlElement group;
    group.attrs = {{"fill-rule", "evenodd"}};
    parent = resolveStyle(group, parent);
    expect(parent.fillRule == FillRule::EvenOdd, "SVG presentation fill-rule");
    XmlElement child;
    expect(resolveStyle(child, parent).fillRule == FillRule::EvenOdd, "SVG rule inherits");
    child.attrs = {{"fill-rule", "nonzero"}};
    child.attrs.push_back({"style", "fill-rule: evenodd"});
    expect(resolveStyle(child, parent).fillRule == FillRule::EvenOdd, "SVG inline rule wins");
    child.attrs.back().value = "fill-rule: nonsense";
    expect(resolveStyle(child, parent).fillRule == FillRule::NonZero, "invalid rule preserves resolved value");
    expect(resolveStyle(child, parent, "fill-rule: evenodd").fillRule == FillRule::EvenOdd,
           "stylesheet overrides presentation rule");
    child.attrs.back().value = "fill-rule: nonzero";
    expect(resolveStyle(child, parent, "fill-rule: evenodd").fillRule == FillRule::NonZero,
           "inline rule overrides stylesheet");
}
