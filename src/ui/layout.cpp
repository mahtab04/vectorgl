#include "vectorgl/ui/layout.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace vectorgl::ui
{
namespace
{
constexpr float infinity = std::numeric_limits<float>::infinity();
bool finiteSize(Size s)
{
    return std::isfinite(s.width) && std::isfinite(s.height) && s.width >= 0 && s.height >= 0;
}
float checked(double value)
{
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
        throw std::overflow_error("UI layout exceeds finite coordinate range");
    return static_cast<float>(value);
}
void validateSpacing(Padding p, float x, float y)
{
    for (float v : {p.left, p.top, p.right, p.bottom, x, y})
        if (!std::isfinite(v) || v < 0)
            throw std::invalid_argument("UI padding/gaps must be finite and nonnegative");
}
void validateAlignment(Alignment a)
{
    if (a != Alignment::Start && a != Alignment::Center && a != Alignment::End && a != Alignment::Stretch)
        throw std::invalid_argument("Invalid UI layout alignment");
}
float gaps(std::size_t count, float gap)
{
    return count ? checked(double(count - 1) * gap) : 0;
}
float inner(float offer, float a, float b)
{
    return std::max(0.0f, offer - a - b);
}
float dimension(Size s, bool horizontal)
{
    return horizontal ? s.width : s.height;
}
float offset(float space, Alignment alignment)
{
    space = std::max(0.0f, space);
    return alignment == Alignment::End ? space : alignment == Alignment::Center ? space * 0.5f : 0;
}
double total(const std::vector<float>& values)
{
    return std::accumulate(values.begin(), values.end(), 0.0);
}
void grow(std::vector<float>& sizes, const std::vector<float>& maxima, const std::vector<float>& weights, double extra)
{
    for (std::size_t pass = 0; pass <= sizes.size() && extra > 0.00001; ++pass)
    {
        double sum = 0;
        for (std::size_t i = 0; i < sizes.size(); ++i)
            if (sizes[i] < maxima[i])
                sum += weights[i];
        if (sum <= 0)
            break;
        const double before = total(sizes);
        for (std::size_t i = 0; i < sizes.size(); ++i)
            if (weights[i] > 0 && sizes[i] < maxima[i])
                sizes[i] = checked(std::min(double(maxima[i]), double(sizes[i]) + extra * weights[i] / sum));
        const double used = total(sizes) - before;
        if (used <= 0)
            break;
        extra = std::max(0.0, extra - used);
    }
}
Size extent(const std::vector<Bounds>& boxes, Padding padding)
{
    double width = padding.left, height = padding.top;
    for (auto b : boxes)
    {
        width = std::max(width, double(b.x) + b.width);
        height = std::max(height, double(b.y) + b.height);
    }
    return {checked(width + padding.right), checked(height + padding.bottom)};
}
} // namespace

void Constraints::validate() const
{
    if (!finiteSize(minimum) || std::isnan(maximum.width) || std::isnan(maximum.height) ||
        maximum.width < minimum.width || maximum.height < minimum.height)
        throw std::invalid_argument("Invalid UI size constraints");
}
Size Constraints::constrain(Size size) const
{
    validate();
    if (!finiteSize(size))
        throw std::invalid_argument("UI measurement must return a finite nonnegative size");
    return {std::clamp(size.width, minimum.width, maximum.width),
            std::clamp(size.height, minimum.height, maximum.height)};
}
void LayoutHints::validate() const
{
    Constraints{minimum, maximum}.validate();
    if ((width && (!std::isfinite(*width) || *width < 0)) || (height && (!std::isfinite(*height) || *height < 0)) ||
        !std::isfinite(flex) || flex < 0 || cell.rowSpan == 0 || cell.columnSpan == 0)
        throw std::invalid_argument("Invalid UI layout hints");
    if (align)
        validateAlignment(*align);
}
Size LayoutHints::minimumSize() const
{
    validate();
    return {width ? std::clamp(*width, minimum.width, maximum.width) : minimum.width,
            height ? std::clamp(*height, minimum.height, maximum.height) : minimum.height};
}
Size LayoutHints::maximumSize() const
{
    const auto low = minimumSize();
    return {width ? low.width : maximum.width, height ? low.height : maximum.height};
}
Size LayoutItem::measure(Constraints c) const
{
    c.validate();
    const auto low = hints.minimumSize(), high = hints.maximumSize();
    // Widget limits take precedence over offers, including a stretched cell.
    c.minimum = {std::clamp(c.minimum.width, low.width, high.width),
                 std::clamp(c.minimum.height, low.height, high.height)};
    c.maximum = {std::max(c.minimum.width, std::min(c.maximum.width, high.width)),
                 std::max(c.minimum.height, std::min(c.maximum.height, high.height))};
    return c.constrain(onMeasure ? onMeasure(c) : Size{});
}
LinearLayout::LinearLayout(bool horizontal, Padding padding, float gap, Alignment alignment)
    : horizontal_(horizontal), padding_(padding), gap_(gap), alignment_(alignment)
{
    validateSpacing(padding, gap, gap);
    validateAlignment(alignment);
}
std::vector<Bounds> LinearLayout::compute(std::span<const LayoutItem> items, Size offer, bool arranging) const
{
    const float width = inner(offer.width, padding_.left, padding_.right);
    const float height = inner(offer.height, padding_.top, padding_.bottom);
    const float main = horizontal_ ? width : height, cross = horizontal_ ? height : width;
    std::vector<Size> desired;
    std::vector<float> sizes, minima, maxima, weights;
    for (const auto& item : items)
    {
        const auto low = item.hints.minimumSize(), high = item.hints.maximumSize();
        const auto alignment = item.hints.align.value_or(alignment_);
        Constraints c{{}, {horizontal_ ? infinity : width, infinity}};
        if (!horizontal_ && alignment == Alignment::Stretch && std::isfinite(width))
            c.minimum.width = std::clamp(width, low.width, high.width);
        auto measured = item.measure(c);
        desired.push_back(measured);
        sizes.push_back(dimension(measured, horizontal_));
        minima.push_back(dimension(low, horizontal_));
        maxima.push_back(dimension(high, horizontal_));
        weights.push_back(item.hints.flex);
    }
    if (std::isfinite(main))
    {
        const double extra = double(main) - gaps(items.size(), gap_) - total(sizes);
        if (extra > 0)
            grow(sizes, maxima, weights, extra);
        else if (extra < 0)
        {
            double capacity = 0;
            for (std::size_t i = 0; i < sizes.size(); ++i)
                capacity += double(sizes[i]) - minima[i];
            const double fraction = capacity > 0 ? std::min(1.0, -extra / capacity) : 0;
            for (std::size_t i = 0; i < sizes.size(); ++i)
                sizes[i] = checked(sizes[i] - (double(sizes[i]) - minima[i]) * fraction);
        }
    }
    std::vector<Bounds> boxes;
    double cursor = horizontal_ ? padding_.left : padding_.top;
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        const auto& item = items[i];
        const auto low = item.hints.minimumSize(), high = item.hints.maximumSize();
        const auto alignment = item.hints.align.value_or(alignment_);
        if (horizontal_)
            desired[i] = item.measure({{sizes[i], 0}, {sizes[i], infinity}}); // Width-dependent height.
        float crossSize = dimension(desired[i], !horizontal_);
        if (arranging && alignment == Alignment::Stretch && std::isfinite(cross))
            crossSize = std::clamp(cross, dimension(low, !horizontal_), dimension(high, !horizontal_));
        const float position = arranging ? offset(cross - crossSize, alignment) : 0;
        boxes.push_back(horizontal_ ? Bounds{checked(cursor), padding_.top + position, sizes[i], crossSize}
                                    : Bounds{padding_.left + position, checked(cursor), crossSize, sizes[i]});
        cursor += double(sizes[i]) + gap_;
    }
    return boxes;
}
Size LinearLayout::measure(std::span<const LayoutItem> items, Constraints c) const
{
    c.validate();
    return c.constrain(extent(compute(items, c.maximum, false), padding_));
}
std::vector<Bounds> LinearLayout::arrange(std::span<const LayoutItem> items, Size area) const
{
    if (!finiteSize(area))
        throw std::invalid_argument("UI arrangement requires finite nonnegative area");
    return compute(items, area, true);
}

