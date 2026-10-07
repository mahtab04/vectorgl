#include "vectorgl/animator.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "vectorgl/node.hpp"
#include "vectorgl/scene.hpp"

namespace vectorgl
{

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
    nodeId_ = nodeId;
}

bool TweenAnimation::update(float dt)
{
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
        while (elapsed_ >= duration_)
            elapsed_ -= duration_;
    }
    else if (loop_ == LoopMode::PingPong)
    {
        float cycle = duration_ * 2;
        while (elapsed_ >= cycle)
            elapsed_ -= cycle;
        reverse_ = elapsed_ > duration_;
    }
    return finished_;
}

void TweenAnimation::apply(Node& node)
{
    float rawT = duration_ > 0 ? elapsed_ / duration_ : 1.0f;
    if (loop_ == LoopMode::PingPong && reverse_)
    {
        rawT = 2.0f - (elapsed_ / duration_);
    }
    rawT = std::clamp(rawT, 0.0f, 1.0f);
    float t = evalEasing(easing_, rawT);

    uint32_t mask = to_.mask;
    {
        float px = node.position().x;
        float py = node.position().y;
        if (mask & AnimTarget::PosX)
            px = from_.x + (to_.x - from_.x) * t;
        if (mask & AnimTarget::PosY)
            py = from_.y + (to_.y - from_.y) * t;
        if (mask & (AnimTarget::PosX | AnimTarget::PosY))
            node.setPosition(px, py);
    }
    if (mask & AnimTarget::Opacity)
        node.setOpacity(from_.opacity + (to_.opacity - from_.opacity) * t);
    if (mask & AnimTarget::Fill)
        node.setFill(Color::lerpOklab(from_.fill, to_.fill, t));
    if (mask & AnimTarget::Rotation)
        node.setRotation(from_.rotation + (to_.rotation - from_.rotation) * t);
    if (mask & AnimTarget::ScaleX)
    {
        float sy = (mask & AnimTarget::ScaleY) ? from_.scaleY + (to_.scaleY - from_.scaleY) * t : node.scale().y;
        node.setScale(from_.scaleX + (to_.scaleX - from_.scaleX) * t, sy);
    }
    else if (mask & AnimTarget::ScaleY)
    {
        node.setScale(node.scale().x, from_.scaleY + (to_.scaleY - from_.scaleY) * t);
    }
}

// --- SpringAnimation ---

SpringAnimation::SpringAnimation(uint32_t nodeId, AnimTarget target, float stiffness, float damping)
    : target_(target), stiffness_(stiffness), damping_(damping)
{
    nodeId_ = nodeId;
}

bool SpringAnimation::update(float dt)
{
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
        totalDuration_ = keyframes_.back().time;
    }
}

bool KeyframeAnimation::update(float dt)
{
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
        while (elapsed_ >= totalDuration_)
            elapsed_ -= totalDuration_;
    }
    else if (loop_ == LoopMode::PingPong)
    {
        float cycle = totalDuration_ * 2;
        while (elapsed_ >= cycle)
            elapsed_ -= cycle;
    }
    return finished_;
}

void KeyframeAnimation::apply(Node& node)
{
    if (keyframes_.size() < 2)
        return;

    float t = elapsed_;
    if (loop_ == LoopMode::PingPong && t > totalDuration_)
    {
        t = totalDuration_ * 2 - t;
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
    float localT = (segEnd > segStart) ? (t - segStart) / (segEnd - segStart) : 1.0f;
    localT = std::clamp(localT, 0.0f, 1.0f);

    auto& from = keyframes_[i].target;
    auto& to = keyframes_[i + 1].target;

    if (to.mask & AnimTarget::Opacity)
        node.setOpacity(from.opacity + (to.opacity - from.opacity) * localT);
    if (to.mask & (AnimTarget::ScaleX | AnimTarget::ScaleY))
    {
        float sx = node.scale().x;
        float sy = node.scale().y;
        if (to.mask & AnimTarget::ScaleX)
            sx = from.scaleX + (to.scaleX - from.scaleX) * localT;
        if (to.mask & AnimTarget::ScaleY)
            sy = from.scaleY + (to.scaleY - from.scaleY) * localT;
        node.setScale(sx, sy);
    }
    {
        float px = node.position().x;
        float py = node.position().y;
        if (to.mask & AnimTarget::PosX)
            px = from.x + (to.x - from.x) * localT;
        if (to.mask & AnimTarget::PosY)
            py = from.y + (to.y - from.y) * localT;
        if (to.mask & (AnimTarget::PosX | AnimTarget::PosY))
            node.setPosition(px, py);
    }
}

// --- Animator ---

void Animator::add(std::unique_ptr<Animation> anim)
{
    animations_.push_back(std::move(anim));
}

void Animator::update(float dt)
{
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
