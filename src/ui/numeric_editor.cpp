#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "control_detail.hpp"
#include "vectorgl/ui/controls.hpp"

namespace vectorgl::ui
{
std::optional<double> InvariantNumberFormat::parse(std::string_view text) const
{
    if (text.empty())
        return {};
    double value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || !std::isfinite(value))
        return {};
    return value;
}
std::string InvariantNumberFormat::format(double value, int precision) const
{
    if (!std::isfinite(value) || precision < 0 || precision > 12)
        throw std::invalid_argument("Invalid numeric value/precision");
    std::array<char, 384> buffer{};
    const auto result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed, precision);
    if (result.ec != std::errc{})
        throw std::invalid_argument("Cannot format numeric value");
    return {buffer.data(), result.ptr};
}
struct NumericEditor::State
{
    Context& ui;
    NumericOptions options;
    WidgetId root;
    double value = 0;
    std::shared_ptr<TextField> field;
    std::unique_ptr<Button> minus, plus;
    State(Context& context, NumericOptions o) : ui(context), options(std::move(o))
    {
        if (!std::isfinite(options.minimum) || !std::isfinite(options.maximum) || options.minimum > options.maximum ||
            !std::isfinite(options.step) || options.step <= 0 || options.precision < 0 || options.precision > 12)
            throw std::invalid_argument("Invalid numeric range, step, or precision");
        if (!options.format)
            options.format = std::make_shared<InvariantNumberFormat>();
    }
    std::string validate(std::string_view text) const
    {
        const auto parsed = options.format->parse(text);
        if (!parsed || !std::isfinite(*parsed))
            return "Enter a complete number";
        if (*parsed < options.minimum || *parsed > options.maximum)
            return "Value is outside the allowed range";
        return {};
    }
    std::string formatted(double number) const
    {
        if (!std::isfinite(number) || number < options.minimum || number > options.maximum)
            throw std::invalid_argument("Numeric value is outside the allowed range");
        auto text = options.format->format(number, options.precision);
        if (!validate(text).empty())
            throw std::invalid_argument("Number format returned an invalid/out-of-range value");
        return text;
    }
    void edited(const EditEvent& e)
    {
        if (e.kind == EditKind::Committed)
        {
            const auto parsed = options.format->parse(e.text);
            if (!parsed || !validate(e.text).empty())
                throw std::logic_error("Number format changed while committing");
            value = *parsed;
        }
        auto copy = options.onEdit;
        if (copy)
            copy({e.kind, value, e.text, e.error});
    }
    bool step(int direction)
    {
        if (direction != -1 && direction != 1)
            throw std::invalid_argument("Step direction must be -1 or +1");
        if (!field || !ui.contains(root) || !validate(field->text()).empty())
            return false;
        const double current = *options.format->parse(field->text());
        // Use long double before clamping so a large finite step cannot overflow double.
        const double next = static_cast<double>(
            std::clamp(static_cast<long double>(current) + direction * static_cast<long double>(options.step),
                       static_cast<long double>(options.minimum), static_cast<long double>(options.maximum)));
        const auto text = formatted(next);
        if (*options.format->parse(text) == current)
            return false;
        field->editText(text);
        if (ui.contains(root))
            field->commit();
        return true;
    }
};
NumericEditor::NumericEditor(Context& ui, Canvas& canvas, WidgetId parent, NumericOptions options)
    : state_(std::make_shared<State>(ui, std::move(options)))
{
    auto state = state_;
    const auto initial = state->formatted(state->options.value);
    state->value = *state->options.format->parse(initial);
    std::weak_ptr<State> weak = state;
    try
    {
        Widget w;
        w.parent = parent;
        w.layout = std::make_shared<RowLayout>(Padding{}, 4.0f);
        w.onEvent = [weak](Context&, WidgetId, const Event& e)
        {
            if ((e.type == EventType::KeyDown || e.type == EventType::KeyRepeat || e.type == EventType::KeyUp) &&
                (e.key == Key::Up || e.key == Key::Down))
            {
                if (auto current = weak.lock(); current && e.type != EventType::KeyUp)
                    current->step(e.key == Key::Up ? 1 : -1);
                return EventReply{true};
            }
            return EventReply{};
        };
        state->root = ui.add(w);
        TextFieldOptions f;
        f.value = initial;
        f.blurPolicy = state->options.blurPolicy;
        f.validate = [weak](std::string_view text)
        {
            auto current = weak.lock();
            return current ? current->validate(text) : std::string{};
        };
        f.onEdit = [weak](const EditEvent& e)
        {
            if (auto current = weak.lock())
                current->edited(e);
        };
        f.onServiceError = state->options.onServiceError;
        state->field = std::make_shared<TextField>(ui, canvas, state->root, std::move(f));
        LayoutHints hints;
        hints.flex = 1;
        hints.minimum.width = 60;
        ui.setLayoutHints(state->field->id(), hints);
        state->minus = std::make_unique<Button>(ui, state->root,
                                                ButtonOptions{"-",
                                                              {36, 44},
                                                              [weak]
                                                              {
                                                                  if (auto current = weak.lock())
                                                                      current->step(-1);
                                                              }});
        state->plus = std::make_unique<Button>(ui, state->root,
                                               ButtonOptions{"+",
                                                             {36, 44},
                                                             [weak]
                                                             {
                                                                 if (auto current = weak.lock())
                                                                     current->step(1);
                                                             }});
    }
    catch (...)
    {
        detail::remove(ui, state->root);
        throw;
    }
}
NumericEditor::~NumericEditor()
{
    detail::remove(state_->ui, state_->root);
}
WidgetId NumericEditor::id() const
{
    return state_->root;
}
WidgetId NumericEditor::fieldId() const
{
    return state_->field->id();
}
const std::string& NumericEditor::text() const
{
    return state_->field->text();
}
const std::string& NumericEditor::error() const
{
    return state_->field->error();
}
bool NumericEditor::editing() const
{
    return state_->field->editing();
}
double NumericEditor::value() const
{
    return state_->value;
}
void NumericEditor::setValue(double value)
{
    const auto text = state_->formatted(value);
    state_->field->setText(text);
    state_->value = *state_->options.format->parse(text);
}
bool NumericEditor::step(int direction)
{
    auto keep = state_;
    return keep->step(direction);
}
bool NumericEditor::commit()
{
    auto keep = state_;
    return keep->field->commit();
}
void NumericEditor::cancel()
{
    auto keep = state_;
    keep->field->cancel();
}
} // namespace vectorgl::ui
