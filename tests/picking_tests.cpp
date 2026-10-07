#include <limits>
#include <vectorgl/scene.hpp>

#include "test_utils.hpp"

int main()
{
    using namespace vectorgl;
    Scene scene;
    expect(!scene.pick(0, 0), "empty scene");
    auto back = scene.rect(0, 0, 100, 100);
    back->setFill(Color::Blue);
    auto front = scene.circle(50, 50, 30);
    front->setFill(Color::Red);
    expect(scene.pick(50, 50) == front, "later node wins tied z-index");
    expect(scene.pick(0, 0) == back, "rectangle boundary");
    expect(scene.pick(79, 79) == back, "circle corners are empty");
    back->setZIndex(1);
    expect(scene.pick(50, 50) == back, "higher z-index wins");
    back->setVisible(false);
    expect(scene.pick(50, 50) == front, "hidden node skipped");
    front->setOpacity(0);
    expect(!scene.pick(50, 50), "zero opacity skipped");
    front->setOpacity(1);
    front->setFill(Color::Transparent);
    front->setStroke(Color::White, 10);
    expect(!scene.pick(50, 50), "outline has empty center");
    expect(scene.pick(84, 50) == front, "outer stroke picked");
    expect(!scene.pick(86, 50), "outside stroke missed");
    front->setPosition(200, 200);
    expect(scene.pick(234, 200) == front, "setter updates picking without update");
    front->setScale(0, 1);
    expect(!scene.pick(200, 200), "collapsed shape skipped");
    expect(!scene.pick(std::numeric_limits<float>::quiet_NaN(), 0), "NaN coordinate rejected");
    expect(!scene.pick(0, std::numeric_limits<float>::infinity()), "infinite coordinate rejected");

    Scene hierarchy;
    auto group = hierarchy.group();
    auto child = hierarchy.rect(-20, -10, 40, 20);
    child->setFill(Color::Blue);
    group->addChild(child);
    group->setPosition(300, 200);
    group->setRotation(1.57079632679f);
    group->setScale(2, 3);
    expect(hierarchy.pick(300, 235) == child, "parent rotation and nonuniform scale");
    expect(!hierarchy.pick(335, 200), "outside transformed child");
    group->setVisible(false);
    expect(!hierarchy.pick(300, 200), "hidden parent hides children still in root list");
    group->setVisible(true);
    auto sibling = hierarchy.circle(0, 0, 10);
    sibling->setFill(Color::Red);
    group->addChild(sibling);
    expect(hierarchy.pick(300, 200) == sibling, "later child wins");
    child->setZIndex(5);
    expect(hierarchy.pick(300, 200) == child, "child z-index is global like rendering");
    group->setPosition(400, 200);
    expect(!hierarchy.pick(300, 200), "parent change invalidates cached transform");
    expect(hierarchy.pick(400, 200) == child, "new parent position picked");

    Scene geometry;
    auto rounded = geometry.roundedRect(0, 0, 100, 100, 30);
    rounded->setFill(Color::Blue);
    expect(!geometry.pick(1, 1), "rounded corner excluded");
    expect(geometry.pick(50, 1) == rounded, "rounded straight edge");
    auto ellipse = geometry.ellipse(200, 50, 40, 20);
    ellipse->setFill(Color::Red);
    expect(geometry.pick(200, 50) == ellipse, "ellipse center handles zero distance denominator");
    expect(geometry.pick(235, 50) == ellipse, "ellipse long axis");
    expect(!geometry.pick(230, 68), "ellipse bounding box corner excluded");
    ellipse->setPosition(std::numeric_limits<float>::quiet_NaN(), 50);
    expect(!geometry.pick(200, 50), "nonfinite transform rejected");
    ellipse->setPosition(200, 50);
    ellipse->setFill(Color::Transparent);
    ellipse->setStroke(Color::White, 4);
    expect(!geometry.pick(200, 50), "ellipse outline center excluded");
    expect(geometry.pick(240, 50) == ellipse, "ellipse stroke boundary");
    auto line = geometry.line(300, 10, 300, 90);
    line->setStroke(Color::White, 8);
    expect(geometry.pick(303, 50) == line, "rotated line stroke");
    expect(!geometry.pick(305, 50), "outside line width");
    expect(!geometry.pick(300, 95), "line uses butt caps");
    line->setStroke(Color::White, 0);
    expect(geometry.pick(300, 50) == line, "line default one-pixel width");

    Path2D polygon;
    polygon.moveTo(0, 0);
    polygon.lineTo(60, 0);
    polygon.lineTo(60, 20);
    polygon.lineTo(20, 20);
    polygon.lineTo(20, 60);
    polygon.lineTo(0, 60);
    polygon.closePath();
    polygon.rect(100, 0, 20, 20);
    auto path = geometry.path(polygon);
    path->setPosition(400, 20);
    path->setFill(Color::Blue);
    path->setScale(-2, 1);
    expect(geometry.pick(380, 60) == path, "concave path with reflected transform");
    expect(!geometry.pick(320, 60), "concave notch excluded");
    expect(geometry.pick(180, 30) == path, "multiple subpaths");
    path->setFill(Color::Transparent);
    path->setStroke(Color::White, 4);
    expect(geometry.pick(380, 20) == path, "transformed path stroke");
    expect(!geometry.pick(380, 30), "unfilled path interior");
    path->setOpacity(0);
    expect(!geometry.pick(380, 20), "path opacity respected");
    path->setOpacity(1);
    path->setScale(0, 1);
    expect(!geometry.pick(400, 20), "singular path transform skipped");

    auto unsupported = std::make_shared<Node>(ShapeType::Text);
    unsupported->setSize(1000, 1000);
    unsupported->setFill(Color::White);
    geometry.addRoot(unsupported);
    expect(geometry.pick(50, 50) == rounded, "unrendered node types skipped");

    Scene animated;
    auto moving = animated.circle(100, 100, 20);
    moving->setFill(Color::Blue);
    AnimTarget from, to;
    from.x = 500;
    from.mask = AnimTarget::PosX;
    to.x = 700;
    to.mask = AnimTarget::PosX;
    animated.animator().emplace<TweenAnimation>(moving->id(), from, to, 1.0f, Ease::Linear);
    expect(animated.pick(100, 100) == moving, "pick does not apply or advance pending animations");
    expectNear(moving->position().x, 100, 0.001f, "picking leaves animated properties unchanged");
}
