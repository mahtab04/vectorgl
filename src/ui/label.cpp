#include <algorithm>
#include <cmath>

#include "control_detail.hpp"
#include "vectorgl/detail/utf8.hpp"
#include "vectorgl/ui/workspace_controls.hpp"

namespace vectorgl::ui
{
std::string ellipsizeText(std::string_view input, float width, const std::function<float(std::string_view)>& measure)
{
    if (!std::isfinite(width) || width < 0 || !measure)
        throw std::invalid_argument("Invalid ellipsis width/measurement");
    const auto text = vectorgl::detail::singleLine(input);
    auto checked = [&](std::string_view s)
    {
        const float value = measure(s);
        if (!std::isfinite(value) || value < 0)
            throw std::invalid_argument("Invalid text measurement");
        return value;
    };
    if (checked(text) <= width)
        return text;
    const std::string suffix = "\xE2\x80\xA6";
    if (checked(suffix) > width)
        return {};
    std::vector<std::size_t> boundaries{0};
    for (std::size_t at = 0; at < text.size();)
    {
        vectorgl::detail::nextCodepoint(text, at);
        boundaries.push_back(at);
    }
    std::size_t low = 0, high = boundaries.size() - 1;
    while (low < high)
    {
        const auto mid = low + (high - low + 1) / 2;
        if (checked(text.substr(0, boundaries[mid]) + suffix) <= width)
            low = mid;
        else
            high = mid - 1;
    }
    return text.substr(0, boundaries[low]) + suffix;
}
namespace
{
void validOverflow(TextOverflow value)
{
    if (value != TextOverflow::Clip && value != TextOverflow::Ellipsis && value != TextOverflow::Wrap)
        throw std::invalid_argument("Invalid label overflow");
}
} // namespace
struct Label::State final : WidgetPainter
{
    Context& ui;
    Canvas& canvas;
    WidgetId id;
    LabelOptions options;
    std::string cachedText, cachedFont;
    float cachedWidth = -1, cachedSize = -1;
    State(Context& c, Canvas& target, LabelOptions value) : ui(c), canvas(target), options(std::move(value))
    {
        validOverflow(options.overflow);
        detail::validPreferred(options.fallbackSize);
        for (auto v : {options.padding.left, options.padding.top, options.padding.right, options.padding.bottom})
            if (!std::isfinite(v) || v < 0)
                throw std::invalid_argument("Invalid label padding");
        if (!std::isfinite(options.padding.left + options.padding.right) ||
            !std::isfinite(options.padding.top + options.padding.bottom) || !std::isfinite(options.fontScale) ||
            options.fontScale <= 0)
            throw std::invalid_argument("Invalid label dimensions/font scale");
        if (options.align != TextAlign::Left && options.align != TextAlign::Center && options.align != TextAlign::Right)
            throw std::invalid_argument("Invalid label alignment");
    }
    std::string normalized() const
    {
        if (options.overflow != TextOverflow::Wrap)
            return vectorgl::detail::singleLine(options.text);
        std::string result;
        for (std::size_t at = 0; at < options.text.size();)
        {
            const auto cp = vectorgl::detail::nextCodepoint(options.text, at);
            if (cp == '\n' || vectorgl::detail::isSingleLineCodepoint(cp))
                vectorgl::detail::appendCodepoint(result, cp);
        }
        return result;
    }
    bool font(Canvas& target, const Theme& theme)
    {
        const float size = theme.fontSize * options.fontScale;
        if (!std::isfinite(size))
            throw std::invalid_argument("Label font size overflow");
        return !theme.fontPath.empty() && target.setFont(theme.fontPath, size);
    }
    Size measure(const MeasureContext& c)
    {
        if (!font(canvas, c.theme))
            return c.constraints.constrain(options.fallbackSize);
        const auto& p = options.padding;
        TextLayoutOptions layout;
        if (options.overflow == TextOverflow::Wrap && std::isfinite(c.constraints.maximum.width))
            layout.maxWidth = std::max(1.0f, c.constraints.maximum.width - p.left - p.right);
        else
            layout.wrap = TextWrap::None;
        const auto measured = canvas.layoutText(normalized(), layout);
        return c.constraints.constrain({measured.width + p.left + p.right, measured.height + p.top + p.bottom});
    }
    void paint(PaintContext& p) override
    {
        if (!font(p.canvas(), p.theme()))
            return;
        auto& c = p.canvas();
        const auto b = p.bounds();
        const auto& pad = options.padding;
        const float width = std::max(0.0f, b.width - pad.left - pad.right),
                    height = std::max(0.0f, b.height - pad.top - pad.bottom);
        if (width == 0 || height == 0)
            return;
        c.clipRect(b.x + pad.left, b.y + pad.top, width, height);
        c.setFillColor(p.enabled() ? p.theme().text : p.theme().muted);
        TextLayoutOptions layout;
        layout.maxWidth = width;
        layout.align = options.align;
        layout.wrap = options.overflow == TextOverflow::Wrap ? TextWrap::Word : TextWrap::None;
        if (options.overflow == TextOverflow::Ellipsis)
        {
            const float size = p.theme().fontSize * options.fontScale;
            if (cachedWidth != width || cachedFont != p.theme().fontPath || cachedSize != size)
            {
                cachedText = ellipsizeText(options.text, width,
                                           [&](std::string_view text) { return c.measureText(std::string(text)); });
                cachedWidth = width;
                cachedFont = p.theme().fontPath;
                cachedSize = size;
            }
            c.fillText(cachedText, b.x + pad.left, b.y + pad.top, layout);
        }
        else
            c.fillText(normalized(), b.x + pad.left, b.y + pad.top, layout);
    }
};
Label::Label(Context& ui, Canvas& canvas, WidgetId parent, LabelOptions options)
    : state_(std::make_shared<State>(ui, canvas, std::move(options)))
{
    auto state = state_;
    Widget w;
    w.parent = parent;
    w.painter = state;
    w.hitTestVisible = false;
    w.onMeasure = [state](const MeasureContext& c) { return state->measure(c); };
    state->id = ui.add(w);
}
Label::~Label()
{
    detail::remove(state_->ui, state_->id);
}
WidgetId Label::id() const
{
    return state_->id;
}
const std::string& Label::text() const
{
    return state_->options.text;
}
void Label::setText(std::string text)
{
    state_->options.text = std::move(text);
    state_->cachedWidth = -1;
    state_->ui.invalidate(Invalidation::Measure);
}
void Label::setOverflow(TextOverflow value)
{
    validOverflow(value);
    state_->options.overflow = value;
    state_->cachedWidth = -1;
    state_->ui.invalidate(Invalidation::Measure);
}

namespace
{
struct LinePainter final : WidgetPainter
{
    SeparatorOptions options;
    explicit LinePainter(SeparatorOptions value) : options(value) {}
    void paint(PaintContext& p) override
    {
        const auto b = p.bounds();
        if (options.axis == SeparatorAxis::Horizontal)
        {
            const float thickness = std::min(options.thickness, b.height),
                        inset = std::min(options.inset, b.width * .5f);
            p.fillRect({b.x + inset, b.y + (b.height - thickness) * .5f, b.width - inset * 2, thickness},
                       p.theme().border);
        }
        else
        {
            const float thickness = std::min(options.thickness, b.width),
                        inset = std::min(options.inset, b.height * .5f);
            p.fillRect({b.x + (b.width - thickness) * .5f, b.y + inset, thickness, b.height - inset * 2},
                       p.theme().border);
        }
    }
};
} // namespace
Separator::Separator(Context& ui, WidgetId parent, SeparatorOptions options) : ui_(ui)
{
    if ((options.axis != SeparatorAxis::Horizontal && options.axis != SeparatorAxis::Vertical) ||
        !std::isfinite(options.thickness) || options.thickness <= 0 || !std::isfinite(options.length) ||
        options.length < 0 || !std::isfinite(options.inset) || options.inset < 0)
        throw std::invalid_argument("Invalid separator options");
    Widget w;
    w.parent = parent;
    w.hitTestVisible = false;
    w.painter = std::make_shared<LinePainter>(options);
    w.onMeasure = [options](const MeasureContext& c)
    {
        return c.constraints.constrain(options.axis == SeparatorAxis::Horizontal
                                           ? Size{options.length, options.thickness}
                                           : Size{options.thickness, options.length});
    };
    id_ = ui.add(w);
}
Separator::~Separator()
{
    detail::remove(ui_, id_);
}
WidgetId Separator::id() const
{
    return id_;
}
} // namespace vectorgl::ui
