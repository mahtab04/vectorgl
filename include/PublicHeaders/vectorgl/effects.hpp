#pragma once
#include "vectorgl/color.hpp"
#include "vectorgl/path.hpp"

namespace vectorgl
{

// Effect descriptors (also referenced in node.hpp, defined here for clarity)
// These are used in the effects pipeline for FBO-based post-processing

/*! @brief Composable chain of post-processing effects.
 *
 *  Use the fluent builder API to describe the effects to apply.
 *
 *  @code
 *  EffectChain fx;
 *  fx.shadow(4.0f, {2, 2}, Color::Black).blur(2.0f);
 *  @endcode
 */
struct EffectChain
{
    /*! @brief Returns `true` if no effects are configured. */
    bool empty() const
    {
        return !hasShadow && !hasBlur && !hasGlow;
    }

    // Shadow
    bool hasShadow = false;
    float shadowBlur = 0.0f;
    Vec2 shadowOffset{0, 0};
    Color shadowColor{0, 0, 0, 0.25f};

    // Blur
    bool hasBlur = false;
    float blurRadius = 0.0f;

    // Glow
    bool hasGlow = false;
    float glowRadius = 0.0f;
    Color glowColor = Color::White;

    /*! @brief Adds a drop shadow to the chain.
     *  @param[in] blur    Shadow blur radius.
     *  @param[in] offset  Shadow offset.
     *  @param[in] color   Shadow color.
     *  @return Reference to this chain (fluent API).
     */
    EffectChain& shadow(float blur, Vec2 offset, Color color)
    {
        hasShadow = true;
        shadowBlur = blur;
        shadowOffset = offset;
        shadowColor = color;
        return *this;
    }

    /*! @brief Adds a Gaussian blur to the chain.
     *  @param[in] radius  Blur radius in pixels.
     *  @return Reference to this chain.
     */
    EffectChain& blur(float radius)
    {
        hasBlur = true;
        blurRadius = radius;
        return *this;
    }

    /*! @brief Adds an outer glow to the chain.
     *  @param[in] radius  Glow radius in pixels.
     *  @param[in] color   Glow color.
     *  @return Reference to this chain.
     */
    EffectChain& glow(float radius, Color color)
    {
        hasGlow = true;
        glowRadius = radius;
        glowColor = color;
        return *this;
    }
};

} // namespace vectorgl
