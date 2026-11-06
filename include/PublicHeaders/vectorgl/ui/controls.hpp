#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "vectorgl/ui/context.hpp"

namespace vectorgl::ui
{
struct ControlState
{
    bool enabled = true, hovered = false, focused = false, pressed = false, checked = false, invalid = false;
};
struct ControlColors
{
    Color background, border, text;
};
ControlColors controlColors(const Theme& theme, ControlState state);

struct ButtonOptions
{
    std::string label;
    Size preferredSize{140, 44};
    std::function<void()> onActivated;
};
/*! Controllers own their registered subtrees. Context must outlive them.
 * Destruction suppresses client callback errors; explicit Context::remove(id())
 * allows handling those errors. Programmatic setters do not emit user actions.
 */
class Button
{
public:
    Button(Context& context, WidgetId parent = {}, ButtonOptions options = {});
    ~Button();
    Button(const Button&) = delete;
    Button& operator=(const Button&) = delete;
    WidgetId id() const;
    void setLabel(std::string label);
    void setOnActivated(std::function<void()> callback);

private:
    struct State;
    std::shared_ptr<State> state_;
};
enum class CheckStyle
{
    Checkbox,
    Toggle
};
struct CheckBoxOptions
{
    std::string label;
    bool checked = false;
    CheckStyle style = CheckStyle::Checkbox;
    Size preferredSize{200, 44};
    std::function<void(bool)> onChanged;
};
class CheckBox
{
public:
    CheckBox(Context& context, WidgetId parent = {}, CheckBoxOptions options = {});
    ~CheckBox();
    CheckBox(const CheckBox&) = delete;
    CheckBox& operator=(const CheckBox&) = delete;
    WidgetId id() const;
    bool checked() const;
    void setChecked(bool checked);
    void setOnChanged(std::function<void(bool)> callback);

private:
    struct State;
    std::shared_ptr<State> state_;
};
enum class EditKind
{
    Changed,
    Committed,
    Cancelled
};
enum class BlurPolicy
{
    CommitOrCancel,
    Cancel
};
struct EditEvent
{
    EditKind kind;
    std::string text, error;
};
struct TextFieldOptions
{
    std::string value, placeholder;
    Size preferredSize{240, 44};
    BlurPolicy blurPolicy = BlurPolicy::CommitOrCancel;
    // Empty result means valid. Invalid intermediate buffers remain editable.
    std::function<std::string(std::string_view)> validate;
    std::function<void(const EditEvent&)> onEdit;
    std::function<void(const ServiceResult&)> onServiceError;
};
/*! Routed single-line TextBox wrapper. Context and the supplied Canvas must outlive
 * the controller. Use the same initialized Canvas for painting/pointer measurement.
 * Keyboard editing and validation need no font or GL context. Direct TextBox is unchanged.
 */
class TextField
{
public:
    TextField(Context& context, Canvas& canvas, WidgetId parent = {}, TextFieldOptions options = {});
    ~TextField();
    TextField(const TextField&) = delete;
    TextField& operator=(const TextField&) = delete;
    WidgetId id() const;
    const std::string& text() const;
    const std::string& committedText() const;
    const std::string& error() const;
    bool editing() const;
    void setText(std::string text);  // Validated replacement of buffer and committed value; no notification.
    void editText(std::string text); // Replace buffer, allowing invalid intermediate text; emits Changed.
    void selectAll();
    bool commit(); // False for invalid buffer. One completion per changed edit session.
    void cancel();
    void setOnEdit(std::function<void(const EditEvent&)> callback);

private:
    struct State;
    std::shared_ptr<State> state_;
};

/*! Invariant parsing/formatting by default; applications may inject locale policy.
 * Implementations must not change global locale. Parsing must consume the whole input.
 */
class NumberFormat
{
public:
    virtual ~NumberFormat() = default;
    virtual std::optional<double> parse(std::string_view text) const = 0;
    virtual std::string format(double value, int precision) const = 0;
};
class InvariantNumberFormat final : public NumberFormat
{
public:
    std::optional<double> parse(std::string_view text) const override;
    std::string format(double value, int precision) const override;
};
struct NumberEditEvent
{
    EditKind kind;
    double value; // Last committed value for Changed/Cancelled.
    std::string text, error;
};
struct NumericOptions
{
    double value = 0, minimum = 0, maximum = 100, step = 1;
    int precision = 2;
    BlurPolicy blurPolicy = BlurPolicy::CommitOrCancel;
    std::shared_ptr<const NumberFormat> format;
    std::function<void(const NumberEditEvent&)> onEdit;
    std::function<void(const ServiceResult&)> onServiceError;
};
class NumericEditor
{
public:
    NumericEditor(Context& context, Canvas& canvas, WidgetId parent = {}, NumericOptions options = {});
    ~NumericEditor();
    NumericEditor(const NumericEditor&) = delete;
    NumericEditor& operator=(const NumericEditor&) = delete;
    WidgetId id() const;
    WidgetId fieldId() const;
    const std::string& text() const;
    const std::string& error() const;
    bool editing() const;
    double value() const;
    void setValue(double value);
    bool commit();
    void cancel();
    bool step(int direction); // -1/+1; valid buffers only, clamped to the range.
private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace vectorgl::ui
