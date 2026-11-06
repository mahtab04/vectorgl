#pragma once

#include <cmath>
#include <cstdint>

namespace vectorgl
{

/*!
 * @brief RGBA color with floating-point components.
 *
 * Colors use straight (non-premultiplied) alpha.
 * Named constants and Oklab interpolation use sRGB-encoded components.
 * Construction does not clamp components; clamped() does so explicitly.
 *
 * @code
 * auto red = Color::Red;
 * auto orange = Color::hex(0xFF8800);
 * auto transparent = Color::Blue.withAlpha(0.5f);
 * auto gradient = Color::lerpOklab(Color::Red, Color::Blue, 0.5f);
 * @endcode
 */
struct Color
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    // Constructors
    constexpr Color() = default;

    constexpr Color(float red, float green, float blue, float alpha = 1.0f) : r(red), g(green), b(blue), a(alpha) {}

    /*! Creates RGB from bytes; alpha is a float, normally in [0, 1]. */
    static constexpr Color rgba(uint8_t red, uint8_t green, uint8_t blue, float alpha = 1.0f)
    {
        return {red / 255.0f, green / 255.0f, blue / 255.0f, alpha};
    }

    static constexpr Color rgba(float red, float green, float blue, float alpha)
    {
        return {red, green, blue, alpha};
    }

    /*! Creates opaque RGB from 0xRRGGBB. High bits are ignored. */
    static constexpr Color hex(uint32_t value)
    {
        return hex(value, 1.0f);
    }

    /*! Creates RGB from 0xRRGGBB with explicit floating-point alpha. */
    static constexpr Color hex(uint32_t value, float alpha)
    {
        return rgba(static_cast<uint8_t>((value >> 16) & 0xFF), static_cast<uint8_t>((value >> 8) & 0xFF),
                    static_cast<uint8_t>(value & 0xFF), alpha);
    }

    /*! Component interpolation without clamping, including alpha. */
    static constexpr Color lerp(Color start, Color end, float t)
    {
        return {start.r + (end.r - start.r) * t, start.g + (end.g - start.g) * t, start.b + (end.b - start.b) * t,
                start.a + (end.a - start.a) * t};
    }

    /*! Oklab interpolation for sRGB colors. RGB is clipped to [0,1]; alpha is
     * interpolated independently without clamping, preserving the existing API.
     * Use t in [0,1] for a blend; extrapolation is also supported.
     */
    static Color lerpOklab(Color start, Color end, float t);

    // Alpha utilities
    constexpr Color withAlpha(float alpha) const
    {
        return {r, g, b, alpha};
    }

    constexpr Color premultiplied() const
    {
        return {r * a, g * a, b * a, a};
    }

    /*! Explicit component clamp. Infinities are bounded; NaNs propagate. */
    constexpr Color clamped() const
    {
        const auto clamp = [](float value) constexpr { return value < 0 ? 0.0f : value > 1 ? 1.0f : value; };
        return {clamp(r), clamp(g), clamp(b), clamp(a)};
    }

