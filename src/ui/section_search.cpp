#include "control_detail.hpp"
#include "vectorgl/ui/workspace_controls.hpp"

namespace vectorgl::ui
{
struct CollapsibleSection::State
{
    struct Header final : detail::ClickState
    {
        std::weak_ptr<State> owner;
        Header(Context& c, Size size, std::weak_ptr<State> state) : ClickState(c, "", size), owner(std::move(state)) {}
        bool checked() const override
        {
            auto s = owner.lock();
            return s && s->options.expanded;
        }
        void activate() override
        {
            if (auto s = owner.lock())
                s->expand(!s->options.expanded, true);
        }
        EventReply routed(const Event& e)
        {
            if ((e.key == Key::Left || e.key == Key::Right) && !e.modifiers.control && !e.modifiers.alt &&
                !e.modifiers.meta)
            {
                if (e.type == EventType::KeyDown || e.type == EventType::KeyRepeat)
                {
                    if (auto s = owner.lock())
                        s->expand(e.key == Key::Right, true);
                    return {true};
                }
                if (e.type == EventType::KeyUp)
                    return {true};
            }
            return event(e);
        }
    };
    Context& ui;
    WidgetId root, body;
    SectionOptions options;
    std::shared_ptr<Header> header;
    std::uint64_t revision = 0;
    State(Context& c, SectionOptions value) : ui(c), options(std::move(value))
    {
        detail::validPreferred(options.headerSize);
    }
    void expand(bool value, bool notify)
    {
        if (options.expanded == value)
            return;
        options.expanded = value;
        const auto current = ++revision;
        header->label = (value ? "v " : "> ") + options.title;
        if (!value && ui.isDescendant(ui.focused(), body))
            ui.focus(header->id);
        if (current != revision)
            return;
        ui.setVisible(body, value);
        if (current != revision)
            return;
        auto cb = options.onChanged;
        if (notify && cb)
            cb(value);
    }
};
CollapsibleSection::CollapsibleSection(Context& ui, WidgetId parent, SectionOptions options)
    : state_(std::make_shared<State>(ui, std::move(options)))
{
    auto state = state_;
    try
    {
        Widget root;
        root.parent = parent;
        root.clipChildren = true;
        root.layout = std::make_shared<ColumnLayout>(Padding{}, 8.0f);
        state->root = ui.add(root);
        state->header = std::make_shared<State::Header>(ui, state->options.headerSize, state);
        state->header->label = (state->options.expanded ? "v " : "> ") + state->options.title;
        auto header = state->header;
        Widget w;
        w.parent = state->root;
        w.focusable = true;
        w.painter = header;
        w.onMeasure = [header](const MeasureContext& c) { return c.constraints.constrain(header->preferred); };
        w.onEvent = [header](Context&, WidgetId, const Event& e) { return header->routed(e); };
        header->id = ui.add(w);
        Widget body;
        body.parent = state->root;
        body.visible = state->options.expanded;
        body.layout = std::make_shared<ColumnLayout>(Padding{}, 8.0f);
        state->body = ui.add(body);
    }
    catch (...)
    {
        detail::remove(ui, state->root);
        throw;
    }
}
CollapsibleSection::~CollapsibleSection()
{
    detail::remove(state_->ui, state_->root);
}
WidgetId CollapsibleSection::id() const
{
    return state_->root;
}
WidgetId CollapsibleSection::headerId() const
{
    return state_->header->id;
}
WidgetId CollapsibleSection::content() const
{
    return state_->body;
}
bool CollapsibleSection::expanded() const
{
    return state_->options.expanded;
}
void CollapsibleSection::setExpanded(bool value)
{
    auto keep = state_;
    keep->expand(value, false);
}

struct SearchField::State
{
    Context& ui;
    WidgetId root;
    SearchFieldOptions options;
    std::shared_ptr<TextField> field;
    std::unique_ptr<Button> clearButton;
    std::string previous;
    bool clearEnabled = true;
    State(Context& c, SearchFieldOptions value) : ui(c), options(std::move(value))
    {
        detail::validPreferred(options.preferredSize);
    }
    void sync(bool notify)
    {
        const auto query = field->text();
        const bool enabled = !query.empty();
        if (!enabled && ui.focused() == clearButton->id())
            ui.focus(field->id());
        if (clearEnabled != enabled)
        {
            ui.setEnabled(clearButton->id(), enabled);
            clearEnabled = enabled;
        }
        const bool changed = query != previous;
        previous = query;
        auto cb = options.onChanged;
        if (changed && notify && cb)
            cb(query);
    }
    void clear()
    {
        field->setText("");
        ui.focus(field->id());
        sync(true);
    }
};
SearchField::SearchField(Context& ui, Canvas& canvas, WidgetId parent, SearchFieldOptions options)
    : state_(std::make_shared<State>(ui, std::move(options)))
{
    auto state = state_;
    std::weak_ptr<State> weak = state;
    try
    {
        Widget root;
        root.parent = parent;
        root.clipChildren = true;
        root.layout = std::make_shared<RowLayout>(Padding{}, 4.0f);
        state->root = ui.add(root);
        TextFieldOptions text;
        text.value = state->options.query;
        text.placeholder = state->options.placeholder;
        text.preferredSize = {std::max(0.0f, state->options.preferredSize.width - 40),
                              state->options.preferredSize.height};
        text.onServiceError = state->options.onServiceError;
        text.onEdit = [weak](const EditEvent&)
        {
            if (auto s = weak.lock())
                s->sync(true);
        };
        state->field = std::make_shared<TextField>(ui, canvas, state->root, std::move(text));
        LayoutHints hints;
        hints.flex = 1;
        ui.setLayoutHints(state->field->id(), hints);
        state->clearButton = std::make_unique<Button>(ui, state->root,
                                                      ButtonOptions{"x",
                                                                    {36, state->options.preferredSize.height},
                                                                    [weak]
                                                                    {
                                                                        if (auto s = weak.lock())
                                                                            s->clear();
                                                                    }});
        state->previous = state->field->text();
        state->sync(false);
    }
    catch (...)
    {
        detail::remove(ui, state->root);
        throw;
    }
}
SearchField::~SearchField()
{
    detail::remove(state_->ui, state_->root);
}
WidgetId SearchField::id() const
{
    return state_->root;
}
WidgetId SearchField::fieldId() const
{
    return state_->field->id();
}
WidgetId SearchField::clearId() const
{
    return state_->clearButton->id();
}
const std::string& SearchField::query() const
{
    return state_->field->text();
}
void SearchField::setQuery(std::string query)
{
    auto keep = state_;
    keep->field->setText(query);
    keep->sync(false);
}
void SearchField::clear()
{
    auto keep = state_;
    keep->clear();
}
} // namespace vectorgl::ui
