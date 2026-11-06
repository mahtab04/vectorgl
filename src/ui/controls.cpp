#include "vectorgl/ui/controls.hpp"

#include "control_detail.hpp"

namespace vectorgl::ui
{
ControlColors controlColors(const Theme& t, ControlState s)
{
    if (!s.enabled)
        return {t.disabled, t.border, t.muted};
    return {s.pressed   ? t.pressed
            : s.hovered ? t.hover
                        : t.surface,
            s.invalid                ? t.error
            : s.focused || s.checked ? t.accent
                                     : t.border,
            t.text};
}
struct Button::State final : detail::ClickState
{
    std::function<void()> callback;
    State(Context& ui, ButtonOptions o)
        : ClickState(ui, std::move(o.label), o.preferredSize), callback(std::move(o.onActivated))
    {
    }
    void activate() override
    {
        auto copy = callback;
        if (copy)
            copy();
    }
};
Button::Button(Context& ui, WidgetId parent, ButtonOptions options)
    : state_(std::make_shared<State>(ui, std::move(options)))
{
    detail::registerClick(state_, parent);
}
Button::~Button()
{
    detail::remove(state_->ui, state_->id);
}
WidgetId Button::id() const
{
    return state_->id;
}
void Button::setLabel(std::string label)
{
    state_->label = std::move(label);
    state_->ui.invalidate();
}
void Button::setOnActivated(std::function<void()> callback)
{
    state_->callback = std::move(callback);
}

struct CheckBox::State final : detail::ClickState
{
    bool value;
    CheckStyle style;
    std::function<void(bool)> callback;
    State(Context& ui, CheckBoxOptions o)
        : ClickState(ui, std::move(o.label), o.preferredSize), value(o.checked), style(o.style),
          callback(std::move(o.onChanged))
    {
    }
    bool checked() const override
    {
        return value;
    }
    float textInset() const override
    {
        return style == CheckStyle::Toggle ? 62.0f : 42.0f;
    }
    void activate() override
    {
        value = !value;
        auto copy = callback;
        if (copy)
            copy(value);
    }
    void indicator(PaintContext& p) override
    {
        auto& c = p.canvas();
        const auto b = p.bounds();
        const auto color = !p.enabled() ? p.theme().muted : value ? p.theme().accent : p.theme().border;
        c.setFillColor(color);
        if (style == CheckStyle::Toggle)
        {
            c.fillRoundedRect(b.x + 10, b.y + b.height * .5f - 10, 40, 20, 10);
            c.setFillColor(p.theme().background);
            c.fillCircle(b.x + (value ? 40 : 20), b.y + b.height * .5f, 7);
        }
        else
        {
            c.fillRoundedRect(b.x + 12, b.y + b.height * .5f - 9, 18, 18, 3);
            if (value)
            {
                c.setStrokeColor(p.theme().background);
                c.setLineWidth(2.5f);
                c.beginPath();
                c.moveTo(b.x + 16, b.y + b.height * .5f);
                c.lineTo(b.x + 20, b.y + b.height * .5f + 4);
                c.lineTo(b.x + 27, b.y + b.height * .5f - 4);
                c.stroke();
            }
        }
    }
};
CheckBox::CheckBox(Context& ui, WidgetId parent, CheckBoxOptions options)
{
    if (options.style != CheckStyle::Checkbox && options.style != CheckStyle::Toggle)
        throw std::invalid_argument("Invalid check style");
    state_ = std::make_shared<State>(ui, std::move(options));
    detail::registerClick(state_, parent);
}
CheckBox::~CheckBox()
{
    detail::remove(state_->ui, state_->id);
}
WidgetId CheckBox::id() const
{
    return state_->id;
}
bool CheckBox::checked() const
{
    return state_->value;
}
void CheckBox::setChecked(bool value)
{
    state_->value = value;
    state_->ui.invalidate();
}
void CheckBox::setOnChanged(std::function<void(bool)> callback)
{
    state_->callback = std::move(callback);
}
} // namespace vectorgl::ui
