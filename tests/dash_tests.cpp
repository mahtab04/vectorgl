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
    line.moveTo(0, 20);
    line.lineTo(100, 20);
    StrokeStyle style;
    style.dashArray = {10, 10};
    expect(line.strokeContains(5, 20, 4, style), "dash is painted");
    expect(!line.strokeContains(15, 20, 4, style), "gap is empty");
    expect(line.strokeContains(25, 20, 4, style), "pattern repeats");
    style.dashOffset = 5;
    expect(line.strokeContains(2, 20, 4, style) && !line.strokeContains(7, 20, 4, style),
           "positive offset advances phase");
    style.dashOffset = -5;
    expect(!line.strokeContains(2, 20, 4, style) && line.strokeContains(7, 20, 4, style),
           "negative offset wraps phase");
    style.dashOffset = 35;
    expect(!line.strokeContains(2, 20, 4, style) && line.strokeContains(7, 20, 4, style), "offset wraps by period");
    style.dashOffset = 0;
    style.dashArray = {5, 3, 2};
    for (const float x : {1.0f, 9.0f, 16.0f, 21.0f})
        expect(line.strokeContains(x, 20, 4, style), "odd pattern repeats to an even length");
    for (const float x : {6.0f, 12.0f, 19.0f})
        expect(!line.strokeContains(x, 20, 4, style), "odd pattern alternates its second copy");
    style.dashArray = {0, 0, 0};
    expect(line.strokeContains(15, 20, 4, style), "all-zero pattern is solid");
    style.dashArray.clear();
    style.dashOffset = 9999;
    expect(line.strokeContains(15, 20, 4, style), "empty pattern ignores offset");

    style = {};
    style.cap = LineCap::Round;
    style.dashArray = {0, 12};
    expect(line.strokeContains(1, 20, 6, style), "zero-length round dash paints first dot");
    expect(line.strokeContains(12, 22, 6, style), "zero-length round dashes repeat");
    expect(!line.strokeContains(6, 20, 6, style), "dotted stroke has real gaps");
    Path2D diagonal;
    diagonal.moveTo(0, 0);
    diagonal.lineTo(60, 60);
    style.cap = LineCap::Square;
    style.dashArray = {0, 20};
    expect(diagonal.strokeContains(2.5f, 0, 4, style), "zero-length square dash follows path tangent");
    expect(!diagonal.strokeContains(7, 7, 4, style), "square dotted gap stays empty");
    style.cap = LineCap::Butt;
    expect(!line.strokeContains(0, 20, 6, style), "zero-length butt dashes paint nothing");

    Path2D corner;
    corner.moveTo(0, 0);
    corner.lineTo(20, 0);
    corner.lineTo(20, 30);
    style = {};
    style.dashArray = {35, 10};
    expect(corner.strokeContains(24, -4, 10, style), "dash crossing a corner keeps its join");
    expect(!corner.strokeContains(20, 20, 4, style), "phase continues across segments");
    Path2D subpaths;
    subpaths.moveTo(0, 0);
    subpaths.lineTo(30, 0);
    subpaths.moveTo(0, 20);
    subpaths.lineTo(30, 20);
    style.dashArray = {10, 10};
    expect(subpaths.strokeContains(5, 20, 4, style) && !subpaths.strokeContains(15, 20, 4, style),
           "dash phase restarts at each subpath");

    Path2D loop;
    loop.rect(20, 20, 40, 40);
    style.dashArray = {30, 20};
    style.dashOffset = 10;
    expect(loop.strokeContains(16, 16, 10, style), "visible dash crossing the closure gets a join");
    expect(!loop.strokeContains(50, 20, 4, style), "closed path still has gaps");
    style.dashArray = {1000, 10};
    style.dashOffset = 0;
    expect(loop.strokeContains(16, 16, 10, style), "fully covered loop stays closed");
    style.dashArray = {30, 20};
    style.dashOffset = 35;
    expect(!loop.strokeContains(16, 16, 10, style), "gap crossing the closure stays empty");

    Path2D dot;
    dot.moveTo(0, 20);
    dot.lineTo(0, 20);
    style = {};
    style.cap = LineCap::Round;
    style.dashArray = {10, 10};
    expect(dot.strokeContains(1, 20, 6, style), "zero-length subpath in a dash paints a cap");
    style.dashOffset = 15;
    expect(!dot.strokeContains(1, 20, 6, style), "zero-length subpath in a gap is empty");

    bool hit = false;
    style = {};
    style.dashArray = {10, 10};
    (void)detail::strokeMesh(line, 4, style, &hit);
    (void)detail::strokeMesh(line, 4, style, &hit);
    expect(hit, "unchanged dash reuses stroke cache");
    style.dashOffset = 2;
    (void)detail::strokeMesh(line, 4, style, &hit);
    expect(!hit, "dash offset changes the cache key");
    style.dashArray = {20, 10};
    (void)detail::strokeMesh(line, 4, style, &hit);
    expect(!hit, "dash pattern changes the cache key");
    for (float invalid : {-1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        style.dashArray = {10, invalid};
        expect(!line.strokeContains(5, 20, 4, style), "invalid dash lengths are rejected");
    }
    style.dashArray = {10, 10};
    style.dashOffset = std::numeric_limits<float>::infinity();
    expect(!line.strokeContains(5, 20, 4, style), "nonfinite offset is rejected");
    style.dashOffset = std::numeric_limits<float>::max();
    expect(!detail::strokeMesh(line, 4, style).triangles.empty(), "large finite offsets normalize safely");
    style.dashArray = {1e-30f, 1e-30f};
    style.dashOffset = 0;
    bool limited = false;
    try
    {
        (void)detail::strokeMesh(line, 4, style);
    }
    catch (const std::length_error&)
    {
        limited = true;
    }
    expect(limited && !line.strokeContains(5, 20, 4, style), "excessive dash density fails without hanging");

    Scene scene;
    auto node = scene.path(line);
    node->setStroke(Color::White, 4);
    style = {};
    style.dashArray = {10, 10};
    node->setStrokeStyle(style);
    node->setPosition(200, 0);
    node->setScale(-2, 1);
    expect(scene.pick(190, 20) == node && !scene.pick(170, 20), "picking respects transformed dash gaps");
    style.dashOffset = 10;
    node->setStrokeStyle(style);
    expect(!scene.pick(190, 20) && scene.pick(170, 20) == node, "offset edit immediately updates picking");
    auto connector = scene.line(0, 100, 100, 100);
    connector->setStroke(Color::White, 4);
    style.dashOffset = 0;
    connector->setStrokeStyle(style);
    expect(scene.pick(5, 100) == connector && !scene.pick(15, 100), "line nodes honor dash gaps too");

    using namespace vectorgl::detail::svg;
    XmlElement group;
    group.attrs = {{"stroke-dasharray", "10px, 5 2"}, {"stroke-dashoffset", "-3px"}};
    const auto parent = resolveStyle(group, Style{});
    expect(parent.strokeStyle.dashArray == std::vector<float>{10, 5, 2} && parent.strokeStyle.dashOffset == -3,
           "SVG reads comma/space dash lists, px and signed offsets");
    XmlElement child;
    expect(resolveStyle(child, parent).strokeStyle == parent.strokeStyle, "SVG dash styles inherit");
    child.attrs = {{"style", "stroke-dasharray: none; stroke-dashoffset: 2"}};
    const auto overridden = resolveStyle(child, parent, "stroke-dasharray: 1 2");
    expect(overridden.strokeStyle.dashArray.empty() && overridden.strokeStyle.dashOffset == 2,
           "inline SVG dash style overrides stylesheet");
    for (const char* invalid : {"10 -2", "10,", "10,,5", "nan 5", "10% 5", "1em 2", "10+5"})
    {
        child.attrs = {{"stroke-dasharray", invalid}};
        expect(resolveStyle(child, parent).strokeStyle.dashArray == parent.strokeStyle.dashArray,
               "invalid/unsupported SVG dash lists preserve inherited pattern");
    }
}
