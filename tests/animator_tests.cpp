#include <vector>

#include <vectorgl/animator.hpp>
#include <vectorgl/node.hpp>
#include <vectorgl/scene.hpp>

#include "test_utils.hpp"

int main()
{
    using namespace vectorgl;

    expectNear(evalEasing(Ease::Linear, -1.0f), 0.0f, 0.0001f, "easing clamps low");
    expectNear(evalEasing(Ease::Linear, 2.0f), 1.0f, 0.0001f, "easing clamps high");
    expectNear(evalEasing(Ease::InQuad, 0.5f), 0.25f, 0.0001f, "quadratic easing");
    expectNear(evalEasing(Ease::OutCubic, 1.0f), 1.0f, 0.0001f, "cubic endpoint");

    Scene scene;
    auto node = scene.rect(0.0f, 0.0f, 20.0f, 20.0f);

    AnimTarget from;
    from.x = 10.0f;
    from.opacity = 0.0f;
    from.mask = AnimTarget::PosX | AnimTarget::Opacity;
    AnimTarget to;
    to.x = 110.0f;
    to.opacity = 1.0f;
    to.mask = from.mask;

    scene.animator().emplace<TweenAnimation>(node->id(), from, to, 2.0f, Ease::Linear);
    scene.update(1.0f);
    expectNear(node->position().x, 60.0f, 0.0001f, "tween midpoint position");
    expectNear(node->style().opacity, 0.5f, 0.0001f, "tween midpoint opacity");
    expect(scene.animator().hasActiveAnimations(), "midpoint tween remains active");
    scene.update(1.0f);
    expectNear(node->position().x, 110.0f, 0.0001f, "completed tween applies endpoint");
    expectNear(node->style().opacity, 1.0f, 0.0001f, "completed tween applies endpoint opacity");
    expect(!scene.animator().hasActiveAnimations(), "completed tween is removed after apply");

    AnimTarget first;
    first.x = 0.0f;
    first.mask = AnimTarget::PosX;
    AnimTarget second;
    second.x = 20.0f;
    second.mask = AnimTarget::PosX;
    std::vector<Keyframe> keyframes{{0.0f, first}, {2.0f, second}};
    KeyframeAnimation keyframe(node->id(), keyframes);
    keyframe.update(1.0f);
    keyframe.apply(*node);
    expectNear(node->position().x, 10.0f, 0.0001f, "keyframe midpoint");

    KeyframeAnimation empty(node->id(), {}, LoopMode::Loop);
    expect(empty.update(1.0f), "empty keyframes finish without hanging");

    TweenAnimation zeroDuration(node->id(), from, to, 0.0f, Ease::Linear);
    expect(zeroDuration.update(0.1f), "zero-duration tween completes");
    zeroDuration.apply(*node);
    expectNear(node->position().x, 110.0f, 0.0001f, "zero-duration tween endpoint");
}
