#pragma once
#include <cstdint>
#include <vector>

#include "vectorgl/color.hpp"
#include "vectorgl/path.hpp"

namespace vectorgl
{

/*! @brief Type of paint (fill strategy) used for rendering. */
enum class PaintType : uint8_t
{
    Solid,
    LinearGradient,
    RadialGradient,
    Pattern
};

/*! @brief Pattern repetition mode along each axis. */
enum class PatternRepeat : uint8_t
{
    None,
    X,
    Y,
    Both
};

/*! @brief A color stop in a gradient.
 *  @sa Paint::linearGradient, Paint::radialGradient
 */
struct GradientStop
{
    float position; // 0.0 to 1.0
    Color color;
};

/*! @brief Describes how a shape is painted (solid color, gradient, or pattern).
 *
 *  Use the static factory methods to create Paint objects:
 *  @code
 *  auto fill = vectorgl::Paint::solid(Color::Red);
 *  auto grad = vectorgl::Paint::linearGradient({0,0}, {100,0},
 *      {{0.0f, Color::Red}, {1.0f, Color::Blue}});
 *  @endcode
 */
struct Paint
{
    PaintType type = PaintType::Solid;
    Color color{Color::Black};

    // Gradient
    Vec2 gradientStart{0, 0};
    Vec2 gradientEnd{0, 0};
    float innerRadius = 0.0f;
    float outerRadius = 0.0f;
    std::vector<GradientStop> stops;

    // Pattern
    uint32_t patternTexture = 0;
    PatternRepeat patternRepeat = PatternRepeat::Both;

    Paint() = default;
    explicit Paint(Color c) : type(PaintType::Solid), color(c) {}

    /*! @brief Creates a solid-color paint.
     *  @param[in] c  Fill color.
     */
    static Paint solid(Color c)
    {
        Paint p;
        p.type = PaintType::Solid;
        p.color = c;
        return p;
    }

    /*! @brief Creates a linear gradient paint.
     *  @param[in] start  Gradient start point.
     *  @param[in] end    Gradient end point.
     *  @param[in] stops  Color stops along the gradient.
     */
    static Paint linearGradient(Vec2 start, Vec2 end, std::vector<GradientStop> stops)
    {
        Paint p;
        p.type = PaintType::LinearGradient;
        p.gradientStart = start;
        p.gradientEnd = end;
        p.stops = std::move(stops);
        if (!p.stops.empty())
            p.color = p.stops[0].color;
        return p;
    }

    /*! @brief Creates a radial gradient paint.
     *  @param[in] center  Gradient center.
     *  @param[in] inner   Inner radius (start of gradient).
     *  @param[in] outer   Outer radius (end of gradient).
     *  @param[in] stops   Color stops.
     */
    static Paint radialGradient(Vec2 center, float inner, float outer, std::vector<GradientStop> stops)
    {
        Paint p;
        p.type = PaintType::RadialGradient;
        p.gradientStart = center;
        p.innerRadius = inner;
        p.outerRadius = outer;
        p.stops = std::move(stops);
        if (!p.stops.empty())
            p.color = p.stops[0].color;
        return p;
    }

    /*! @brief Creates a texture pattern paint.
     *  @param[in] texture  OpenGL texture ID.
     *  @param[in] repeat   Repetition mode (default: both axes).
     */
    static Paint pattern(uint32_t texture, PatternRepeat repeat = PatternRepeat::Both)
    {
        Paint p;
        p.type = PaintType::Pattern;
        p.patternTexture = texture;
        p.patternRepeat = repeat;
        return p;
    }
};

} // namespace vectorgl
