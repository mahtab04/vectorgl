#include <vectorgl/color.hpp>
#include <vectorgl/node.hpp>
#include <vectorgl/path.hpp>

#include "test_utils.hpp"

int main()
{
    using namespace vectorgl;

    const Vec2 vector{3.0f, 4.0f};
    expectNear(vector.length(), 5.0f, 0.0001f, "vector length");
    expectNear(vector.normalized().length(), 1.0f, 0.0001f, "normalized vector length");
    expectNear(Vec2{}.normalized().length(), 0.0f, 0.0001f, "zero vector normalization");
    const auto perpendicular = vector.perp();
    expectNear(perpendicular.x, -4.0f, 0.0001f, "perpendicular x");
    expectNear(perpendicular.y, 3.0f, 0.0001f, "perpendicular y");

    const auto orange = Color::hex(0xFF8000, 0.25f);
    expectNear(orange.r, 1.0f, 0.0001f, "hex red");
    expectNear(orange.g, 128.0f / 255.0f, 0.0001f, "hex green");
    expectNear(orange.a, 0.25f, 0.0001f, "hex alpha");
    const auto midpoint = Color::lerp(Color::Black, Color::White, 0.5f);
    expectNear(midpoint.r, 0.5f, 0.0001f, "color midpoint");
    expectNear(orange.premultiplied().a, orange.a, 0.0001f, "premultiplied alpha");

    const auto matrix = Mat3x3::translation(5.0f, 7.0f) * Mat3x3::scaling(2.0f, 3.0f);
    const auto point = matrix.transformPoint({4.0f, 6.0f});
    expectNear(point.x, 13.0f, 0.0001f, "matrix transformed x");
    expectNear(point.y, 25.0f, 0.0001f, "matrix transformed y");

    Node node(ShapeType::Rect);
    node.clearDirty();
    node.setPosition(2.0f, 3.0f);
    expect(hasDirty(node.dirtyFlags(), DirtyFlag::TransformDirty), "position marks transform dirty");
    node.clearDirty();
    node.setSize(10.0f, 20.0f);
    expect(hasDirty(node.dirtyFlags(), DirtyFlag::GeometryDirty), "size marks geometry dirty");
    node.clearDirty();
    node.setFill(Color::Red);
    expect(hasDirty(node.dirtyFlags(), DirtyFlag::StyleDirty), "fill marks style dirty");
}