GridLayout::GridLayout(std::vector<GridTrack> columns, std::vector<GridTrack> rows, Padding padding, float columnGap,
                       float rowGap)
    : columns_(std::move(columns)), rows_(std::move(rows)), padding_(padding), columnGap_(columnGap), rowGap_(rowGap)
{
    validateSpacing(padding, columnGap, rowGap);
    for (const auto* tracks : {&columns_, &rows_})
    {
        if (tracks->empty() || tracks->size() > 4096)
            throw std::invalid_argument("UI grid requires 1..4096 tracks per axis");
        for (auto track : *tracks)
            if (!std::isfinite(track.minimum) || track.minimum < 0 || std::isnan(track.maximum) ||
                track.maximum < track.minimum || !std::isfinite(track.value) || track.value < 0 ||
                (track.kind == TrackKind::Fraction && track.value == 0) ||
                (track.kind != TrackKind::Fixed && track.kind != TrackKind::Auto && track.kind != TrackKind::Fraction))
                throw std::invalid_argument("Invalid UI grid track");
    }
}
namespace
{
std::vector<float> trackSizes(const std::vector<GridTrack>& tracks)
{
    std::vector<float> sizes;
    for (auto t : tracks)
        sizes.push_back(t.kind == TrackKind::Fixed ? std::clamp(t.value, t.minimum, t.maximum) : t.minimum);
    return sizes;
}
void spanRequirement(std::vector<float>& sizes, const std::vector<GridTrack>& tracks, std::size_t begin,
                     std::size_t count, float gap, float required, bool intrinsic, bool bounded)
{
    double current = gaps(count, gap);
    bool automatic = false;
    for (std::size_t i = begin; i < begin + count; ++i)
    {
        current += sizes[i];
        automatic |= tracks[i].kind == TrackKind::Auto;
    }
    if (required <= current)
        return;
    std::vector<float> maxima(sizes), weights(sizes.size(), 0);
    for (std::size_t i = begin; i < begin + count; ++i)
    {
        const auto kind = tracks[i].kind;
        if (kind == TrackKind::Fixed || (intrinsic && (automatic ? kind != TrackKind::Auto : bounded)))
            continue;
        maxima[i] = tracks[i].maximum;
        weights[i] = kind == TrackKind::Fraction ? tracks[i].value : 1;
    }
    grow(sizes, maxima, weights, required - current);
}
void fillFractions(std::vector<float>& sizes, const std::vector<GridTrack>& tracks, float offer, float gap)
{
    if (!std::isfinite(offer))
        return;
    std::vector<float> maxima, weights;
    for (auto t : tracks)
    {
        maxima.push_back(t.maximum);
        weights.push_back(t.kind == TrackKind::Fraction ? t.value : 0);
    }
    grow(sizes, maxima, weights, double(offer) - gaps(sizes.size(), gap) - total(sizes));
}
float spanSize(const std::vector<float>& sizes, std::size_t begin, std::size_t count, float gap)
{
    double size = gaps(count, gap);
    for (std::size_t i = begin; i < begin + count; ++i)
        size += sizes[i];
    return checked(size);
}
std::vector<float> positions(const std::vector<float>& sizes, float start, float gap)
{
    std::vector<float> result;
    double pos = start;
    for (float size : sizes)
    {
        result.push_back(checked(pos));
        pos += double(size) + gap;
    }
    return result;
}
} // namespace
std::vector<Bounds> GridLayout::compute(std::span<const LayoutItem> items, Size offer, bool arranging,
                                        Size* trackExtent) const
{
    const float width = inner(offer.width, padding_.left, padding_.right),
                height = inner(offer.height, padding_.top, padding_.bottom);
    auto columns = trackSizes(columns_), rows = trackSizes(rows_);
    for (const auto& item : items)
    {
        item.hints.validate();
        const auto cell = item.hints.cell;
        if (cell.column >= columns.size() || cell.row >= rows.size() ||
            cell.columnSpan > columns.size() - cell.column || cell.rowSpan > rows.size() - cell.row)
            throw std::invalid_argument("UI grid cell extends beyond explicit tracks");
        const auto natural = item.measure({{}, {width, infinity}});
        spanRequirement(columns, columns_, cell.column, cell.columnSpan, columnGap_, natural.width, true,
                        std::isfinite(width));
        spanRequirement(columns, columns_, cell.column, cell.columnSpan, columnGap_, item.hints.minimumSize().width,
                        false, false);
    }
    fillFractions(columns, columns_, width, columnGap_);
    std::vector<Size> desired;
    for (const auto& item : items)
    {
        const auto cell = item.hints.cell;
        const float cellWidth = spanSize(columns, cell.column, cell.columnSpan, columnGap_);
        const auto low = item.hints.minimumSize(), high = item.hints.maximumSize();
        auto size = item.measure({{}, {cellWidth, infinity}});
        const float actualWidth = item.hints.align.value_or(Alignment::Stretch) == Alignment::Stretch
                                      ? std::clamp(cellWidth, low.width, high.width)
                                      : size.width;
        size = item.measure({{actualWidth, 0}, {actualWidth, infinity}});
        desired.push_back(size);
        spanRequirement(rows, rows_, cell.row, cell.rowSpan, rowGap_, size.height, true, std::isfinite(height));
        spanRequirement(rows, rows_, cell.row, cell.rowSpan, rowGap_, low.height, false, false);
    }
    fillFractions(rows, rows_, height, rowGap_);
    if (trackExtent)
        *trackExtent = {checked(total(columns) + gaps(columns.size(), columnGap_) + padding_.left + padding_.right),
                        checked(total(rows) + gaps(rows.size(), rowGap_) + padding_.top + padding_.bottom)};
    const auto x = positions(columns, padding_.left, columnGap_), y = positions(rows, padding_.top, rowGap_);
    std::vector<Bounds> result;
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        const auto& item = items[i];
        const auto cell = item.hints.cell;
        const float w = spanSize(columns, cell.column, cell.columnSpan, columnGap_),
                    h = spanSize(rows, cell.row, cell.rowSpan, rowGap_);
        const auto low = item.hints.minimumSize(), high = item.hints.maximumSize();
        const auto align = item.hints.align.value_or(Alignment::Stretch);
        Size size = desired[i];
        if (arranging && align == Alignment::Stretch)
            size = {std::clamp(w, low.width, high.width), std::clamp(h, low.height, high.height)};
        result.push_back({x[cell.column] + (arranging ? offset(w - size.width, align) : 0),
                          y[cell.row] + (arranging ? offset(h - size.height, align) : 0), size.width, size.height});
    }
    return result;
}
Size GridLayout::measure(std::span<const LayoutItem> items, Constraints c) const
{
    c.validate();
    Size tracks;
    auto result = extent(compute(items, c.maximum, false, &tracks), padding_);
    result = {std::max(result.width, tracks.width), std::max(result.height, tracks.height)};
    return c.constrain(result);
}
std::vector<Bounds> GridLayout::arrange(std::span<const LayoutItem> items, Size area) const
{
    if (!finiteSize(area))
        throw std::invalid_argument("UI arrangement requires finite nonnegative area");
    return compute(items, area, true);
}
} // namespace vectorgl::ui
