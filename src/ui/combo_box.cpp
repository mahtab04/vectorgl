#include "control_detail.hpp"
#include "vectorgl/ui/overlays.hpp"

namespace vectorgl::ui
{
struct ComboBox::State final : detail::ClickState, std::enable_shared_from_this<ComboBox::State>
{
    OverlayManager& overlays;
    ComboBoxOptions options;
    OverlayId popup;
    State(Context& ui, OverlayManager& manager, ComboBoxOptions value)
        : ClickState(ui, "", value.preferredSize), overlays(manager), options(std::move(value))
    {
        select(options.selected);
    }
    void select(std::optional<std::size_t> index)
    {
        if (index &&
            (*index >= options.items.size() || !options.items[*index].enabled || options.items[*index].separator))
            throw std::invalid_argument("ComboBox selection must identify an enabled item");
        options.selected = index;
        label = (index ? options.items[*index].text : "Choose...") + "  v";
        ui.invalidate();
    }
    void activate() override
    {
        if (overlays.contains(popup))
            return;
        PopupOptions list;
        list.items = options.items;
        list.selected = options.selected;
        const auto bounds = ui.bounds(id);
        list.width = bounds && bounds->width > 0 ? bounds->width : std::max(1.0f, preferred.width);
        std::weak_ptr<State> weak = shared_from_this();
        list.onSelected = [weak](std::size_t index)
        {
            if (auto state = weak.lock())
            {
                const bool changed = state->options.selected != index;
                state->select(index);
                auto callback = state->options.onChanged;
                if (changed && callback)
                    callback(index);
            }
        };
        list.onClosed = [weak](DismissReason)
        {
            if (auto state = weak.lock())
                state->popup = {};
        };
        popup = overlays.openPopup(id, std::move(list));
    }
    EventReply routed(const Event& e)
    {
        if ((e.key == Key::Down || e.key == Key::Up) &&
            (e.type == EventType::KeyDown || e.type == EventType::KeyRepeat || e.type == EventType::KeyUp))
        {
            if (e.type == EventType::KeyDown)
                activate();
            return {true};
        }
        return event(e);
    }
};
ComboBox::ComboBox(Context& ui, OverlayManager& manager, WidgetId parent, ComboBoxOptions options)
    : state_(std::make_shared<State>(ui, manager, std::move(options)))
{
    detail::validPreferred(state_->preferred);
    auto state = state_;
    Widget w;
    w.parent = parent;
    w.focusable = true;
    w.painter = state;
    w.onMeasure = [state](const MeasureContext& c) { return c.constraints.constrain(state->preferred); };
    w.onEvent = [state](Context&, WidgetId, const Event& e) { return state->routed(e); };
    state->id = ui.add(w);
}
ComboBox::~ComboBox()
{
    try
    {
        state_->overlays.close(state_->popup);
    }
    catch (...)
    {
    }
    detail::remove(state_->ui, state_->id);
}
WidgetId ComboBox::id() const
{
    return state_->id;
}
std::optional<std::size_t> ComboBox::selected() const
{
    return state_->options.selected;
}
void ComboBox::setSelected(std::optional<std::size_t> index)
{
    state_->select(index);
}
void ComboBox::open()
{
    auto keep = state_;
    keep->activate();
}
} // namespace vectorgl::ui
