#include <cassert>
#include <cmath>
#include <memory>

#include <vectorgl/scene.hpp>

namespace
{

bool near(float lhs, float rhs)
{
    return std::abs(lhs - rhs) < 0.0001f;
}

} // namespace

int main()
{
    using namespace vectorgl;

    const auto transform = Mat3x3::translation(10.0f, 20.0f) * Mat3x3::rotation(0.5f) *
                           Mat3x3::scaling(2.0f, 3.0f);
    const auto origin = transform.transformPoint({0.0f, 0.0f});
    assert(near(origin.x, 10.0f));
    assert(near(origin.y, 20.0f));

    Scene scene;
    auto root = scene.rect(10.0f, 20.0f, 40.0f, 60.0f);
    assert(scene.roots().size() == 1);
    scene.addRoot(root);
    assert(scene.roots().size() == 1);

    auto child = std::make_shared<Node>(ShapeType::Circle);
    root->addChild(child);
    assert(scene.findNode(child->id()) == child);
    std::vector<Node*> renderList;
    scene.collectRenderList(renderList);
    assert(renderList.size() == 2);

    scene.update(0.0f);
    const auto center = root->worldTransform().transformPoint({0.0f, 0.0f});
    assert(near(center.x, 30.0f));
    assert(near(center.y, 50.0f));
}
