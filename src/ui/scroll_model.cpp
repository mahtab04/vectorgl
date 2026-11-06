#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "vectorgl/ui/containers.hpp"

namespace vectorgl::ui
{
namespace
{
void validSize(Size value)
{
    if (!std::isfinite(value.width) || !std::isfinite(value.height) || value.width < 0 || value.height < 0)
        throw std::invalid_argument("Scroll dimensions must be finite and nonnegative");
}
float revealAxis(float current, float view, float start, float size, float margin)
{
    if (size + margin * 2 >= view || start - margin < current)
        return start - margin;
    if (start + size + margin > current + view)
        return start + size + margin - view;
    return current;
}
} // namespace
ScrollModel::ScrollModel(bool horizontal, bool vertical) : horizontal_(horizontal), vertical_(vertical) {}
void ScrollModel::setGeometry(Size viewport, Size content)
{
    validSize(viewport);
    validSize(content);
    viewport_ = viewport;
    content_ = content;
    setOffset(offset_);
}
Vec2 ScrollModel::maximumOffset() const
{
    return {horizontal_ ? std::max(0.0f, content_.width - viewport_.width) : 0,
            vertical_ ? std::max(0.0f, content_.height - viewport_.height) : 0};
}
Vec2 ScrollModel::setOffset(Vec2 value)
{
    if (!std::isfinite(value.x) || !std::isfinite(value.y))
        throw std::invalid_argument("Scroll offsets must be finite");
    const auto previous = offset_, maximum = maximumOffset();
    offset_ = {std::clamp(value.x, 0.0f, maximum.x), std::clamp(value.y, 0.0f, maximum.y)};
    return offset_ - previous;
}
Vec2 ScrollModel::scrollBy(Vec2 delta)
{
    if (!std::isfinite(delta.x) || !std::isfinite(delta.y))
        throw std::invalid_argument("Scroll deltas must be finite");
    const auto maximum = maximumOffset();
    return setOffset({static_cast<float>(std::clamp(double(offset_.x) + delta.x, 0.0, double(maximum.x))),
                      static_cast<float>(std::clamp(double(offset_.y) + delta.y, 0.0, double(maximum.y)))});
}
bool ScrollModel::reveal(Bounds target, float margin)
{
    validSize({target.width, target.height});
    if (!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.x + target.width) ||
        !std::isfinite(target.y + target.height) || !std::isfinite(margin) || margin < 0 ||
        !std::isfinite(target.x + target.width + margin) || !std::isfinite(target.y + target.height + margin))
        throw std::invalid_argument("Scroll reveal bounds/margin must be finite");
    const auto moved = setOffset({revealAxis(offset_.x, viewport_.width, target.x, target.width, margin),
                                  revealAxis(offset_.y, viewport_.height, target.y, target.height, margin)});
    return moved.x != 0 || moved.y != 0;
}
Bounds ScrollModel::thumb(bool horizontal, float length, float thickness, float minimum) const
{
    if (!std::isfinite(length) || !std::isfinite(thickness) || !std::isfinite(minimum) || length < 0 || thickness < 0 ||
        minimum < 0)
        throw std::invalid_argument("Scrollbar geometry must be finite and nonnegative");
    const float view = horizontal ? viewport_.width : viewport_.height,
                content = horizontal ? content_.width : content_.height;
    const float maximum = horizontal ? maximumOffset().x : maximumOffset().y;
    if (maximum <= 0 || length == 0 || view == 0 || content == 0)
        return {};
    const float size = std::clamp(length * (view / content), std::min(minimum, length), length);
    const float pos = (length - size) * ((horizontal ? offset_.x : offset_.y) / maximum);
    return horizontal ? Bounds{pos, 0, size, thickness} : Bounds{0, pos, thickness, size};
}
} // namespace vectorgl::ui
