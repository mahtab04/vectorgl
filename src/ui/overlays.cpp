#include "vectorgl/ui/overlays.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <stdexcept>

#include "control_detail.hpp"
#include "vectorgl/ui/containers.hpp"

namespace vectorgl::ui
{
namespace
{
std::atomic<std::uint64_t> overlaySerial{1};
OverlayId nextId()
{
    const auto value = overlaySerial.fetch_add(1);
    if (!value)
        throw std::overflow_error("Overlay identity exhausted");
    return {value};
}
void positive(float value)
{
    if (!std::isfinite(value) || value <= 0)
        throw std::invalid_argument("Overlay sizes must be positive and finite");
}
Bounds fit(Bounds view, Size size, float x, float y)
{
    const float margin = std::min({8.0f, view.width * .25f, view.height * .25f});
    const float w = std::min(size.width, view.width - margin * 2), h = std::min(size.height, view.height - margin * 2);
    return {std::clamp(x, view.x + margin, view.x + view.width - margin - w),
            std::clamp(y, view.y + margin, view.y + view.height - margin - h), w, h};
}
struct TextPanel final : WidgetPainter
{
    std::string title, message;
    TextPanel(std::string heading, std::string text) : title(std::move(heading)), message(std::move(text)) {}
    void paint(PaintContext& p) override
    {
        SurfacePainter{SurfaceRole::Panel}.paint(p);
        auto& c = p.canvas();
        const auto b = p.bounds();
        c.clipRect(b.x, b.y, b.width, b.height);
        if (p.theme().fontPath.empty() || !c.setFont(p.theme().fontPath, p.theme().fontSize))
            return;
        float y = b.y + 16;
        c.setFillColor(p.theme().text);
        if (!title.empty())
        {
            c.fillText(title, b.x + 20, y);
            y += c.lineHeight() + 12;
        }
        TextLayoutOptions options;
        options.maxWidth = std::max(1.0f, b.width - 40);
        c.clipRect(b.x, b.y, b.width, std::max(0.0f, b.height - (title.empty() ? 0 : 74)));
        c.fillText(message, b.x + 20, y, options);
    }
};
struct Backdrop final : WidgetPainter
{
    void paint(PaintContext& p) override
    {
        p.fillRect(p.bounds(), {0, 0, 0, .45f});
    }
};
struct Separator final : WidgetPainter
{
    void paint(PaintContext& p) override
    {
        auto b = p.bounds();
        p.fillRect({b.x + 8, b.y + b.height * .5f, std::max(0.0f, b.width - 16), 1}, p.theme().border);
    }
};
} // namespace
struct OverlayManager::State : std::enable_shared_from_this<OverlayManager::State>
{
    enum class Kind
    {
        Popup,
        Dialog
    };
    struct Entry
    {
        OverlayId id;
        Kind kind;
        WidgetId owner, root, panel;
        Bounds placed;
        bool arranged = false;
        Size lastSize;
        std::optional<Vec2> point;
        PopupOptions popup;
        DialogOptions dialog;
        std::vector<WidgetId> choices;
        std::vector<std::unique_ptr<Button>> buttons;
        std::unique_ptr<ScrollView> scroll;
    };
    struct Tip
    {
        WidgetId owner;
        std::string text;
        TooltipOptions options;
    };
    Context& ui;
    Bounds viewport;
    bool prepared = false, destroying = false, tipShown = false;
    std::vector<std::shared_ptr<Entry>> entries;
    std::vector<Tip> tips;
    WidgetId tipRoot, tipPanel, tipOwner, suppressedTip;
    explicit State(Context& context) : ui(context) {}
    std::shared_ptr<Entry> find(OverlayId id) const
    {
        for (auto& e : entries)
            if (e->id == id)
                return e;
        return {};
    }
    void clearTip()
    {
        auto root = tipRoot;
        tipRoot = tipPanel = tipOwner = {};
        tipShown = false;
        if (root)
            ui.remove(root);
    }
    std::optional<Bounds> anchor(WidgetId owner) const
    {
        if (!ui.isAvailable(owner) || !ui.mapFromRoot(owner, {}))
            return {};
        auto b = ui.bounds(owner);
        if (!b)
            return {};
        Bounds result;
        bool first = true;
        float right = 0, bottom = 0;
        for (Vec2 p : {Vec2{b->x, b->y}, Vec2{b->x + b->width, b->y}, Vec2{b->x, b->y + b->height},
                       Vec2{b->x + b->width, b->y + b->height}})
        {
            auto root = ui.mapToRoot(owner, p);
            if (!root)
                return {};
            if (first)
            {
                result.x = right = root->x;
                result.y = bottom = root->y;
                first = false;
            }
            result.x = std::min(result.x, root->x);
            result.y = std::min(result.y, root->y);
            right = std::max(right, root->x);
            bottom = std::max(bottom, root->y);
        }
        result.width = right - result.x;
        result.height = bottom - result.y;
        return result;
    }
    void finish(OverlayId id, DismissReason reason, std::optional<int> result = {})
    {
        auto it = std::find_if(entries.begin(), entries.end(), [id](auto& e) { return e->id == id; });
        if (it == entries.end())
            return;
        std::vector<std::shared_ptr<Entry>> removed(it, entries.end());
        entries.erase(it, entries.end());
        std::exception_ptr failure;
        for (auto p = removed.rbegin(); p != removed.rend(); ++p)
        {
            try
            {
                if (ui.modalRoot() == (*p)->root)
                    ui.popModal();
            }
            catch (...)
            {
                if (!failure)
                    failure = std::current_exception();
            }
            try
            {
                ui.remove((*p)->root);
            }
            catch (...)
            {
                if (!failure)
                    failure = std::current_exception();
            }
        }
        for (auto p = removed.rbegin(); p != removed.rend(); ++p)
        {
            try
            {
                auto& e = **p;
                if (e.kind == Kind::Popup)
                {
                    auto cb = e.popup.onClosed;
                    if (cb)
                        cb(e.id == id ? reason : DismissReason::Programmatic);
                }
                else
                {
                    auto cb = e.dialog.onFinished;
                    if (cb)
                        cb(e.id == id ? result : std::nullopt);
                }
            }
            catch (...)
            {
                if (!failure)
                    failure = std::current_exception();
            }
        }
        if (failure)
            std::rethrow_exception(failure);
    }
    void choose(OverlayId id, std::size_t index)
    {
        auto e = find(id);
        if (!e)
            return;
        if (e->kind == Kind::Dialog)
        {
            if (index < e->dialog.actions.size() && e->dialog.actions[index].enabled)
                finish(id, DismissReason::Selected, e->dialog.actions[index].value);
        }
        else if (index < e->popup.items.size() && e->popup.items[index].enabled && !e->popup.items[index].separator)
        {
            auto callback = e->popup.onSelected;
            finish(id, DismissReason::Selected);
            if (callback)
                callback(index);
        }
    }
    EventReply event(OverlayId id, const Event& event)
    {
        auto e = find(id);
        if (!e)
            return {};
        if (event.type == EventType::WindowFocusLost)
        {
            if (e->kind == Kind::Popup)
                finish(id, DismissReason::WindowFocusLost);
            return {};
        }
        if (event.type == EventType::PointerDown)
        {
            if (e->kind == Kind::Popup && !e->placed.contains(event.x, event.y))
                finish(id, DismissReason::OutsidePointer);
            return {true};
        }
        if (event.type == EventType::KeyDown || event.type == EventType::KeyRepeat)
        {
            if (event.key == Key::Escape)
            {
                if (e->kind == Kind::Dialog && e->dialog.cancelAction)
                    choose(id, *e->dialog.cancelAction);
                else
                    finish(id, DismissReason::Escape);
                return {true};
            }
            if (e->kind == Kind::Dialog && event.key == Key::Enter && e->dialog.defaultAction)
            {
                choose(id, *e->dialog.defaultAction);
                return {true};
            }
            if (e->kind == Kind::Popup &&
                (event.key == Key::Up || event.key == Key::Down || event.key == Key::Home || event.key == Key::End))
            {
                std::vector<WidgetId> enabled;
                for (std::size_t i = 0; i < e->choices.size(); ++i)
                    if (e->popup.items[i].enabled && !e->popup.items[i].separator)
                        enabled.push_back(e->choices[i]);
                if (!enabled.empty())
                {
                    auto it = std::find(enabled.begin(), enabled.end(), ui.focused());
                    std::size_t index = it == enabled.end() ? 0 : std::size_t(it - enabled.begin());
                    if (event.key == Key::Home)
                        index = 0;
                    else if (event.key == Key::End || (event.key == Key::Up && it == enabled.end()))
                        index = enabled.size() - 1;
                    else if (it != enabled.end())
                        index = (index + (event.key == Key::Up ? enabled.size() - 1 : 1)) % enabled.size();
                    ui.focus(enabled[index]);
                    e->scroll->scrollToVisible(enabled[index]);
                }
                return {true};
            }
        }
        return {};
    }
    void begin(const std::shared_ptr<Entry>& e)
    {
        if (!prepared || destroying)
            throw std::logic_error("Prepare a live OverlayManager before opening overlays");
        if (ui.modalRoot() && (entries.empty() || ui.modalRoot() != entries.back()->root))
            throw std::logic_error("OverlayManager cannot mix with an external modal scope");
        clearTip();
        std::weak_ptr<State> weak = shared_from_this();
        Widget root;
        root.parent = ui.modalRoot();
        root.bounds = viewport;
        root.overlay = true;
        root.clipChildren = true;
        root.focusable = true;
        if (e->kind == Kind::Dialog)
            root.painter = std::make_shared<Backdrop>();
        root.onEvent = [weak, id = e->id](Context&, WidgetId, const Event& event)
        {
            if (auto s = weak.lock())
                return s->event(id, event);
            return EventReply{};
        };
        e->root = ui.add(root);
        entries.push_back(e);
    }
    void position(const std::shared_ptr<Entry>& e, bool arrange)
    {
        if (ui.bounds(e->root) != viewport)
            ui.setBounds(e->root, viewport);
        if (e->kind == Kind::Popup)
        {
            auto a = anchor(e->owner);
            if (!a)
            {
                finish(e->id, DismissReason::OwnerUnavailable);
                return;
            }
            float height = 8;
            for (auto& item : e->popup.items)
                height += item.separator ? 8 : e->popup.rowHeight;
            height = std::min(height, e->popup.maximumHeight);
            const float x = e->point ? e->point->x : a->x;
            float y = e->point ? e->point->y : a->y + a->height;
            if (y + height > viewport.y + viewport.height - 8 && (e->point || a->y - height >= viewport.y + 8))
                y = (e->point ? e->point->y : a->y) - height;
            const auto previous = e->placed;
            e->placed = fit(viewport, {e->popup.width, height}, x, y);
            if (previous != e->placed || !e->arranged)
                ui.setTransform(e->panel, Mat3x3::translation(e->placed.x, e->placed.y));
            const Bounds local{0, 0, e->placed.width, e->placed.height};
            if (ui.bounds(e->panel) != local)
                ui.setBounds(e->panel, local);
            if (arrange)
            {
                const Size size{e->placed.width, e->placed.height};
                const bool resized = !e->arranged || size != e->lastSize;
                ui.layout(e->panel, {0, 0, e->placed.width, e->placed.height});
                if (resized && ui.isDescendant(ui.focused(), e->panel))
                    e->scroll->scrollToVisible(ui.focused());
                e->arranged = true;
                e->lastSize = size;
            }
        }
        else
        {
            const auto previous = e->placed;
            e->placed = fit(viewport, e->dialog.preferredSize,
                            viewport.x + (viewport.width - e->dialog.preferredSize.width) * .5f,
                            viewport.y + (viewport.height - e->dialog.preferredSize.height) * .5f);
            if (previous == e->placed && e->arranged)
                return;
            e->arranged = arrange;
            ui.setBounds(e->panel, {0, 0, e->placed.width, e->placed.height});
            ui.setTransform(e->panel, Mat3x3::translation(e->placed.x, e->placed.y));
            const float gap = 8, count = float(e->buttons.size());
            const float width = count > 0 ? std::max(0.0f, (e->placed.width - 32 - gap * (count - 1)) / count) : 0;
            for (std::size_t i = 0; i < e->buttons.size(); ++i)
            {
                ui.setBounds(e->buttons[i]->id(), {0, 0, width, std::min(44.0f, e->placed.height)});
                ui.setTransform(e->buttons[i]->id(), Mat3x3::translation(16 + float(i) * (width + gap),
                                                                         std::max(0.0f, e->placed.height - 60)));
            }
        }
    }
    OverlayId popup(WidgetId owner, std::optional<Vec2> point, PopupOptions options)
    {
        positive(options.width);
        positive(options.rowHeight);
        positive(options.maximumHeight);
        if (options.selected && *options.selected >= options.items.size())
            throw std::invalid_argument("Popup selection is out of range");
        if (!anchor(owner))
            throw std::invalid_argument("Popup owner must be available");
        if (ui.modalRoot() && !ui.isDescendant(owner, ui.modalRoot()))
            throw std::invalid_argument("Popup owner is outside the active modal scope");
        if (point && (!std::isfinite(point->x) || !std::isfinite(point->y)))
            throw std::invalid_argument("Context menu position must be finite");
        auto e = std::make_shared<Entry>();
        e->id = nextId();
        e->kind = Kind::Popup;
        e->owner = owner;
        e->point = point;
        e->popup = std::move(options);
        begin(e);
        try
        {
            ScrollOptions scrollOptions;
            scrollOptions.focusable = false;
            e->scroll = std::make_unique<ScrollView>(ui, e->root, scrollOptions);
            e->panel = e->scroll->id();
            e->scroll->setContentLayout(std::make_shared<ColumnLayout>(Padding{4, 4, 4, 4}, 0.0f));
            std::weak_ptr<State> weak = shared_from_this();
            Widget list;
            list.parent = e->scroll->content();
            list.layout = std::make_shared<ColumnLayout>();
            list.onEvent = [weak, id = e->id](Context&, WidgetId, const Event& event)
            {
                if (auto s = weak.lock())
                    return s->event(id, event);
                return EventReply{};
            };
            const auto listId = ui.add(list);
            for (std::size_t i = 0; i < e->popup.items.size(); ++i)
            {
                const auto& item = e->popup.items[i];
                WidgetId id;
                if (item.separator)
                {
                    Widget w;
                    w.parent = listId;
                    w.painter = std::make_shared<Separator>();
                    id = ui.add(w);
                }
                else
                {
                    auto button = std::make_unique<Button>(ui, listId,
                                                           ButtonOptions{item.text,
                                                                         {e->popup.width, e->popup.rowHeight},
                                                                         [weak, id = e->id, i]
                                                                         {
                                                                             if (auto s = weak.lock())
                                                                                 s->choose(id, i);
                                                                         }});
                    id = button->id();
                    ui.setEnabled(id, item.enabled);
                    e->buttons.push_back(std::move(button));
                }
                e->choices.push_back(id);
                LayoutHints hints;
                hints.height = item.separator ? 8.0f : e->popup.rowHeight;
                ui.setLayoutHints(id, hints);
            }
            position(e, false);
            ui.pushModal(e->root);
            std::size_t selected = e->popup.selected.value_or(0);
            for (std::size_t i = 0; i < e->choices.size(); ++i)
            {
                const auto index = (selected + i) % e->choices.size();
                if (e->popup.items[index].enabled && !e->popup.items[index].separator)
                {
                    ui.focus(e->choices[index]);
                    break;
                }
            }
        }
        catch (...)
        {
            auto failure = std::current_exception();
            try
            {
                finish(e->id, DismissReason::Programmatic);
            }
            catch (...)
            {
            }
            std::rethrow_exception(failure);
        }
        return e->id;
    }
    OverlayId dialog(DialogOptions options)
    {
        positive(options.preferredSize.width);
        positive(options.preferredSize.height);
        for (auto index : {options.defaultAction, options.cancelAction})
            if (index && (*index >= options.actions.size() || !options.actions[*index].enabled))
                throw std::invalid_argument("Dialog action index must identify an enabled action");
        auto e = std::make_shared<Entry>();
        e->id = nextId();
        e->kind = Kind::Dialog;
        e->dialog = std::move(options);
        begin(e);
        try
        {
            Widget panel;
            panel.parent = e->root;
            panel.clipChildren = true;
            panel.painter = std::make_shared<TextPanel>(e->dialog.title, e->dialog.message);
            e->panel = ui.add(panel);
            std::weak_ptr<State> weak = shared_from_this();
            for (std::size_t i = 0; i < e->dialog.actions.size(); ++i)
            {
                const auto& a = e->dialog.actions[i];
                auto button = std::make_unique<Button>(ui, e->panel,
                                                       ButtonOptions{a.label,
                                                                     {120, 44},
                                                                     [weak, id = e->id, i]
                                                                     {
                                                                         if (auto s = weak.lock())
                                                                             s->choose(id, i);
                                                                     }});
                ui.setEnabled(button->id(), a.enabled);
                e->choices.push_back(button->id());
                e->buttons.push_back(std::move(button));
            }
            position(e, false);
            ui.pushModal(e->root);
            if (e->dialog.defaultAction)
                ui.focus(e->choices[*e->dialog.defaultAction]);
            else
                for (auto id : e->choices)
                    if (ui.focus(id))
                        break;
        }
        catch (...)
        {
            auto failure = std::current_exception();
            try
            {
                finish(e->id, DismissReason::Programmatic);
            }
            catch (...)
            {
            }
            std::rethrow_exception(failure);
        }
        return e->id;
    }
    void tipTimer()
    {
        auto it = std::find_if(tips.begin(), tips.end(), [&](auto& t) { return t.owner == tipOwner; });
        if (it == tips.end() || !ui.isAvailable(tipOwner))
        {
            clearTip();
            return;
        }
        if (tipShown)
        {
            suppressedTip = tipOwner;
            clearTip();
            return;
        }
        tipShown = true;
        ui.setVisible(tipPanel, true);
        ui.requestWakeup(tipRoot, ui.now() + it->options.duration);
    }
    void update(Bounds area)
    {
        positive(area.width);
        positive(area.height);
        if (!std::isfinite(area.x) || !std::isfinite(area.y) || !std::isfinite(area.x + area.width) ||
            !std::isfinite(area.y + area.height))
            throw std::invalid_argument("Overlay viewport must be finite");
        viewport = area;
        prepared = true;
        std::erase_if(tips, [&](auto& tip) { return !ui.contains(tip.owner); });
        const auto snapshot = entries;
        for (auto& e : snapshot)
        {
            if (!find(e->id))
                continue;
            if (!ui.contains(e->root) || (e->owner && !ui.isAvailable(e->owner)))
                finish(e->id, DismissReason::OwnerUnavailable);
            else
                position(e, true);
        }
        if (!entries.empty())
        {
            clearTip();
            return;
        }
        WidgetId candidate;
        for (auto& t : tips)
            if (ui.isAvailable(t.owner) && ui.isDescendant(ui.hovered(), t.owner))
            {
                candidate = t.owner;
                break;
            }
        if (!candidate)
            for (auto& t : tips)
                if (ui.isAvailable(t.owner) && ui.isDescendant(ui.focused(), t.owner))
                {
                    candidate = t.owner;
                    break;
                }
        if (candidate != suppressedTip)
            suppressedTip = {};
        if (candidate != tipOwner)
            clearTip();
        if (!candidate || candidate == suppressedTip)
            return;
        auto it = std::find_if(tips.begin(), tips.end(), [&](auto& t) { return t.owner == candidate; });
        if (!tipRoot)
        {
            tipOwner = candidate;
            std::weak_ptr<State> weak = shared_from_this();
            Widget root;
            root.overlay = true;
            root.hitTestVisible = false;
            root.onEvent = [weak](Context&, WidgetId, const Event& event)
            {
                if (auto s = weak.lock())
                {
                    if (event.type == EventType::KeyDown && event.key == Key::Escape && s->tipShown)
                    {
                        s->suppressedTip = s->tipOwner;
                        s->clearTip();
                        return EventReply{true};
                    }
                    if (event.type == EventType::Timer)
                        s->tipTimer();
                    else if (event.type == EventType::WindowFocusLost)
                    {
                        s->suppressedTip = s->tipOwner;
                        s->clearTip();
                    }
                }
                return EventReply{};
            };
            tipRoot = ui.add(root);
            Widget panel;
            panel.parent = tipRoot;
            panel.visible = false;
            panel.painter = std::make_shared<TextPanel>("", it->text);
            tipPanel = ui.add(panel);
            ui.requestWakeup(tipRoot, ui.now() + it->options.delay);
        }
        auto a = anchor(candidate);
        if (!a)
        {
            clearTip();
            return;
        }
        auto box = fit(viewport, it->options.preferredSize, a->x, a->y + a->height + 8);
        if (box.y < a->y + a->height && a->y - it->options.preferredSize.height >= viewport.y)
            box = fit(viewport, it->options.preferredSize, a->x, a->y - it->options.preferredSize.height - 8);
        if (ui.bounds(tipPanel) != box)
            ui.setBounds(tipPanel, box);
    }
};
OverlayManager::OverlayManager(Context& ui) : state_(std::make_shared<State>(ui)) {}
OverlayManager::~OverlayManager()
{
    state_->destroying = true;
    try
    {
        state_->clearTip();
        closeAll();
    }
    catch (...)
    {
    }
}
void OverlayManager::update(Bounds viewport)
{
    auto keep = state_;
    keep->update(viewport);
}
OverlayId OverlayManager::openPopup(WidgetId owner, PopupOptions options)
{
    auto keep = state_;
    return keep->popup(owner, {}, std::move(options));
}
OverlayId OverlayManager::openContextMenu(WidgetId owner, Vec2 point, PopupOptions options)
{
    auto keep = state_;
    return keep->popup(owner, point, std::move(options));
}
OverlayId OverlayManager::openDialog(DialogOptions options)
{
    auto keep = state_;
    return keep->dialog(std::move(options));
}
void OverlayManager::close(OverlayId id, DismissReason reason)
{
    auto keep = state_;
    keep->finish(id, reason);
}
void OverlayManager::closeAll()
{
    auto keep = state_;
    keep->suppressedTip = keep->tipOwner;
    keep->clearTip();
    if (!keep->entries.empty())
        keep->finish(keep->entries.front()->id, DismissReason::Programmatic);
}
bool OverlayManager::contains(OverlayId id) const
{
    auto e = state_->find(id);
    return e && state_->ui.contains(e->root);
}
WidgetId OverlayManager::root(OverlayId id) const
{
    auto e = state_->find(id);
    return e ? e->root : WidgetId{};
}
std::optional<Bounds> OverlayManager::contentBounds(OverlayId id) const
{
    auto e = state_->find(id);
    return e ? std::optional(e->placed) : std::nullopt;
}
void OverlayManager::setTooltip(WidgetId owner, std::string text, TooltipOptions options)
{
    positive(options.preferredSize.width);
    positive(options.preferredSize.height);
    if (!std::isfinite(options.delay) || options.delay < 0 || !std::isfinite(options.duration) ||
        options.duration <= 0 || !state_->ui.contains(owner))
        throw std::invalid_argument("Invalid tooltip options/owner");
    removeTooltip(owner);
    state_->tips.push_back({owner, std::move(text), options});
}
void OverlayManager::removeTooltip(WidgetId owner)
{
    auto keep = state_;
    if (keep->suppressedTip == owner)
        keep->suppressedTip = {};
    if (keep->tipOwner == owner)
        keep->clearTip();
    std::erase_if(keep->tips, [owner](auto& t) { return t.owner == owner; });
}
bool OverlayManager::tooltipVisible() const
{
    return state_->tipShown;
}
} // namespace vectorgl::ui