    // Comparison
    constexpr bool operator==(const Color& other) const
    {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    constexpr bool operator!=(const Color& other) const
    {
        return !(*this == other);
    }

    // Basic colors
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
    static const Color Aqua;
    static const Color Fuchsia;
    static const Color Maroon;
    static const Color Grey;
    static const Color DarkGrey;
    static const Color LightGrey;

    // Reds and pinks
    static const Color DarkRed;
    static const Color Crimson;
    static const Color Firebrick;
    static const Color IndianRed;
    static const Color Salmon;
    static const Color LightSalmon;
    static const Color Coral;
    static const Color Tomato;
    static const Color HotPink;
    static const Color DeepPink;
    static const Color LightPink;
    static const Color Pink;

    // Oranges and yellows
    static const Color DarkOrange;
    static const Color OrangeRed;
    static const Color Gold;
    static const Color Goldenrod;
    static const Color Khaki;
    static const Color LightYellow;
    static const Color LemonChiffon;
    static const Color Amber;

    // Greens
    static const Color DarkGreen;
    static const Color ForestGreen;
    static const Color Lime;
    static const Color LimeGreen;
    static const Color LightGreen;
    static const Color SeaGreen;
    static const Color MediumSeaGreen;
    static const Color SpringGreen;
    static const Color Olive;
    static const Color OliveDrab;
    static const Color YellowGreen;
    static const Color Chartreuse;
    static const Color Teal;
    static const Color MintGreen;
    static const Color Emerald;

    // Blues and cyans
    static const Color Navy;
    static const Color DarkBlue;
    static const Color MediumBlue;
    static const Color RoyalBlue;
    static const Color DodgerBlue;
    static const Color DeepSkyBlue;
    static const Color SkyBlue;
    static const Color LightSkyBlue;
    static const Color LightBlue;
    static const Color SteelBlue;
    static const Color LightSteelBlue;
    static const Color CornflowerBlue;
    static const Color MidnightBlue;
    static const Color Turquoise;
    static const Color DarkTurquoise;
    static const Color Aquamarine;
    static const Color Indigo;

    // Purples and violets
    static const Color Purple;
    static const Color DarkViolet;
    static const Color BlueViolet;
    static const Color Violet;
    static const Color Orchid;
    static const Color MediumOrchid;
    static const Color Plum;
    static const Color Lavender;
    static const Color RebeccaPurple;
    static const Color SlateBlue;
    static const Color MediumSlateBlue;

    // Browns and earth tones
    static const Color Brown;
    static const Color SaddleBrown;
    static const Color Sienna;
    static const Color Chocolate;
    static const Color Peru;
    static const Color Tan;
    static const Color SandyBrown;
    static const Color BurlyWood;
    static const Color Wheat;
    static const Color Beige;
    static const Color Ivory;

    // Grayscale and neutrals
    static const Color DimGray;
    static const Color SlateGray;
    static const Color LightSlateGray;
    static const Color Gainsboro;
    static const Color Silver;
    static const Color Snow;
    static const Color WhiteSmoke;
    static const Color GhostWhite;
    static const Color AliceBlue;
    static const Color AntiqueWhite;
    static const Color Linen;

    // Suggested dark UI palette (independent of ui::Theme defaults).
    static const Color Primary;
    static const Color Secondary;
    static const Color Success;
    static const Color Warning;
    static const Color Error;
    static const Color Info;
    static const Color Background;
    static const Color Surface;
    static const Color Border;
    static const Color TextPrimary;
    static const Color TextSecondary;
};

// ============================================================
// BASIC COLORS
// ============================================================

inline constexpr Color Color::Black = Color::hex(0x000000);
inline constexpr Color Color::White = Color::hex(0xFFFFFF);
inline constexpr Color Color::Red = Color::hex(0xFF0000);
inline constexpr Color Color::Green = Color::hex(0x00FF00);
inline constexpr Color Color::Blue = Color::hex(0x0000FF);
inline constexpr Color Color::Yellow = Color::hex(0xFFFF00);
inline constexpr Color Color::Cyan = Color::hex(0x00FFFF);
inline constexpr Color Color::Magenta = Color::hex(0xFF00FF);
inline constexpr Color Color::Transparent = Color::hex(0x000000, 0.0f);
inline constexpr Color Color::Gray{0.5f, 0.5f, 0.5f};
inline constexpr Color Color::DarkGray{0.25f, 0.25f, 0.25f};
inline constexpr Color Color::LightGray{0.75f, 0.75f, 0.75f};
inline constexpr Color Color::Orange{1.0f, 0.65f, 0.0f};
inline constexpr Color Color::Aqua = Color::Cyan;
inline constexpr Color Color::Fuchsia = Color::Magenta;
inline constexpr Color Color::Maroon = Color::hex(0x800000);
inline constexpr Color Color::Grey = Color::Gray;
inline constexpr Color Color::DarkGrey = Color::DarkGray;
inline constexpr Color Color::LightGrey = Color::LightGray;

// ============================================================
// REDS AND PINKS
// ============================================================

inline constexpr Color Color::DarkRed = Color::hex(0x8B0000);
inline constexpr Color Color::Crimson = Color::hex(0xDC143C);
inline constexpr Color Color::Firebrick = Color::hex(0xB22222);
inline constexpr Color Color::IndianRed = Color::hex(0xCD5C5C);
inline constexpr Color Color::Salmon = Color::hex(0xFA8072);
inline constexpr Color Color::LightSalmon = Color::hex(0xFFA07A);
inline constexpr Color Color::Coral = Color::hex(0xFF7F50);
inline constexpr Color Color::Tomato = Color::hex(0xFF6347);
inline constexpr Color Color::HotPink = Color::hex(0xFF69B4);
inline constexpr Color Color::DeepPink = Color::hex(0xFF1493);
inline constexpr Color Color::LightPink = Color::hex(0xFFB6C1);
inline constexpr Color Color::Pink = Color::hex(0xFFC0CB);

// ============================================================
// ORANGES AND YELLOWS
// ============================================================

inline constexpr Color Color::DarkOrange = Color::hex(0xFF8C00);
inline constexpr Color Color::OrangeRed = Color::hex(0xFF4500);
inline constexpr Color Color::Gold = Color::hex(0xFFD700);
inline constexpr Color Color::Goldenrod = Color::hex(0xDAA520);
inline constexpr Color Color::Khaki = Color::hex(0xF0E68C);
inline constexpr Color Color::LightYellow = Color::hex(0xFFFFE0);
inline constexpr Color Color::LemonChiffon = Color::hex(0xFFFACD);
inline constexpr Color Color::Amber = Color::hex(0xFFBF00);

// ============================================================
// GREENS
// ============================================================

inline constexpr Color Color::DarkGreen = Color::hex(0x006400);
inline constexpr Color Color::ForestGreen = Color::hex(0x228B22);
inline constexpr Color Color::Lime = Color::hex(0x00FF00);
inline constexpr Color Color::LimeGreen = Color::hex(0x32CD32);
inline constexpr Color Color::LightGreen = Color::hex(0x90EE90);
inline constexpr Color Color::SeaGreen = Color::hex(0x2E8B57);
inline constexpr Color Color::MediumSeaGreen = Color::hex(0x3CB371);
inline constexpr Color Color::SpringGreen = Color::hex(0x00FF7F);
inline constexpr Color Color::Olive = Color::hex(0x808000);
inline constexpr Color Color::OliveDrab = Color::hex(0x6B8E23);
inline constexpr Color Color::YellowGreen = Color::hex(0x9ACD32);
inline constexpr Color Color::Chartreuse = Color::hex(0x7FFF00);
inline constexpr Color Color::Teal = Color::hex(0x008080);
inline constexpr Color Color::MintGreen = Color::hex(0x98FF98);
inline constexpr Color Color::Emerald = Color::hex(0x50C878);

// ============================================================
// BLUES AND CYANS
// ============================================================

inline constexpr Color Color::Navy = Color::hex(0x000080);
inline constexpr Color Color::DarkBlue = Color::hex(0x00008B);
inline constexpr Color Color::MediumBlue = Color::hex(0x0000CD);
inline constexpr Color Color::RoyalBlue = Color::hex(0x4169E1);
inline constexpr Color Color::DodgerBlue = Color::hex(0x1E90FF);
inline constexpr Color Color::DeepSkyBlue = Color::hex(0x00BFFF);
inline constexpr Color Color::SkyBlue = Color::hex(0x87CEEB);
inline constexpr Color Color::LightSkyBlue = Color::hex(0x87CEFA);
inline constexpr Color Color::LightBlue = Color::hex(0xADD8E6);
inline constexpr Color Color::SteelBlue = Color::hex(0x4682B4);
inline constexpr Color Color::LightSteelBlue = Color::hex(0xB0C4DE);
inline constexpr Color Color::CornflowerBlue = Color::hex(0x6495ED);
inline constexpr Color Color::MidnightBlue = Color::hex(0x191970);
inline constexpr Color Color::Turquoise = Color::hex(0x40E0D0);
inline constexpr Color Color::DarkTurquoise = Color::hex(0x00CED1);
inline constexpr Color Color::Aquamarine = Color::hex(0x7FFFD4);
inline constexpr Color Color::Indigo = Color::hex(0x4B0082);

// ============================================================
// PURPLES AND VIOLETS
// ============================================================

inline constexpr Color Color::Purple = Color::hex(0x800080);
inline constexpr Color Color::DarkViolet = Color::hex(0x9400D3);
inline constexpr Color Color::BlueViolet = Color::hex(0x8A2BE2);
inline constexpr Color Color::Violet = Color::hex(0xEE82EE);
inline constexpr Color Color::Orchid = Color::hex(0xDA70D6);
inline constexpr Color Color::MediumOrchid = Color::hex(0xBA55D3);
inline constexpr Color Color::Plum = Color::hex(0xDDA0DD);
inline constexpr Color Color::Lavender = Color::hex(0xE6E6FA);
inline constexpr Color Color::RebeccaPurple = Color::hex(0x663399);
inline constexpr Color Color::SlateBlue = Color::hex(0x6A5ACD);
inline constexpr Color Color::MediumSlateBlue = Color::hex(0x7B68EE);

// ============================================================
// BROWNS AND EARTH TONES
// ============================================================

inline constexpr Color Color::Brown = Color::hex(0xA52A2A);
inline constexpr Color Color::SaddleBrown = Color::hex(0x8B4513);
inline constexpr Color Color::Sienna = Color::hex(0xA0522D);
inline constexpr Color Color::Chocolate = Color::hex(0xD2691E);
inline constexpr Color Color::Peru = Color::hex(0xCD853F);
inline constexpr Color Color::Tan = Color::hex(0xD2B48C);
inline constexpr Color Color::SandyBrown = Color::hex(0xF4A460);
inline constexpr Color Color::BurlyWood = Color::hex(0xDEB887);
inline constexpr Color Color::Wheat = Color::hex(0xF5DEB3);
inline constexpr Color Color::Beige = Color::hex(0xF5F5DC);
inline constexpr Color Color::Ivory = Color::hex(0xFFFFF0);

// ============================================================
// GRAYSCALE AND NEUTRALS
// ============================================================

inline constexpr Color Color::DimGray = Color::hex(0x696969);
inline constexpr Color Color::SlateGray = Color::hex(0x708090);
inline constexpr Color Color::LightSlateGray = Color::hex(0x778899);
inline constexpr Color Color::Gainsboro = Color::hex(0xDCDCDC);
inline constexpr Color Color::Silver = Color::hex(0xC0C0C0);
inline constexpr Color Color::Snow = Color::hex(0xFFFAFA);
inline constexpr Color Color::WhiteSmoke = Color::hex(0xF5F5F5);
inline constexpr Color Color::GhostWhite = Color::hex(0xF8F8FF);
inline constexpr Color Color::AliceBlue = Color::hex(0xF0F8FF);
inline constexpr Color Color::AntiqueWhite = Color::hex(0xFAEBD7);
inline constexpr Color Color::Linen = Color::hex(0xFAF0E6);

// ============================================================
// SUGGESTED DARK UI PALETTE
// ============================================================

inline constexpr Color Color::Primary = Color::hex(0x6366F1);
inline constexpr Color Color::Secondary = Color::hex(0x8B5CF6);
inline constexpr Color Color::Success = Color::hex(0x22C55E);
inline constexpr Color Color::Warning = Color::hex(0xF59E0B);
inline constexpr Color Color::Error = Color::hex(0xEF4444);
inline constexpr Color Color::Info = Color::hex(0x06B6D4);
inline constexpr Color Color::Background = Color::hex(0x0F172A);
inline constexpr Color Color::Surface = Color::hex(0x1E293B);
inline constexpr Color Color::Border = Color::hex(0x334155);
inline constexpr Color Color::TextPrimary = Color::hex(0xF8FAFC);
inline constexpr Color Color::TextSecondary = Color::hex(0x94A3B8);

// ============================================================
// OKLAB COLOR CONVERSION
// ============================================================

namespace detail
{

inline float srgbToLinear(float value)
{
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

inline float linearToSrgb(float value)
{
    return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}

struct OklabColor
{
    float L;
    float a;
    float b;
};

inline OklabColor rgbToOklab(float r, float g, float b)
{
    const float lr = srgbToLinear(r);
    const float lg = srgbToLinear(g);
    const float lb = srgbToLinear(b);

    const float l = 0.4122214708f * lr + 0.5363325363f * lg + 0.0514459929f * lb;

    const float m = 0.2119034982f * lr + 0.6806995451f * lg + 0.1073969566f * lb;

    const float s = 0.0883024619f * lr + 0.2817188376f * lg + 0.6299787005f * lb;

    const float l_ = std::cbrt(l);
    const float m_ = std::cbrt(m);
    const float s_ = std::cbrt(s);

    return {0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,

            1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,

            0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_};
}

inline void oklabToRgb(OklabColor color, float& r, float& g, float& b)
{
    const float l_ = color.L + 0.3963377774f * color.a + 0.2158037573f * color.b;

    const float m_ = color.L - 0.1055613458f * color.a - 0.0638541728f * color.b;

    const float s_ = color.L - 0.0894841775f * color.a - 1.2914855480f * color.b;

    const float l = l_ * l_ * l_;
    const float m = m_ * m_ * m_;
    const float s = s_ * s_ * s_;

    r = linearToSrgb(4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s);

    g = linearToSrgb(-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s);

    b = linearToSrgb(-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s);
}

} // namespace detail

// ============================================================
// OKLAB INTERPOLATION
// ============================================================

inline Color Color::lerpOklab(Color start, Color end, float t)
{
    const auto a = detail::rgbToOklab(start.r, start.g, start.b);

    const auto b = detail::rgbToOklab(end.r, end.g, end.b);

    const detail::OklabColor mixed{a.L + (b.L - a.L) * t, a.a + (b.a - a.a) * t, a.b + (b.b - a.b) * t};

    float red, green, blue;

    detail::oklabToRgb(mixed, red, green, blue);

    auto result = Color{red, green, blue}.clamped();
    result.a = start.a + (end.a - start.a) * t;
    return result;
}

} // namespace vectorgl
