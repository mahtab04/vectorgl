# Color palette and interpolation

`<vectorgl/color.hpp>` provides 115 named constants, including the existing basic
colors, additional hue families, common aliases, and a suggested dark UI palette.
Definitions are inline constexpr; no runtime palette registry or extra dependency
is required.

```cpp
constexpr auto accent = vectorgl::Color::RebeccaPurple;
constexpr auto overlay = vectorgl::Color::Coral.withAlpha(0.5f);
constexpr auto bounded = vectorgl::Color{1.2f, -0.1f, 0.4f, 2.0f}.clamped();
auto blend = vectorgl::Color::lerpOklab(accent, vectorgl::Color::DodgerBlue, 0.5f);
```

RGB constants use sRGB-encoded components and straight alpha. Construction,
`hex`, `rgba`, `withAlpha`, and ordinary `lerp` do not implicitly clamp values.
Byte RGB factories take floating-point alpha, not an alpha byte. Both original
`hex(uint32_t)` and `hex(uint32_t, float)` overloads remain available, including for
function-pointer callers. The high byte of the hex input is ignored.

`clamped()` explicitly bounds all four components to [0,1], including infinities;
NaNs propagate. `premultiplied()` is a representation helper for code that expects
premultiplied channels; normal Canvas Color inputs use straight alpha. These helpers
do not configure the framebuffer color space.

## Names and compatibility

The standard named values were checked against the
[W3C CSS named-color table](https://www.w3.org/TR/css-color-4/#named-colors).
Names such as RebeccaPurple, MediumSeaGreen, LightSteelBlue, and LemonChiffon use
their published RGB values. Aqua aliases Cyan, Fuchsia aliases Magenta, and Maroon
is `#800000`. Grey/DarkGrey/LightGrey alias the corresponding legacy Gray names.

Existing library constants retain their original floating-point values, including
several historical differences from CSS/SVG keywords:

| C++ constant | Existing RGB value | Corresponding SVG/CSS keyword |
|---|---|---|
| Green | `(0, 1, 0)` | `green` = `#008000`; `lime` = `#00FF00` |
| Gray / Grey | `(0.5, 0.5, 0.5)` | `gray` / `grey` = `#808080` |
| DarkGray / DarkGrey | `(0.25, 0.25, 0.25)` | `darkgray` / `darkgrey` = `#A9A9A9` |
| LightGray / LightGrey | `(0.75, 0.75, 0.75)` | `lightgray` / `lightgrey` = `#D3D3D3` |
| Orange | `(1, 0.65, 0)` | `orange` = `#FFA500` |

The SVG parser preserves those CSS keyword values and gains 38 standard names
matching the expanded palette. Nonstandard conveniences such as Amber, MintGreen,
Emerald, Primary, and Surface are C++ constants, not new SVG keywords. The suggested
UI palette does not replace `ui::Theme` defaults.

## Interpolation

`lerp` interpolates stored RGB components and alpha directly. `lerpOklab` converts
sRGB to linear RGB/Oklab, interpolates there, converts back, and clips RGB to [0,1].
Alpha is interpolated independently without clamping, preserving existing behavior
for extrapolation or explicitly out-of-range alpha. Use `clamped()` on the returned
value if all four output channels must be bounded. This is channel interpolation;
it does not implement premultiplied-alpha color mixing or automatic gamut mapping.

The ordinary interpolation/helpers/equality are now usable in constant expressions;
Oklab remains a runtime operation because it uses power/cube-root functions.

## Palette example

`example/palette_demo.cpp` displays all named colors over several pages, a checker
for transparency, and RGB/Oklab gradient comparisons. Its rounded 8-bit hex previews
do not change the exact legacy floating-point constants. Left/Right or Page Up/Down
browse pages; Escape closes. It supports `--page N` (zero-based), `--font PATH`,
`--smoke-test`, and `--screenshot FILE.bmp`.

```powershell
cmake --preset windows-debug -DVECTORGL_BUILD_EXAMPLES=ON
cmake --build build/windows-debug --config Debug --target vectorgl_palette
.\build\windows-debug\example\Debug\vectorgl_palette.exe
```

The example links only the core graphics library and GLFW. No optional UI module
is required. Headless color regressions cover factory signatures, constexpr use,
legacy values, clamping, Oklab neutral/alpha behavior, and new SVG keywords. The
example references every constant in a constexpr range check, and its GPU smoke test
verifies rendering without touching the clipboard or other user data.
