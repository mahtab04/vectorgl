#include <cmath>
#include <limits>
#include <vectorgl/color.hpp>

#include "test_utils.hpp"
#include "vectorgl/detail/svg_parser.hpp"

using vectorgl::Color;
constexpr Color (*opaqueHex)(uint32_t) = &Color::hex;
constexpr Color (*alphaHex)(uint32_t, float) = &Color::hex;
static_assert(opaqueHex(0x123456) == alphaHex(0x123456, 1));
static_assert(Color::RebeccaPurple == Color::hex(0x663399));
static_assert(Color::Lime == Color::Green);
static_assert(Color::Aqua == Color::Cyan && Color::Fuchsia == Color::Magenta);
static_assert(Color::Grey == Color::Gray && Color::DarkGrey == Color::DarkGray && Color::LightGrey == Color::LightGray);
static_assert(Color::hex(0x12ABCDEF) == Color::hex(0xABCDEF));
static_assert(Color::lerp(Color::Black, Color::White, .5f) == Color{.5f, .5f, .5f});
static_assert(Color::Blue.withAlpha(.5f).premultiplied() == Color{0, 0, .5f, .5f});
static_assert(Color{-1, 2, .5f, 3}.clamped() == Color{0, 1, .5f, 1});
static_assert(sizeof(Color) == sizeof(float) * 4);

int main()
{
    expect(Color::Green == Color{0, 1, 0} && Color::Gray == Color{.5f, .5f, .5f}, "legacy colors changed");
    expect(Color::DarkGray == Color{.25f, .25f, .25f} && Color::LightGray == Color{.75f, .75f, .75f},
           "legacy gray levels changed");
    expect(Color::Orange == Color{1, .65f, 0}, "legacy orange changed");
    expect(Color::Maroon == Color::hex(0x800000) && Color::LightPink == Color::hex(0xFFB6C1),
           "named red/pink values incorrect");
    expect(Color::MediumSeaGreen == Color::hex(0x3CB371) && Color::LightSteelBlue == Color::hex(0xB0C4DE),
           "named green/blue values incorrect");
    expect(Color::LemonChiffon == Color::hex(0xFFFACD) && Color::BurlyWood == Color::hex(0xDEB887),
           "named yellow/brown values incorrect");
    expect(Color::Transparent == Color{0, 0, 0, 0}, "transparent changed");
    const auto middle = Color::lerpOklab(Color::Black, Color::White, .5f);
    // Midpoint Oklab L=.5 -> linear RGB=.125 -> sRGB approximately .38857286.
    for (float value : {middle.r, middle.g, middle.b})
    {
        expect(std::isfinite(value), "Oklab neutral midpoint must be finite");
        expectNear(value, .38857286f, .0001f, "Oklab neutral midpoint");
    }
    const auto extended = Color::lerpOklab(Color::Black.withAlpha(.2f), Color::White.withAlpha(.8f), 2);
    expectNear(extended.a, 1.4f, .0001f, "Oklab alpha extrapolation must retain legacy behavior");
    expect(extended.r <= 1 && extended.g <= 1 && extended.b <= 1, "Oklab RGB escaped display range");
    const auto alpha = Color::lerpOklab(Color::Red.withAlpha(-2), Color::Blue.withAlpha(0), .5f);
    expectNear(alpha.a, -1, .0001f, "Oklab unexpectedly clamped alpha");
    const auto bounded =
        Color{-std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::nanf(""), .5f}
            .clamped();
    expect(bounded.r == 0 && bounded.g == 1 && std::isnan(bounded.b) && bounded.a == .5f,
           "component clamp nonfinite contract changed");
    using vectorgl::detail::svg::parseColor;
    expect(parseColor("RebeccaPurple") == Color::RebeccaPurple, "SVG new color lookup failed");
    expect(parseColor("LIGHTSTEELBLUE") == Color::LightSteelBlue &&
               parseColor("mediumseagreen") == Color::MediumSeaGreen,
           "SVG family lookup failed");
    expect(parseColor("green") == Color::hex(0x008000) && parseColor("gray") == Color::hex(0x808080) &&
               parseColor("orange") == Color::hex(0xFFA500),
           "SVG CSS values changed to legacy C++ values");
}
