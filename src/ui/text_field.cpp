#include <algorithm>
#include <utility>

#include "control_detail.hpp"
#include "vectorgl/text_box.hpp"
#include "vectorgl/ui/controls.hpp"

namespace vectorgl::ui
{
struct TextField::State final : WidgetPainter
{
    Context& ui;
    Canvas& canvas;
    WidgetId id;
    TextBox box;
    TextFieldOptions options;
    std::string committed, validationError;
    bool session = false, dragging = false;
    std::uint64_t pointer = 0;
    float blinkAdvance = 0;
    State(Context& context, Canvas& target, TextFieldOptions value)
        : ui(context), canvas(target), options(std::move(value))
    {
        detail::validPreferred(options.preferredSize);
        if (options.blurPolicy != BlurPolicy::CommitOrCancel && options.blurPolicy != BlurPolicy::Cancel)
            throw std::invalid_argument("Invalid text field blur policy");
        box.setText(options.value);
        box.setPlaceholder(options.placeholder);
        committed = box.text();
        if (auto error = validate(committed); !error.empty())
            throw std::invalid_argument(error);
    }
    std::string validate(std::string_view text) const
    {
        return options.validate ? options.validate(text) : std::string{};
    }
    void notify(EditKind kind)
    {
        auto copy = options.onEdit;
        const EditEvent event{kind, box.text(), validationError};
        if (copy)
            copy(event);
    }
    void wake()
    {
        ui.invalidate();
        if (ui.focused() == id && ui.contains(id))
            ui.requestWakeup(id, ui.now() + 0.5);
    }
    void changed(const std::string& before)
    {
        wake();
        if (before == box.text())
            return;
        session = true;
        validationError = validate(box.text());
        notify(EditKind::Changed);
    }
    bool commit()
    {
        validationError = validate(box.text());
        ui.invalidate();
        if (!validationError.empty())
            return false;
        if (!session)
            return true;
        committed = box.text();
        session = false;
        notify(EditKind::Committed);
        return true;
    }
    void cancel()
    {
        if (!session)
            return;
        box.setText(committed);
        validationError.clear();
        session = false;
        wake();
        notify(EditKind::Cancelled);
    }
    void sync()
    {
        const auto b = ui.bounds(id);
        if (b)
            box.setBounds(b->x, b->y, b->width, b->height);
        const auto& t = ui.theme();
        box.setFont(t.fontPath, t.fontSize);
        box.style().paddingX = 12;
    }
    void serviceError(ServiceResult result)
    {
        auto copy = options.onServiceError;
        if (copy)
            copy(result);
    }
    EventReply event(const Event& e)
    {
        if (e.type == EventType::LayoutChanged)
        {
            sync();
            return {};
        }
        if (e.type == EventType::FocusGained)
        {
            box.setFocused(true);
            wake();
            return {};
        }
        if (e.type == EventType::FocusLost)
        {
            dragging = false;
            box.setFocused(false);
            ui.cancelWakeup(id);
            if (options.blurPolicy == BlurPolicy::Cancel)
                cancel();
            else if (!commit())
                cancel();
            return {};
        }
        if (e.type == EventType::Cancel)
        {
            dragging = false;
            if (e.reason == CancelReason::Explicit)
                cancel();
            return {};
        }
        if (e.type == EventType::Timer)
        {
            if (ui.focused() == id)
            {
                blinkAdvance += 0.5f;
                wake();
            }
            return {};
        }
        if (e.type == EventType::PointerDown && e.button == PointerButton::Primary)
        {
            sync();
            box.handlePointerDown(e.localX, e.localY, canvas);
            dragging = true;
            pointer = e.pointer;
            wake();
            return {true, true, true};
        }
        if (dragging && pointer == e.pointer && e.type == EventType::PointerMove)
        {
            sync();
            box.handlePointerDrag(e.localX, e.localY, canvas);
            wake();
            return {true};
        }
        if (dragging && pointer == e.pointer && e.type == EventType::PointerUp && e.button == PointerButton::Primary)
        {
            dragging = false;
            return {true};
        }
        if (e.type == EventType::TextInput)
        {
            const auto before = box.text();
            box.pasteText(e.text);
            changed(before);
            return {true};
        }
        if (e.type != EventType::KeyDown && e.type != EventType::KeyRepeat && e.type != EventType::KeyUp)
            return {};
        const bool down = e.type != EventType::KeyUp;
        if ((e.modifiers.control || e.modifiers.meta) && !e.modifiers.alt)
        {
            if (e.key == Key::A)
            {
                if (down)
                {
                    box.selectAll();
                    wake();
                }
                return {true};
            }
            if (e.key == Key::C || e.key == Key::X || e.key == Key::V)
            {
                if (!down || e.type == EventType::KeyRepeat)
                    return {true};
                auto clipboard = ui.services().clipboard;
                if (!clipboard)
                {
                    serviceError({ServiceStatus::Unavailable, "Clipboard unavailable"});
                    return {true};
                }
                // Keep the editing state unchanged until the external operation succeeds.
                if (e.key == Key::V)
                {
                    auto result = clipboard->read();
                    if (result.result.status == ServiceStatus::Success)
                    {
                        const auto before = box.text();
                        box.pasteText(result.text);
                        changed(before);
                    }
                    else
                        serviceError(result.result);
                }
                else
                {
                    std::string selected;
                    if (box.copySelection(selected))
                    {
                        auto result = clipboard->write(selected);
                        if (result.status == ServiceStatus::Success && e.key == Key::X)
                        {
                            const auto before = box.text();
                            box.cutSelection(selected);
                            changed(before);
                        }
                        else if (result.status != ServiceStatus::Success)
                            serviceError(result);
                    }
                }
                return {true};
            }
            // Undo/redo and word navigation are not implemented by this codepoint editor.
            if (e.key == Key::Z || e.key == Key::Y)
                return {true};
        }
        if (e.key == Key::Enter)
        {
            if (e.type == EventType::KeyDown)
                commit();
            return {true};
        }
        if (e.key == Key::Escape)
        {
            if (e.type == EventType::KeyDown)
                cancel();
            return {true};
        }
        std::optional<TextBoxKey> key;
        switch (e.key)
        {
        case Key::Backspace:
            key = TextBoxKey::Backspace;
            break;
        case Key::Delete:
            key = TextBoxKey::Delete;
            break;
        case Key::Left:
            key = TextBoxKey::Left;
            break;
        case Key::Right:
            key = TextBoxKey::Right;
            break;
        case Key::Home:
            key = TextBoxKey::Home;
            break;
        case Key::End:
            key = TextBoxKey::End;
            break;
        default:
            break;
        }
        if (!key)
            return {};
        if (down)
        {
            const auto before = box.text();
            box.handleKey(*key, e.modifiers.shift);
            changed(before);
        }
        return {true}; // No-op editing keys belong to the field too.
    }
    void paint(PaintContext& p) override
    {
        sync();
        const auto colors =
            controlColors(p.theme(), {p.enabled(), p.hovered(), p.focused(), false, false, !validationError.empty()});
        auto& style = box.style();
        style.backgroundColor = colors.background;
        style.borderColor = style.focusedBorderColor = colors.border;
        style.textColor = colors.text;
        style.placeholderColor = p.theme().muted;
        style.selectionColor = p.theme().pressed;
        style.caretColor = p.theme().accent;
        style.borderWidth = p.theme().borderWidth;
        style.focusedBorderWidth = p.theme().borderWidth + 1;
        style.cornerRadius = 6;
        style.paddingX = 12;
        box.render(p.canvas(), blinkAdvance);
        blinkAdvance = 0;
    }
};
TextField::TextField(Context& ui, Canvas& canvas, WidgetId parent, TextFieldOptions options)
    : state_(std::make_shared<State>(ui, canvas, std::move(options)))
{
    auto state = state_;
    Widget w;
    w.parent = parent;
    w.focusable = true;
    w.painter = state;
    w.onMeasure = [state](const MeasureContext& c) { return c.constraints.constrain(state->options.preferredSize); };
    w.onEvent = [state](Context&, WidgetId, const Event& e) { return state->event(e); };
    state->id = ui.add(w);
}
TextField::~TextField()
{
    detail::remove(state_->ui, state_->id);
}
WidgetId TextField::id() const
{
    return state_->id;
}
const std::string& TextField::text() const
{
    return state_->box.text();
}
const std::string& TextField::committedText() const
{
    return state_->committed;
}
const std::string& TextField::error() const
{
    return state_->validationError;
}
bool TextField::editing() const
{
    return state_->session;
}
void TextField::setText(std::string text)
{
    TextBox normalized;
    normalized.setText(text);
    if (auto error = state_->validate(normalized.text()); !error.empty())
        throw std::invalid_argument(error);
    state_->box.setText(normalized.text());
    state_->committed = state_->box.text();
    state_->session = false;
    state_->validationError.clear();
    state_->wake();
}
void TextField::selectAll()
{
    state_->box.selectAll();
    state_->wake();
}
void TextField::editText(std::string text)
{
    auto keep = state_;
    const auto before = keep->box.text();
    keep->box.setText(text);
    keep->changed(before);
}
bool TextField::commit()
{
    auto keep = state_;
    return keep->commit();
}
void TextField::cancel()
{
    auto keep = state_;
    keep->cancel();
}
void TextField::setOnEdit(std::function<void(const EditEvent&)> callback)
{
    state_->options.onEdit = std::move(callback);
}
} // namespace vectorgl::ui
