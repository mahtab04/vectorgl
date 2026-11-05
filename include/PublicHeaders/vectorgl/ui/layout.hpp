#pragma once

#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace vectorgl::ui
{
struct Theme;
struct Size
{
    float width = 0, height = 0;
    bool operator==(const Size&) const = default;
};
struct Bounds
{
    float x = 0, y = 0, width = 0, height = 0;
    bool contains(float px, float py) const
    {
        return px >= x && py >= y && px < x + width && py < y + height;
    }
    bool operator==(const Bounds&) const = default;
};
/*! Maximum dimensions may be +infinity for intrinsic measurement. */
struct Constraints
{
    Size minimum;
    Size maximum{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
    void validate() const;
    Size constrain(Size size) const;
    bool operator==(const Constraints&) const = default;
};
struct Padding
{
    float left = 0, top = 0, right = 0, bottom = 0;
};
enum class Alignment
{
    Start,
    Center,
    End,
    Stretch
};
struct GridCell
{
    std::size_t row = 0, column = 0, rowSpan = 1, columnSpan = 1;
};
struct LayoutHints
{
    Size minimum;
    Size maximum{std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
    std::optional<float> width, height; // Fixed dimensions, clamped to minimum/maximum.
    float flex = 0;                     // Linear layout's share of positive remaining main-axis space.
    std::optional<Alignment> align;     // Linear cross axis; both axes for grid cells.
    GridCell cell;
    void validate() const;
    Size minimumSize() const;
    Size maximumSize() const;
};
struct MeasureContext
{
    Constraints constraints;
    const Theme& theme;
};
/*! Ephemeral child measurement adapter. Strategies must not retain callbacks.
 * Fixed sizes and hints are applied by measure(); minima take precedence when
 * the parent's offer is too small. Callbacks return finite nonnegative sizes.
 */
struct LayoutItem
{
    LayoutHints hints;
    std::function<Size(Constraints)> onMeasure;
    Size measure(Constraints constraints = {}) const;
};
/*! Stateless, immutable layout strategy. Returned boxes match input item order.
 * measure() may receive unbounded maximum dimensions; arrange() receives a finite
 * nonnegative area. Custom strategies must return finite nonnegative sizes.
 */
class Layout
{
public:
    virtual ~Layout() = default;
    virtual Size measure(std::span<const LayoutItem> items, Constraints constraints) const = 0;
    virtual std::vector<Bounds> arrange(std::span<const LayoutItem> items, Size area) const = 0;
};
class LinearLayout : public Layout
{
public:
    Size measure(std::span<const LayoutItem> items, Constraints constraints) const override;
    std::vector<Bounds> arrange(std::span<const LayoutItem> items, Size area) const override;

protected:
    LinearLayout(bool horizontal, Padding padding, float gap, Alignment alignment);

private:
    bool horizontal_;
    Padding padding_;
    float gap_;
    Alignment alignment_;
    std::vector<Bounds> compute(std::span<const LayoutItem> items, Size offer, bool arranging) const;
};
class RowLayout final : public LinearLayout
{
public:
    explicit RowLayout(Padding padding = {}, float gap = 0, Alignment alignment = Alignment::Stretch)
        : LinearLayout(true, padding, gap, alignment)
    {
    }
};
class ColumnLayout final : public LinearLayout
{
public:
    explicit ColumnLayout(Padding padding = {}, float gap = 0, Alignment alignment = Alignment::Stretch)
        : LinearLayout(false, padding, gap, alignment)
    {
    }
};
enum class TrackKind
{
    Fixed,
    Auto,
    Fraction
};
struct GridTrack
{
    TrackKind kind = TrackKind::Auto;
    float value = 1, minimum = 0, maximum = std::numeric_limits<float>::infinity();
    static GridTrack fixed(float size)
    {
        return {TrackKind::Fixed, size};
    }
    static GridTrack automatic(float minimum = 0, float maximum = std::numeric_limits<float>::infinity())
    {
        return {TrackKind::Auto, 1, minimum, maximum};
    }
    static GridTrack fraction(float weight = 1, float minimum = 0,
                              float maximum = std::numeric_limits<float>::infinity())
    {
        return {TrackKind::Fraction, weight, minimum, maximum};
    }
};
/*! Explicit fixed/auto/fraction tracks and cell spans. No implicit placement.
 * Overlapping cells paint in widget order. Fixed tracks never grow; overflowing
 * item minima remain intact. Track counts are bounded at 4096 per axis.
 */
class GridLayout final : public Layout
{
public:
    GridLayout(std::vector<GridTrack> columns, std::vector<GridTrack> rows, Padding padding = {}, float columnGap = 0,
               float rowGap = 0);
    Size measure(std::span<const LayoutItem> items, Constraints constraints) const override;
    std::vector<Bounds> arrange(std::span<const LayoutItem> items, Size area) const override;

private:
    std::vector<GridTrack> columns_, rows_;
    Padding padding_;
    float columnGap_, rowGap_;
    std::vector<Bounds> compute(std::span<const LayoutItem> items, Size offer, bool arranging,
                                Size* trackExtent = nullptr) const;
};
struct LayoutStats
{
    std::size_t measurements = 0, cacheHits = 0, arrangedWidgets = 0;
};
} // namespace vectorgl::ui
