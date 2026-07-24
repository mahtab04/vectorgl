#include <memory>
#include <vector>

#include <vectorgl/scene.hpp>

#include "test_utils.hpp"

int main()
{
    using namespace vectorgl;

    const auto transform = Mat3x3::translation(10.0f, 20.0f) * Mat3x3::rotation(0.5f) *
                           Mat3x3::scaling(2.0f, 3.0f);
    const auto origin = transform.transformPoint({0.0f, 0.0f});
    expectNear(origin.x, 10.0f, 0.0001f, "matrix translation x");
    expectNear(origin.y, 20.0f, 0.0001f, "matrix translation y");

    Scene scene;
    auto root = scene.rect(10.0f, 20.0f, 40.0f, 60.0f);
    expect(scene.roots().size() == 1, "factory adds exactly one root");
    scene.addRoot(root);
    expect(scene.roots().size() == 1, "duplicate root is rejected");

    auto child = std::make_shared<Node>(ShapeType::Circle);
    root->addChild(child);
    expect(scene.findNode(child->id()) == child, "child lookup by id");

    std::vector<Node*> renderList;
    scene.collectRenderList(renderList);
    expect(renderList.size() == 2, "root and child appear once in render list");

    root->addChild(root);
    child->addChild(root);
    expect(root->parent() == nullptr, "hierarchy cycle is rejected");
    expect(child->children().empty(), "child cannot adopt its ancestor");

    scene.update(0.0f);
    const auto center = root->worldTransform().transformPoint({0.0f, 0.0f});
    expectNear(center.x, 30.0f, 0.0001f, "rectangle center x");
    expectNear(center.y, 50.0f, 0.0001f, "rectangle center y");

    root->removeChild(child);
    expect(child->parent() == nullptr, "removeChild clears parent");
    expect(root->children().empty(), "removeChild updates child list");
}
