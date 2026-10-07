#include "vectorgl/animator.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include "vectorgl/node.hpp"
#include "vectorgl/scene.hpp"

namespace vectorgl
{
namespace
{
void validateDeltaTime(float dt)
{
    if (!std::isfinite(dt) || dt < 0.0f)
        throw std::invalid_argument("Animation delta time must be finite and non-negative");
}

void applyProperties(Node& node, const AnimTarget& from, const AnimTarget& to, float t)
{
    const auto interpolate = [t](float a, float b) { return a + (b - a) * t; };
    Vec2 position = node.position();
    if (to.mask & AnimTarget::PosX)
        position.x = interpolate(from.x, to.x);
    if (to.mask & AnimTarget::PosY)
        position.y = interpolate(from.y, to.y);
    if (to.mask & (AnimTarget::PosX | AnimTarget::PosY))
        node.setPosition(position.x, position.y);
    Vec2 size = node.size();
    if (to.mask & AnimTarget::Width)
        size.x = interpolate(from.width, to.width);
    if (to.mask & AnimTarget::Height)
        size.y = interpolate(from.height, to.height);
    if (to.mask & (AnimTarget::Width | AnimTarget::Height))
        node.setSize(size.x, size.y);
    Vec2 scale = node.scale();
    if (to.mask & AnimTarget::ScaleX)
        scale.x = interpolate(from.scaleX, to.scaleX);
    if (to.mask & AnimTarget::ScaleY)
        scale.y = interpolate(from.scaleY, to.scaleY);
    if (to.mask & (AnimTarget::ScaleX | AnimTarget::ScaleY))
        node.setScale(scale.x, scale.y);
    if (to.mask & AnimTarget::Rotation)
        node.setRotation(interpolate(from.rotation, to.rotation));
    if (to.mask & AnimTarget::Opacity)
        node.setOpacity(interpolate(from.opacity, to.opacity));
    if (to.mask & AnimTarget::Fill)
        node.setFill(Color::lerpOklab(from.fill, to.fill, t));
    if (to.mask & AnimTarget::Stroke)
        node.setStroke(Color::lerpOklab(from.stroke, to.stroke, t), node.style().strokeWidth);
}
} // namespace

// --- Easing functions ---

static float bounceOut(float t)
{
    if (t < 1.0f / 2.75f)
        return 7.5625f * t * t;
    if (t < 2.0f / 2.75f)
    {
        t -= 1.5f / 2.75f;
        return 7.5625f * t * t + 0.75f;
    }
    if (t < 2.5f / 2.75f)
    {
        t -= 2.25f / 2.75f;
        return 7.5625f * t * t + 0.9375f;
    }
    t -= 2.625f / 2.75f;
    return 7.5625f * t * t + 0.984375f;
}

float evalEasing(Ease ease, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    switch (ease)
    {
    case Ease::Linear:
        return t;
    case Ease::InQuad:
        return t * t;
    case Ease::OutQuad:
        return t * (2.0f - t);
    case Ease::InOutQuad:
        return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
    case Ease::InCubic:
        return t * t * t;
    case Ease::OutCubic:
    {
        float u = t - 1;
        return u * u * u + 1;
    }
    case Ease::InOutCubic:
        return t < 0.5f ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
    case Ease::InQuart:
        return t * t * t * t;
    case Ease::OutQuart:
    {
        float u = t - 1;
        return 1 - u * u * u * u;
    }
    case Ease::InOutQuart:
        return t < 0.5f ? 8 * t * t * t * t : 1 - 8 * (t - 1) * (t - 1) * (t - 1) * (t - 1);
    case Ease::InExpo:
        return t == 0 ? 0 : std::pow(2.0f, 10 * (t - 1));
    case Ease::OutExpo:
        return t == 1 ? 1 : 1 - std::pow(2.0f, -10 * t);
    case Ease::InOutExpo:
        if (t == 0 || t == 1)
            return t;
        return t < 0.5f ? 0.5f * std::pow(2.0f, 20 * t - 10) : 1 - 0.5f * std::pow(2.0f, -20 * t + 10);
    case Ease::InBack:
    {
        constexpr float s = 1.70158f;
        return t * t * ((s + 1) * t - s);
    }
    case Ease::OutBack:
    {
        constexpr float s = 1.70158f;
        float u = t - 1;
        return u * u * ((s + 1) * u + s) + 1;
    }
    case Ease::InOutBack:
    {
        constexpr float s = 1.70158f * 1.525f;
        float u = t * 2;
        if (u < 1)
            return 0.5f * (u * u * ((s + 1) * u - s));
        u -= 2;
        return 0.5f * (u * u * ((s + 1) * u + s) + 2);
    }
    case Ease::InElastic:
    {
        if (t == 0 || t == 1)
            return t;
        return -std::pow(2.0f, 10 * (t - 1)) * std::sin((t - 1.1f) * 5 * std::numbers::pi_v<float>);
    }
    case Ease::OutElastic:
    {
        if (t == 0 || t == 1)
            return t;
        return std::pow(2.0f, -10 * t) * std::sin((t - 0.1f) * 5 * std::numbers::pi_v<float>) + 1;
    }
    case Ease::InOutElastic:
    {
        if (t == 0 || t == 1)
            return t;
        float u = t * 2;
        if (u < 1)
            return -0.5f * std::pow(2.0f, 10 * (u - 1)) * std::sin((u - 1.1f) * 5 * std::numbers::pi_v<float>);
        return 0.5f * std::pow(2.0f, -10 * (u - 1)) * std::sin((u - 1.1f) * 5 * std::numbers::pi_v<float>) + 1;
    }
    case Ease::InBounce:
        return 1 - bounceOut(1 - t);
    case Ease::OutBounce:
        return bounceOut(t);
    case Ease::InOutBounce:
        return t < 0.5f ? (1 - bounceOut(1 - 2 * t)) * 0.5f : (1 + bounceOut(2 * t - 1)) * 0.5f;
    }
    return t;
}

// --- TweenAnimation ---

TweenAnimation::TweenAnimation(uint32_t nodeId, AnimTarget from, AnimTarget to, float duration, Ease easing,
                               LoopMode loop)
    : from_(from), to_(to), duration_(std::max(duration, 0.000001f)), easing_(easing), loop_(loop)
{
    if (!std::isfinite(duration) || duration < 0.0f)
        throw std::invalid_argument("Tween duration must be finite and non-negative");
    nodeId_ = nodeId;
}

bool TweenAnimation::update(float dt)
{
    validateDeltaTime(dt);
    elapsed_ += dt;

    if (loop_ == LoopMode::None)
    {
        if (elapsed_ >= duration_)
        {
            elapsed_ = duration_;
            finished_ = true;
        }
    }
    else if (loop_ == LoopMode::Loop)
    {
        elapsed_ = std::fmod(elapsed_, static_cast<double>(duration_));
    }
    else if (loop_ == LoopMode::PingPong)
    {
        double cycle = static_cast<double>(duration_) * 2;
        elapsed_ = std::fmod(elapsed_, cycle);
        reverse_ = elapsed_ > duration_;
    }
    return finished_;
}

void TweenAnimation::apply(Node& node)
{
    float rawT = static_cast<float>(elapsed_ / duration_);
    if (loop_ == LoopMode::PingPong && reverse_)
    {
        rawT = static_cast<float>(2.0 - (elapsed_ / duration_));
    }
    rawT = std::clamp(rawT, 0.0f, 1.0f);
    float t = evalEasing(easing_, rawT);

    applyProperties(node, from_, to_, t);
}

// --- SpringAnimation ---

SpringAnimation::SpringAnimation(uint32_t nodeId, AnimTarget target, float stiffness, float damping)
    : target_(target), stiffness_(stiffness), damping_(damping)
{
    nodeId_ = nodeId;
}

bool SpringAnimation::update(float dt)
{
    validateDeltaTime(dt);
    if (!initialized_)
        return false;

    // Critically damped spring: F = -k*x - d*v
    float dx = currentX_ - target_.x;
    float dy = currentY_ - target_.y;
    float ax = -stiffness_ * dx - damping_ * velocityX_;
    float ay = -stiffness_ * dy - damping_ * velocityY_;
    velocityX_ += ax * dt;
    velocityY_ += ay * dt;
    currentX_ += velocityX_ * dt;
    currentY_ += velocityY_ * dt;

    // Check if settled
    float speed = std::sqrt(velocityX_ * velocityX_ + velocityY_ * velocityY_);
    float dist = std::sqrt(dx * dx + dy * dy);
    if (speed < 0.01f && dist < 0.1f)
    {
        currentX_ = target_.x;
        currentY_ = target_.y;
        finished_ = true;
    }
    return finished_;
}

void SpringAnimation::apply(Node& node)
{
    if (!initialized_)
    {
        currentX_ = node.position().x;
        currentY_ = node.position().y;
        initialized_ = true;
    }
    node.setPosition(currentX_, currentY_);
}

// --- KeyframeAnimation ---

KeyframeAnimation::KeyframeAnimation(uint32_t nodeId, std::vector<Keyframe> keyframes, LoopMode loop)
    : keyframes_(std::move(keyframes)), loop_(loop)
{
    nodeId_ = nodeId;
    if (!keyframes_.empty())
    {
        float previous = 0.0f;
        for (const auto& keyframe : keyframes_)
        {
            if (!std::isfinite(keyframe.time) || keyframe.time < previous)
                throw std::invalid_argument("Keyframe times must be finite, non-negative, and sorted");
            previous = keyframe.time;
        }
        totalDuration_ = keyframes_.back().time;
    }
}

bool KeyframeAnimation::update(float dt)
{
    validateDeltaTime(dt);
    if (keyframes_.size() < 2 || totalDuration_ <= 0.0f)
    {
        finished_ = true;
        return finished_;
    }

    elapsed_ += dt;
    if (loop_ == LoopMode::None && elapsed_ >= totalDuration_)
    {
        elapsed_ = totalDuration_;
        finished_ = true;
    }
    else if (loop_ == LoopMode::Loop)
    {
        elapsed_ = std::fmod(elapsed_, static_cast<double>(totalDuration_));
    }
    else if (loop_ == LoopMode::PingPong)
    {
        double cycle = static_cast<double>(totalDuration_) * 2;
        elapsed_ = std::fmod(elapsed_, cycle);
    }
    return finished_;
}

void KeyframeAnimation::apply(Node& node)
{
    if (keyframes_.size() < 2)
        return;

    double t = elapsed_;
    if (loop_ == LoopMode::PingPong && t > totalDuration_)
    {
        t = static_cast<double>(totalDuration_) * 2 - t;
    }

    // Find surrounding keyframes
    size_t i = 0;
    for (i = 0; i + 1 < keyframes_.size(); ++i)
    {
        if (keyframes_[i + 1].time >= t)
            break;
    }
    if (i + 1 >= keyframes_.size())
        i = keyframes_.size() - 2;

    float segStart = keyframes_[i].time;
    float segEnd = keyframes_[i + 1].time;
    float localT = (segEnd > segStart) ? static_cast<float>((t - segStart) / (segEnd - segStart)) : 1.0f;
    localT = std::clamp(localT, 0.0f, 1.0f);

    auto& from = keyframes_[i].target;
    auto& to = keyframes_[i + 1].target;

    applyProperties(node, from, to, localT);
}

// --- Animator ---

void Animator::add(std::unique_ptr<Animation> anim)
{
    animations_.push_back(std::move(anim));
}

void Animator::update(float dt)
{
    validateDeltaTime(dt);
    for (auto& anim : animations_)
    {
        anim->update(dt);
    }
}

void Animator::applyTo(Scene& scene)
{
    // Keep completed animations alive until their final state has been
    // applied. Removing them in update() skipped the endpoint frame.
    animations_.erase(std::remove_if(animations_.begin(), animations_.end(),
                                     [&scene](const auto& animation)
                                     {
                                         auto node = scene.findNode(animation->targetNodeId());
                                         if (!node)
                                             return true;
                                         animation->apply(*node);
                                         return animation->isFinished();
                                     }),
                      animations_.end());
}

void Animator::clear()
{
    animations_.clear();
}

} // namespace vectorgl
