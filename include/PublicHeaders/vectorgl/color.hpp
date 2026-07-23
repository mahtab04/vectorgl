#pragma once
#include <cmath>
#include <cstdint>

namespace vectorgl
{

/*! @brief RGBA color with float components in [0, 1] range.
 *
 *  Colors can be constructed from float RGBA, 8-bit RGBA, hex values, or by
 *  using the predefined constants (Color::Red, Color::White, etc.).
 *
 *  @code
 *  auto red   = vectorgl::Color::Red;
 *  auto semi  = vectorgl::Color::hex(0xFF8800, 0.5f);
 *  auto blend = vectorgl::Color::lerp(red, semi, 0.5f);
 *  @endcode
 */
struct Color
{
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

    constexpr Color() = default;

    /*! @brief Constructs a color from float RGBA values.
     *  @param[in] r  Red   [0, 1].  @param[in] g  Green [0, 1].
     *  @param[in] b  Blue  [0, 1].  @param[in] a  Alpha [0, 1] (default 1).
     */
    constexpr Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

    /*! @brief Creates a color from 8-bit RGB values (0–255).
     *  @param[in] r  Red.  @param[in] g  Green.  @param[in] b  Blue.
     *  @param[in] a  Alpha [0, 1] (default 1).
     */
    static constexpr Color rgba(uint8_t r, uint8_t g, uint8_t b, float a = 1.0f)
    {
        return {r / 255.0f, g / 255.0f, b / 255.0f, a};
    }

    /*! @brief Creates a color from float RGBA values.
     *  @param[in] r  Red.  @param[in] g  Green.  @param[in] b  Blue.  @param[in] a  Alpha.
     */
    static constexpr Color rgba(float r, float g, float b, float a)
    {
        return {r, g, b, a};
    }

    /*! @brief Creates a color from a 24-bit hex value (0xRRGGBB), alpha = 1. */
    static constexpr Color hex(uint32_t h)
    {
        return rgba(static_cast<uint8_t>((h >> 16) & 0xFF), static_cast<uint8_t>((h >> 8) & 0xFF),
                    static_cast<uint8_t>(h & 0xFF), 1.0f);
    }

    /*! @brief Creates a color from a 24-bit hex value with explicit alpha.
     *  @param[in] h  RGB value (0xRRGGBB).
     *  @param[in] a  Alpha [0, 1].
     */
    static constexpr Color hex(uint32_t h, float a)
    {
        return rgba(static_cast<uint8_t>((h >> 16) & 0xFF), static_cast<uint8_t>((h >> 8) & 0xFF),
                    static_cast<uint8_t>(h & 0xFF), a);
    }

    /*! @brief Linearly interpolates between two colors in RGB space.
     *  @param[in] a  Start color.  @param[in] b  End color.
     *  @param[in] t  Interpolation factor [0, 1].
     */
    static Color lerp(Color a, Color b, float t)
    {
        return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
    }

    /*! @brief Perceptually uniform interpolation in Oklab color space.
     *  @param[in] a  Start color.  @param[in] b  End color.
     *  @param[in] t  Interpolation factor [0, 1].
     */
    static Color lerpOklab(Color a, Color b, float t);

    /*! @brief Returns a copy of this color with a new alpha value.
     *  @param[in] newAlpha  Alpha value [0, 1].
     */
    Color withAlpha(float newAlpha) const
    {
        return {r, g, b, newAlpha};
    }
    /*! @brief Returns a premultiplied-alpha version of this color. */
    Color premultiplied() const
    {
        return {r * a, g * a, b * a, a};
    }

    bool operator==(const Color& o) const
    {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
    bool operator!=(const Color& o) const
    {
        return !(*this == o);
    }

    static const Color Black;
    static const Color White;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Yellow;
    static const Color Cyan;
    static const Color Magenta;
    static const Color Transparent;
    static const Color Gray;
    static const Color DarkGray;
    static const Color LightGray;
    static const Color Orange;
};

inline constexpr Color Color::Black{0.0f, 0.0f, 0.0f, 1.0f};
inline constexpr Color Color::White{1.0f, 1.0f, 1.0f, 1.0f};
inline constexpr Color Color::Red{1.0f, 0.0f, 0.0f, 1.0f};
inline constexpr Color Color::Green{0.0f, 1.0f, 0.0f, 1.0f};
inline constexpr Color Color::Blue{0.0f, 0.0f, 1.0f, 1.0f};
inline constexpr Color Color::Yellow{1.0f, 1.0f, 0.0f, 1.0f};
inline constexpr Color Color::Cyan{0.0f, 1.0f, 1.0f, 1.0f};
inline constexpr Color Color::Magenta{1.0f, 0.0f, 1.0f, 1.0f};
inline constexpr Color Color::Transparent{0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr Color Color::Gray{0.5f, 0.5f, 0.5f, 1.0f};
inline constexpr Color Color::DarkGray{0.25f, 0.25f, 0.25f, 1.0f};
inline constexpr Color Color::LightGray{0.75f, 0.75f, 0.75f, 1.0f};
inline constexpr Color Color::Orange{1.0f, 0.65f, 0.0f, 1.0f};

// Oklab implementation (inline for header-only convenience)
namespace detail
{
inline float srgbToLinear(float x)
{
    return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
}
inline float linearToSrgb(float x)
{
    return x <= 0.0031308f ? x * 12.92f : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}
struct OklabColor
{
    float L, a, b;
};

inline OklabColor rgbToOklab(float r, float g, float b)
{
    float lr = srgbToLinear(r), lg = srgbToLinear(g), lb = srgbToLinear(b);
    float l = 0.4122214708f * lr + 0.5363325363f * lg + 0.0514459929f * lb;
    float m = 0.2119034982f * lr + 0.6806995451f * lg + 0.1073969566f * lb;
    float s = 0.0883024619f * lr + 0.2817188376f * lg + 0.6299787005f * lb;
    float l_ = std::cbrt(l), m_ = std::cbrt(m), s_ = std::cbrt(s);
    return {0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
            1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
            0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_};
}

inline void oklabToRgb(OklabColor ok, float& r, float& g, float& b)
{
    float l_ = ok.L + 0.3963377774f * ok.a + 0.2158037573f * ok.b;
    float m_ = ok.L - 0.1055613458f * ok.a - 0.0638541728f * ok.b;
    float s_ = ok.L - 0.0894841775f * ok.a - 1.2914855480f * ok.b;
    float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    r = linearToSrgb(+4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s);
    g = linearToSrgb(-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s);
    b = linearToSrgb(-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s);
}
} // namespace detail

inline Color Color::lerpOklab(Color ca, Color cb, float t)
{
    auto a = detail::rgbToOklab(ca.r, ca.g, ca.b);
    auto b = detail::rgbToOklab(cb.r, cb.g, cb.b);
    detail::OklabColor mixed{a.L + (b.L - a.L) * t, a.a + (b.a - a.a) * t, a.b + (b.b - a.b) * t};
    float ro, go, bo;
    detail::oklabToRgb(mixed, ro, go, bo);
    // Clamp to [0, 1]
    auto clamp01 = [](float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); };
    return {clamp01(ro), clamp01(go), clamp01(bo), ca.a + (cb.a - ca.a) * t};
}

} // namespace vectorgl
