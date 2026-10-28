#include <limits>
#include <stdexcept>
#include <vectorgl/scene.hpp>

#include "test_utils.hpp"
#include "vectorgl/detail/path_tessellation.hpp"
#include "vectorgl/detail/svg_parser.hpp"

using namespace vectorgl;

int main()
{
    Path2D line;
    line.moveTo(20, 50);
    line.lineTo(80, 50);
    StrokeStyle style;
    expect(line.strokeContains(20, 40, 20), "butt stroke boundary inclusive");
    expect(!line.strokeContains(15, 50, 20), "butt has no extension");
    style.cap = LineCap::Round;
    expect(line.strokeContains(15, 50, 20, style), "round cap extends endpoint");
    expect(!line.strokeContains(11, 41, 20, style), "round cap excludes square corner");
    style.cap = LineCap::Square;
    expect(line.strokeContains(11, 41, 20, style), "square cap includes corner");
    expect(!line.strokeContains(9, 50, 20, style), "square cap stops at half-width");

    Path2D corner;
    corner.moveTo(20, 80);
    corner.lineTo(60, 80);
    corner.lineTo(60, 40);
    style = {};
    expect(corner.strokeContains(69, 89, 20, style), "miter joins extend to outer intersection");
    style.miterLimit = 1;
    expect(!corner.strokeContains(69, 89, 20, style), "miter limit falls back to bevel");
    style.join = LineJoin::Bevel;
    expect(!corner.strokeContains(66, 86, 20, style), "bevel trims outer corner");
    style.join = LineJoin::Round;
    expect(corner.strokeContains(66, 86, 20, style), "round join covers circular sector");
    expect(!corner.strokeContains(69, 89, 20, style), "round join excludes miter tip");
    Path2D reverse;
    reverse.moveTo(60, 40);
    reverse.lineTo(60, 80);
    reverse.lineTo(20, 80);
    expect(reverse.strokeContains(66, 86, 20, style), "reversed path has same round join");

    Path2D closed;
    closed.moveTo(30, 30);
    closed.lineTo(80, 30);
    closed.lineTo(30, 80);
    closed.lineTo(30, 30);
    style = {LineJoin::Bevel, LineCap::Square, 10};
    expect(closed.strokeContains(22, 22, 20, style), "open path returning to start retains caps");
    closed.closePath();
    expect(!closed.strokeContains(22, 22, 20, style), "close command joins endpoints instead of capping");
    style.cap = LineCap::Round;
    expect(!closed.strokeContains(22, 22, 20, style), "closed paths ignore cap style");
    expect(!closed.strokeContains(50, 50, 10, style), "closed outline leaves its interior empty");

    Path2D dot;
    dot.moveTo(50, 50);
    expect(!dot.strokeContains(50, 50, 20, style), "move-only contour is not stroked");
    dot.lineTo(50, 50);
    expect(dot.strokeContains(50, 59, 20, style), "zero-length round cap paints a dot");
    expect(!dot.strokeContains(59, 59, 20, style), "round dot is circular");
    Path2D closedDot;
    closedDot.moveTo(50, 50);
    closedDot.closePath();
    expect(closedDot.strokeContains(50, 59, 20, style), "zero-length closed path uses its round cap");
    style.cap = LineCap::Square;
    expect(dot.strokeContains(59, 59, 20, style), "zero-length square cap paints a square");
    style.cap = LineCap::Butt;
    expect(!dot.strokeContains(50, 50, 20, style), "zero-length butt cap paints nothing");
    expect(!line.strokeContains(40, 50, 0), "zero width is empty");
    expect(!line.strokeContains(40, 50, -1), "negative width is empty");
    expect(!line.strokeContains(40, 50, std::numeric_limits<float>::quiet_NaN()), "nonfinite width misses");
    style.miterLimit = 0;
    expect(!line.strokeContains(40, 50, 20, style), "invalid miter limit misses");
    bool rejected = false;
    try
    {
        (void)detail::strokeMesh(line, 20, style);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    expect(rejected, "invalid style rejected before rendering");

    bool hit = true;
    style = {};
    (void)detail::strokeMesh(line, 20, style, &hit);
    (void)detail::strokeMesh(line, 20, style, &hit);
    expect(hit, "unchanged stroke reuses cached mesh");
    style.cap = LineCap::Round;
    (void)detail::strokeMesh(line, 20, style, &hit);
    expect(!hit, "cap edit invalidates stroke cache key");
    (void)detail::strokeMesh(line, 10, style, &hit);
    expect(!hit, "width edit invalidates stroke cache key");
    Path2D copy = line;
    copy.lineTo(100, 50);
    expect(copy.strokeContains(95, 50, 10, style) && !line.strokeContains(95, 50, 10, style),
           "editing a copy preserves original stroke geometry");
    line.clear();
    expect(!line.strokeContains(50, 50, 20), "clear discards cached stroke");

    Scene scene;
    auto node = scene.path(corner);
    node->setStroke(Color::White, 20);
    node->setStrokeStyle({LineJoin::Miter, LineCap::Butt, 10});
    node->setPosition(200, 0);
    node->setScale(-2, 1);
    expect(scene.pick(62, 89) == node, "picking includes transformed miter tip");
    node->setStrokeStyle({LineJoin::Bevel, LineCap::Butt, 10});
    expect(!scene.pick(62, 89), "join edit immediately updates picking");
    auto segment = scene.line(0, 200, 100, 200);
    segment->setStroke(Color::White, 20);
    segment->setStrokeStyle({LineJoin::Miter, LineCap::Round, 10});
    expect(scene.pick(-5, 200) == segment, "scene line uses cap style in picking");
    segment->setSize(20, 0);
    expect(!scene.pick(-5, 200) && scene.pick(50, 200) == segment, "line resizing invalidates cached geometry");

    Path2D continuation;
    continuation.moveTo(10, 10);
    continuation.lineTo(40, 10);
    continuation.closePath();
    continuation.quadraticCurveTo(10, 30, 10, 40);
    expect(continuation.strokeContains(10, 30, 4), "curve after close begins at subpath start");

    using namespace vectorgl::detail::svg;
    XmlElement group;
    group.attrs = {{"stroke-linejoin", "round"}, {"stroke-linecap", "square"}, {"stroke-miterlimit", "2"}};
    const auto parent = resolveStyle(group, Style{});
    XmlElement child;
    expect(resolveStyle(child, parent).strokeStyle == StrokeStyle{LineJoin::Round, LineCap::Square, 2},
           "SVG stroke styles inherit");
    child.attrs = {{"style", "stroke-linejoin: bevel; stroke-linecap: round; stroke-miterlimit: 3"}};
    expect(resolveStyle(child, parent, "stroke-linejoin: miter").strokeStyle ==
               StrokeStyle{LineJoin::Bevel, LineCap::Round, 3},
           "SVG inline style wins over stylesheet");
    child.attrs = {{"stroke-miterlimit", "nan"}};
    expect(resolveStyle(child, parent).strokeStyle.miterLimit == 2, "SVG ignores invalid miter limits");
}
