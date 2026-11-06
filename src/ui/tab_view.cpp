#include "control_detail.hpp"
#include "vectorgl/ui/workspace_controls.hpp"

namespace vectorgl::ui
{
struct TabBar::State : std::enable_shared_from_this<TabBar::State>
{
    struct TabButton final : detail::ClickState
    {
        std::weak_ptr<State> owner;
        std::size_t index;
        TabButton(Context& c, std::string text, Size size, std::weak_ptr<State> state, std::size_t i)
            : ClickState(c, std::move(text), size), owner(std::move(state)), index(i)
        {
        }
        bool checked() const override
        {
            auto s = owner.lock();
            return s && s->options.selected == index;
        }
        void activate() override
        {
            if (auto s = owner.lock())
                s->select(index, true, true);
        }
        EventReply routed(const Event& e)
        {
            if (e.type == EventType::FocusGained)
                if (auto s = owner.lock())
                    s->rove(index);
            return event(e);
        }
    };
    Context& ui;
    WidgetId root;
    TabBarOptions options;
    std::vector<std::shared_ptr<TabButton>> buttons;
    std::uint64_t revision = 0;
    State(Context& c, TabBarOptions o) : ui(c), options(std::move(o))
    {
        detail::validPreferred(options.tabSize);
        if (options.selected)
            validate(*options.selected);
        else
            options.selected = first();
    }
    std::optional<std::size_t> first() const
    {
        for (std::size_t i = 0; i < options.tabs.size(); ++i)
            if (options.tabs[i].enabled)
                return i;
        return {};
    }
    void validate(std::size_t index) const
    {
        if (index >= options.tabs.size() || !options.tabs[index].enabled)
            throw std::invalid_argument("Tab must identify an enabled item");
    }
    std::optional<std::size_t> focused() const
    {
        for (std::size_t i = 0; i < buttons.size(); ++i)
            if (ui.focused() == buttons[i]->id)
                return i;
        return {};
    }
    void rove(std::optional<std::size_t> index)
    {
        for (std::size_t i = 0; i < buttons.size(); ++i)
            ui.setTabStop(buttons[i]->id, index == i);
    }
    void select(std::optional<std::size_t> index, bool notify, bool focus)
    {
        if (index)
            validate(*index);
        const bool changed = options.selected != index;
        const bool move = focus || focused().has_value();
        options.selected = index;
        const auto current = ++revision;
        const auto target = index ? index : first();
        rove(target);
        if (move && target)
            ui.focus(buttons[*target]->id);
        if (current != revision)
            return;
        ui.invalidate();
        auto callback = options.onChanged;
        if (changed && notify && index && callback)
            callback(*index);
    }
    void enable(std::size_t index, bool enabled)
    {
        if (index >= buttons.size())
            throw std::out_of_range("Tab index");
        if (options.tabs[index].enabled == enabled)
            return;
        options.tabs[index].enabled = enabled;
        if (!enabled && options.selected == index)
        {
            std::optional<std::size_t> next;
            for (std::size_t step = 1; step <= buttons.size(); ++step)
            {
                auto candidate = (index + step) % buttons.size();
                if (options.tabs[candidate].enabled)
                {
                    next = candidate;
                    break;
                }
            }
            select(next, false, false);
        }
        else if (!enabled && focused() == index)
            select(options.selected, false, false);
        ui.setEnabled(buttons[index]->id, enabled);
        rove(options.selected ? options.selected : first());
    }
    EventReply event(const Event& e)
    {
        if (e.modifiers.control || e.modifiers.alt || e.modifiers.meta)
            return {};
        if (e.key != Key::Left && e.key != Key::Right && e.key != Key::Home && e.key != Key::End)
            return {};
        if (e.type == EventType::KeyUp)
            return {true};
        if (e.type != EventType::KeyDown && e.type != EventType::KeyRepeat)
            return {};
        std::vector<std::size_t> enabled;
        for (std::size_t i = 0; i < buttons.size(); ++i)
            if (options.tabs[i].enabled)
                enabled.push_back(i);
        if (enabled.empty())
            return {true};
        auto current = focused();
        auto it = std::find(enabled.begin(), enabled.end(), current.value_or(enabled.front()));
        std::size_t index = it == enabled.end() ? 0 : std::size_t(it - enabled.begin());
        if (e.key == Key::Home)
            index = 0;
        else if (e.key == Key::End)
            index = enabled.size() - 1;
        else
            index = (index + (e.key == Key::Left ? enabled.size() - 1 : 1)) % enabled.size();
        select(enabled[index], true, true);
        return {true};
    }
};
TabBar::TabBar(Context& ui, WidgetId parent, TabBarOptions options)
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
        root.onEvent = [weak](Context&, WidgetId, const Event& e)
        {
            if (auto s = weak.lock())
                return s->event(e);
            return EventReply{};
        };
        state->root = ui.add(root);
        for (std::size_t i = 0; i < state->options.tabs.size(); ++i)
        {
            auto tab =
                std::make_shared<State::TabButton>(ui, state->options.tabs[i].label, state->options.tabSize, weak, i);
            Widget w;
            w.parent = state->root;
            w.focusable = true;
            w.enabled = state->options.tabs[i].enabled;
            w.tabStop = state->options.selected == i;
            w.painter = tab;
            w.layoutHints.flex = 1;
            w.layoutHints.height = state->options.tabSize.height;
            w.onMeasure = [tab](const MeasureContext& c) { return c.constraints.constrain(tab->preferred); };
            w.onEvent = [tab](Context&, WidgetId, const Event& e) { return tab->routed(e); };
            tab->id = ui.add(w);
            state->buttons.push_back(std::move(tab));
        }
    }
    catch (...)
    {
        detail::remove(ui, state->root);
        throw;
    }
}
TabBar::~TabBar()
{
    detail::remove(state_->ui, state_->root);
}
WidgetId TabBar::id() const
{
    return state_->root;
}
WidgetId TabBar::tabId(std::size_t index) const
{
    return state_->buttons.at(index)->id;
}
std::optional<std::size_t> TabBar::selected() const
{
    return state_->options.selected;
}
void TabBar::setSelected(std::optional<std::size_t> index)
{
    auto keep = state_;
    keep->select(index, false, false);
}
void TabBar::setTabEnabled(std::size_t index, bool enabled)
{
    auto keep = state_;
    keep->enable(index, enabled);
}

