#include <vectorgl/canvas.hpp>
#include <vectorgl/color.hpp>
#include <vectorgl/path.hpp>

constexpr vectorgl::Color (*opaqueColor)(uint32_t) = &vectorgl::Color::hex;
static_assert(opaqueColor(0x663399) == vectorgl::Color::RebeccaPurple);
static_assert(vectorgl::Color{2, -1, .5f, 1}.clamped() == vectorgl::Color{1, 0, .5f, 1});

int main()
{
    // Constructing Canvas forces the consumer to link the renderer and vendored
    // GLAD objects without requiring an OpenGL context for this smoke test.
    vectorgl::Canvas canvas;

    vectorgl::Path2D path;
    path.moveTo(0.0f, 0.0f);
    path.lineTo(20.0f, 0.0f);
    path.lineTo(20.0f, 20.0f);
    path.closePath();

    const auto color = vectorgl::Color::hex(0x3366FF);
    path.rect(8, 2, 4, 2);
    if (!path.contains(10, 3) || path.contains(10, 3, vectorgl::FillRule::EvenOdd))
        return 1;
    vectorgl::Path2D stroke;
    stroke.moveTo(0, 0);
    stroke.lineTo(20, 0);
    if (!stroke.strokeContains(-2, 0, 6, {vectorgl::LineJoin::Round, vectorgl::LineCap::Round, 4}))
        return 1;
    vectorgl::StrokeStyle dashed;
    dashed.dashArray = {5, 5};
    if (!stroke.strokeContains(2, 0, 2, dashed) || stroke.strokeContains(7, 0, 2, dashed))
        return 1;
    return path.empty() || color.a != 1.0f ? 1 : 0;
}
