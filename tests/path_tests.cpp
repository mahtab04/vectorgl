#include <numbers>

#include <vectorgl/path.hpp>

#include "test_utils.hpp"

int main()
{
    using namespace vectorgl;

    Path2D rectangle;
    rectangle.rect(10.0f, 20.0f, 30.0f, 40.0f);
    const auto rectanglePaths = rectangle.getSubPaths();
    expect(rectanglePaths.size() == 1, "rectangle has one subpath");
    expect(rectanglePaths.front().size() == 5, "closed rectangle contains closing point");
    expectNear(rectanglePaths.front().front().x, 10.0f, 0.0001f, "rectangle start x");
    expectNear(rectanglePaths.front().back().x, 10.0f, 0.0001f, "rectangle closes x");
    expectNear(rectanglePaths.front().back().y, 20.0f, 0.0001f, "rectangle closes y");

    Path2D curves;
    curves.moveTo(0.0f, 0.0f);
    curves.quadraticCurveTo(5.0f, 10.0f, 10.0f, 0.0f);
    curves.bezierCurveTo(15.0f, -10.0f, 20.0f, 10.0f, 25.0f, 0.0f);
    const auto curvePaths = curves.getSubPaths();
    expect(curvePaths.size() == 1, "curves remain in one subpath");
    expect(curvePaths.front().size() > 10, "curves are adaptively sampled");
    expectNear(curvePaths.front().back().x, 25.0f, 0.0001f, "cubic endpoint x");
    expectNear(curvePaths.front().back().y, 0.0f, 0.0001f, "cubic endpoint y");

    Path2D arc;
    arc.arc(2.0f, 3.0f, 4.0f, 0.0f, std::numbers::pi_v<float> * 0.5f);
    const auto arcPaths = arc.getSubPaths();
    expect(arcPaths.size() == 1, "arc has one subpath");
    expectNear(arcPaths.front().front().x, 6.0f, 0.0001f, "arc start x");
    expectNear(arcPaths.front().back().x, 2.0f, 0.001f, "arc end x");
    expectNear(arcPaths.front().back().y, 7.0f, 0.001f, "arc end y");

    arc.clear();
    expect(arc.empty(), "clear removes path points");
    expect(arc.getSubPaths().empty(), "cleared path has no subpaths");
}