struct TabView::State
{
    Context& ui;
    WidgetId root;
    std::shared_ptr<TabBar> bar;
    std::vector<WidgetId> pages;
    std::vector<bool> visible;
    std::function<void(std::size_t)> callback;
    explicit State(Context& c) : ui(c) {}
    void sync()
    {
        const auto selected = bar->selected();
        bool hiddenFocus = false;
        for (std::size_t i = 0; i < pages.size(); ++i)
            hiddenFocus |= selected != i && ui.isDescendant(ui.focused(), pages[i]);
        if (hiddenFocus)
        {
            if (selected)
                ui.focus(bar->tabId(*selected));
            else
                ui.focus({});
        }
        if (bar->selected() != selected)
            return;
        for (std::size_t i = 0; i < pages.size(); ++i)
            if (visible[i] != (selected == i))
            {
                visible[i] = selected == i;
                ui.setVisible(pages[i], selected == i);
                if (bar->selected() != selected)
                    return;
            }
    }
};
TabView::TabView(Context& ui, WidgetId parent, TabBarOptions options) : state_(std::make_shared<State>(ui))
{
    auto state = state_;
    state->callback = std::move(options.onChanged);
    std::weak_ptr<State> weak = state;
    try
    {
        Widget root;
        root.parent = parent;
        root.clipChildren = true;
        root.layout = std::make_shared<ColumnLayout>(Padding{}, 8.0f);
        state->root = ui.add(root);
        options.onChanged = [weak](std::size_t index)
        {
            if (auto s = weak.lock())
            {
                s->sync();
                if (s->bar->selected() != index)
                    return;
                auto cb = s->callback;
                if (cb)
                    cb(index);
            }
        };
        const auto count = options.tabs.size();
        state->bar = std::make_shared<TabBar>(ui, state->root, std::move(options));
        for (std::size_t i = 0; i < count; ++i)
        {
            Widget page;
            page.parent = state->root;
            page.visible = state->bar->selected() == i;
            page.clipChildren = true;
            page.layoutHints.flex = 1;
            page.layout = std::make_shared<ColumnLayout>();
            state->pages.push_back(ui.add(page));
            state->visible.push_back(page.visible);
        }
    }
    catch (...)
    {
        detail::remove(ui, state->root);
        throw;
    }
}
TabView::~TabView()
{
    detail::remove(state_->ui, state_->root);
}
WidgetId TabView::id() const
{
    return state_->root;
}
WidgetId TabView::barId() const
{
    return state_->bar->id();
}
WidgetId TabView::tabId(std::size_t index) const
{
    return state_->bar->tabId(index);
}
WidgetId TabView::page(std::size_t index) const
{
    return state_->pages.at(index);
}
std::optional<std::size_t> TabView::selected() const
{
    return state_->bar->selected();
}
void TabView::setSelected(std::optional<std::size_t> index)
{
    auto keep = state_;
    keep->bar->setSelected(index);
    keep->sync();
}
void TabView::setTabEnabled(std::size_t index, bool enabled)
{
    auto keep = state_;
    keep->bar->setTabEnabled(index, enabled);
    keep->sync();
}
} // namespace vectorgl::ui
