#pragma once
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "vectorgl/color.hpp"
#include "vectorgl/path.hpp"

namespace vectorgl
{

class Node;

/*! @brief Easing function type for tween animations.
 *
 *  Each value selects a standard easing curve.  Pass to TweenAnimation to
 *  control the interpolation rate.
 *
 *  @sa evalEasing, TweenAnimation
 */
enum class Ease : uint8_t
{
    Linear,
    InQuad,
    OutQuad,
    InOutQuad,
    InCubic,
    OutCubic,
    InOutCubic,
    InQuart,
    OutQuart,
    InOutQuart,
    InExpo,
    OutExpo,
    InOutExpo,
    InBack,
    OutBack,
    InOutBack,
    InElastic,
    OutElastic,
    InOutElastic,
    InBounce,
    OutBounce,
    InOutBounce
};

/*! @brief Evaluates an easing curve at a given normalized time.
 *  @param[in] ease  Easing type.
 *  @param[in] t     Normalized time [0, 1].
 *  @return Eased value.
 */
float evalEasing(Ease ease, float t);

/*! @brief Animation loop behavior. */
enum class LoopMode : uint8_t
{
    None,
    Loop,
    PingPong
};

/*! @brief Snapshot of animatable properties on a Node.
 *
 *  Set the @c mask bitfield to indicate which properties are active.
 *  Only masked properties participate in interpolation.
 */
struct AnimTarget
{
    float x = 0, y = 0;
    float width = 0, height = 0;
    float rotation = 0;
    float scaleX = 1, scaleY = 1;
    float opacity = 1;
    Color fill = Color::Transparent;
    Color stroke = Color::Transparent;
    // Flags for which properties are set
    uint32_t mask = 0;
    enum Prop : uint32_t
    {
        PosX = 1,
        PosY = 2,
        Width = 4,
        Height = 8,
        Rotation = 16,
        ScaleX = 32,
        ScaleY = 64,
        Opacity = 128,
        Fill = 256,
        Stroke = 512
    };
};

/*! @brief Abstract base class for all animation types.
 *
 *  Subclasses implement update() (advance time) and apply() (write
 *  interpolated values to a Node).
 *
 *  @sa TweenAnimation, SpringAnimation, KeyframeAnimation
 */
class Animation
{
public:
    virtual ~Animation() = default;
    virtual bool update(float dt) = 0; // returns true when finished
    virtual void apply(Node& node) = 0;
    bool isFinished() const
    {
        return finished_;
    }
    uint32_t targetNodeId() const
    {
        return nodeId_;
    }

protected:
    uint32_t nodeId_ = 0;
    bool finished_ = false;
};

/*! @brief Duration-based tween animation with configurable easing.
 *
 *  @code
 *  AnimTarget from, to;
 *  from.x = 0; from.mask = AnimTarget::PosX;
 *  to.x = 200; to.mask = AnimTarget::PosX;
 *  scene.animator().emplace<TweenAnimation>(
 *      node->id(), from, to, 1.0f, Ease::OutCubic);
 *  @endcode
 */
class TweenAnimation : public Animation
{
public:
    /*! @brief Constructs a tween animation.
     *  @param[in] nodeId    Target node ID.
     *  @param[in] from      Start property state.
     *  @param[in] to        End property state.
     *  @param[in] duration  Duration in seconds.
     *  @param[in] easing    Easing function.
     *  @param[in] loop      Loop behaviour (default: none).
     */
    TweenAnimation(uint32_t nodeId, AnimTarget from, AnimTarget to, float duration, Ease easing,
                   LoopMode loop = LoopMode::None);

    bool update(float dt) override;
    void apply(Node& node) override;

private:
    AnimTarget from_, to_;
    float duration_;
    float elapsed_ = 0.0f;
    Ease easing_;
    LoopMode loop_;
    bool reverse_ = false;
};

/*! @brief Physically-based spring animation (critically-damped).
 *
 *  Converges smoothly towards the target without overshoot when
 *  damping is set correctly.
 */
class SpringAnimation : public Animation
{
public:
    /*! @brief Constructs a spring animation.
     *  @param[in] nodeId     Target node ID.
     *  @param[in] target     Goal property state.
     *  @param[in] stiffness  Spring stiffness (default 200).
     *  @param[in] damping    Damping coefficient (default 20).
     */
    SpringAnimation(uint32_t nodeId, AnimTarget target, float stiffness = 200.0f, float damping = 20.0f);

    bool update(float dt) override;
    void apply(Node& node) override;

private:
    AnimTarget target_;
    float stiffness_, damping_;
    float velocityX_ = 0, velocityY_ = 0;
    float currentX_ = 0, currentY_ = 0;
    bool initialized_ = false;
};

// Keyframe animation
struct Keyframe
{
    float time;
    AnimTarget target;
};

/*! @brief Keyframe-based animation with linear interpolation between stops. */
class KeyframeAnimation : public Animation
{
public:
    /*! @brief Constructs a keyframe animation.
     *  @param[in] nodeId     Target node ID.
     *  @param[in] keyframes  Sorted list of keyframes.
     *  @param[in] loop       Loop behaviour.
     */
    KeyframeAnimation(uint32_t nodeId, std::vector<Keyframe> keyframes, LoopMode loop = LoopMode::None);

    bool update(float dt) override;
    void apply(Node& node) override;

private:
    std::vector<Keyframe> keyframes_;
    float elapsed_ = 0.0f;
    float totalDuration_ = 0.0f;
    LoopMode loop_;
};

/*! @brief Animation engine that owns and updates all active animations.
 *
 *  Attach animations via add(), then call update() + applyTo() each frame.
 */
class Animator
{
public:
    /*! @brief Adds an animation to the engine (takes ownership).
     *  @param[in] anim  Animation to add.
     */
    void add(std::unique_ptr<Animation> anim);

    /*! @brief Constructs an animation in place and adds it to the engine.
     *  @tparam T        Concrete animation type derived from Animation.
     *  @param[in] args  Constructor arguments forwarded to @p T.
     *  @return Reference to the stored animation.
     */
    template <typename T, typename... Args> T& emplace(Args&&... args)
    {
        static_assert(std::is_base_of_v<Animation, T>, "Animator::emplace requires an Animation-derived type");

        auto anim = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *anim;
        add(std::move(anim));
        return ref;
    }

    /*! @brief Advances all animations by the given delta time.
     *  @param[in] dt  Time step in seconds.
     */
    void update(float dt);

    /*! @brief Applies current animation state to the nodes in a scene.
     *  Animations whose target is no longer in the scene are removed.
     *  @param[in,out] scene  The scene whose nodes are updated.
     */
    void applyTo(class Scene& scene);

    /*! @brief Removes all animations. */
    void clear();

    /*! @brief Returns `true` if any animations are still running. */
    bool hasActiveAnimations() const
    {
        return !animations_.empty();
    }

private:
    std::vector<std::unique_ptr<Animation>> animations_;
};

} // namespace vectorgl
