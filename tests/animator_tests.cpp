#include <cmath>
#include <limits>
#include <stdexcept>
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

    for (uint32_t mask : {uint32_t(AnimTarget::ScaleX), uint32_t(AnimTarget::ScaleY),
                          uint32_t(AnimTarget::ScaleX | AnimTarget::ScaleY)})
    {
        AnimTarget scaleFrom;
        scaleFrom.scaleX = 2.0f;
        scaleFrom.scaleY = 3.0f;
        AnimTarget scaleTo;
        scaleTo.scaleX = 6.0f;
        scaleTo.scaleY = 7.0f;
        scaleTo.mask = mask;
        node->setScale(2.0f, 3.0f);
        TweenAnimation scaleTween(node->id(), scaleFrom, scaleTo, 2.0f, Ease::Linear);
        scaleTween.update(1.0f);
        scaleTween.apply(*node);
        expectNear(node->scale().x, (mask & AnimTarget::ScaleX) ? 4.0f : 2.0f, 0.0001f, "tween scale x");
        expectNear(node->scale().y, (mask & AnimTarget::ScaleY) ? 5.0f : 3.0f, 0.0001f, "tween scale y");

        node->setScale(2.0f, 3.0f);
        KeyframeAnimation scaleKeyframe(node->id(), {{0.0f, scaleFrom}, {2.0f, scaleTo}});
        scaleKeyframe.update(1.0f);
        scaleKeyframe.apply(*node);
        expectNear(node->scale().x, (mask & AnimTarget::ScaleX) ? 4.0f : 2.0f, 0.0001f, "keyframe scale x");
        expectNear(node->scale().y, (mask & AnimTarget::ScaleY) ? 5.0f : 3.0f, 0.0001f, "keyframe scale y");
    }

    auto removed = scene.circle(0.0f, 0.0f, 10.0f);
    scene.animator().emplace<SpringAnimation>(removed->id(), to);
    scene.animator().emplace<TweenAnimation>(removed->id(), from, to, 1.0f, Ease::Linear, LoopMode::Loop);
    scene.animator().emplace<KeyframeAnimation>(removed->id(), keyframes, LoopMode::Loop);
    scene.removeRoot(removed);
    scene.update(0.016f);
    expect(!scene.animator().hasActiveAnimations(), "orphan springs and looping animations are removed");

    auto parent = scene.group();
    auto detached = std::make_shared<Node>();
    parent->addChild(detached);
    scene.animator().emplace<SpringAnimation>(detached->id(), to);
    scene.update(0.016f);
    expect(scene.animator().hasActiveAnimations(), "spring with reachable child target remains active");
    parent->removeChild(detached);
    scene.update(0.016f);
    expect(!scene.animator().hasActiveAnimations(), "initialized spring is removed after child detaches");

    for (LoopMode loop : {LoopMode::Loop, LoopMode::PingPong})
    {
        TweenAnimation tiny(node->id(), from, to, 0.000001f, Ease::Linear, loop);
        expect(!tiny.update(100.0f), "large time step wraps tiny tween without hanging");
        tiny.apply(*node);
        expect(std::isfinite(node->position().x), "wrapped tween produces finite position");
        KeyframeAnimation tinyKeyframes(node->id(), {{0, from}, {0.000001f, to}}, loop);
        expect(!tinyKeyframes.update(std::numeric_limits<float>::max()), "large keyframe step wraps without overflow");
        tinyKeyframes.apply(*node);
        expect(std::isfinite(node->position().x), "wrapped keyframes produce finite position");
    }

    AnimTarget allFrom, allTo;
    allFrom.width = 10;
    allFrom.height = 20;
    allFrom.fill = Color::Red;
    allFrom.stroke = Color::Blue;
    allTo.width = 30;
    allTo.height = 40;
    allTo.rotation = 2;
    allTo.fill = Color::Blue;
    allTo.stroke = Color::Red;
    allTo.mask = AnimTarget::Width | AnimTarget::Height | AnimTarget::Rotation | AnimTarget::Fill | AnimTarget::Stroke;
    node->setStroke(Color::White, 3);
    KeyframeAnimation complete(node->id(), {{0, allFrom}, {2, allTo}});
    complete.update(1);
    complete.apply(*node);
    expectNear(node->rotation(), 1, 0.0001f, "keyframe rotation is interpolated");
    expectNear(node->size().x, 20, 0.0001f, "keyframe width is interpolated");
    expectNear(node->size().y, 30, 0.0001f, "keyframe height is interpolated");
    const auto expectedFill = Color::lerpOklab(allFrom.fill, allTo.fill, 0.5f);
    expectNear(node->style().fillColor.r, expectedFill.r, 0.0001f, "keyframe fill is interpolated");
    const auto expectedStroke = Color::lerpOklab(allFrom.stroke, allTo.stroke, 0.5f);
    expectNear(node->style().strokeColor.b, expectedStroke.b, 0.0001f, "keyframe stroke is interpolated");
    expectNear(node->style().strokeWidth, 3, 0.0001f, "stroke animation preserves width");
    TweenAnimation completeTween(node->id(), allFrom, allTo, 2, Ease::Linear);
    completeTween.update(2);
    completeTween.apply(*node);
    expectNear(node->size().x, 30, 0.0001f, "tween supports width");
    expectNear(node->size().y, 40, 0.0001f, "tween supports height");

    const auto rejects = [](auto operation)
    {
        bool rejected = false;
        try
        {
            operation();
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        expect(rejected, "invalid animation timing is rejected");
    };
    rejects([&] { completeTween.update(-1); });
    rejects([&] { complete.update(std::numeric_limits<float>::infinity()); });
    rejects([&] { scene.animator().update(std::numeric_limits<float>::quiet_NaN()); });
    rejects([&] { TweenAnimation invalid(node->id(), {}, {}, std::numeric_limits<float>::infinity(), Ease::Linear); });
    rejects([&] { KeyframeAnimation invalid(node->id(), {{1, from}, {0, to}}); });
    rejects([&] { KeyframeAnimation invalid(node->id(), {{0, from}, {std::numeric_limits<float>::quiet_NaN(), to}}); });
}
