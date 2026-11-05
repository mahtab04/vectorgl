#include "vectorgl/ui/context.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace vectorgl::ui
{
namespace
{
std::atomic<std::uint64_t> nextContext{1};
class SteadyClock final : public Clock
{
public:
    double now() const override
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
};
void validateBounds(Bounds b)
{
    if (!std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.width) || !std::isfinite(b.height) ||
        b.width < 0 || b.height < 0 || !std::isfinite(b.x + b.width) || !std::isfinite(b.y + b.height))
        throw std::invalid_argument("UI bounds must be finite with nonnegative dimensions");
}
bool pointerEvent(EventType type)
{
    return type == EventType::PointerDown || type == EventType::PointerUp || type == EventType::PointerMove ||
           type == EventType::Wheel;
}
} // namespace

struct Context::Impl
{
    struct Registration
    {
        Widget widget;
        bool pending = false, removed = false;
        std::optional<double> wakeup;
    };
    struct Slot
    {
        std::uint64_t generation = 0;
        std::unique_ptr<Registration> item;
    };
    struct Capture
    {
        WidgetId id;
        std::uint64_t pointer;
        PointerButton button;
    };
    struct Modal
    {
        WidgetId root, previousFocus;
    };

    Context& owner;
    Services services;
    const std::uint64_t identity = nextContext.fetch_add(1);
    std::vector<Slot> slots;
    std::vector<WidgetId> order;
    std::vector<Modal> modals;
    WidgetId focus, desiredFocus;
    bool focusPending = false, focusFirst = false, processing = false;
    std::optional<Capture> capture;
    std::optional<CancelReason> cancellation;
    std::exception_ptr callbackError;
    Invalidation dirty = Invalidation::All;

    Impl(Context& context, Services input) : owner(context), services(std::move(input))
    {
        if (!identity)
            throw std::overflow_error("UI context identity exhausted");
        if (!services.clock)
            services.clock = std::make_shared<SteadyClock>();
    }

    Registration* get(WidgetId id, bool includeRemoved = false) const
    {
        if (id.context != identity || id.index >= slots.size() || slots[id.index].generation != id.generation)
            return nullptr;
        auto* item = slots[id.index].item.get();
        return item && (!item->removed || includeRemoved) ? item : nullptr;
    }

    bool descendant(WidgetId id, WidgetId root) const
    {
        for (auto* item = get(id, true); item; item = get(id, true))
        {
            if (id == root)
                return true;
            id = item->widget.parent;
        }
        return false;
    }

    bool available(WidgetId id, bool includePending = false) const
    {
        if (!get(id))
            return false;
        for (auto* item = get(id); item; item = get(item->widget.parent))
            if ((!includePending && item->pending) || !item->widget.visible || !item->widget.enabled)
                return false;
        return true;
    }

    bool eligible(WidgetId id) const
    {
        return available(id) && (modals.empty() || descendant(id, modals.back().root));
    }

    bool focusable(WidgetId id) const
    {
        auto* item = get(id);
        return item && item->widget.focusable && eligible(id);
    }

    std::vector<WidgetId> focusList() const
    {
        std::vector<WidgetId> result;
        for (auto id : order)
            if (focusable(id))
                result.push_back(id);
        return result;
    }

    WidgetId firstFocus() const
    {
        for (auto id : order)
            if (focusable(id))
                return id;
        return {};
    }

    void requestFocus(WidgetId id)
    {
        desiredFocus = id;
        focusPending = true;
        focusFirst = false;
    }

    EventReply invoke(WidgetId id, const Event& event, bool notification = false)
    {
        auto* item = get(id, notification);
        if (!item || !item->widget.onEvent)
            return {};
        // Registrations live on the heap and removal is deferred until callbacks
        // unwind. Keep the stored callable itself so mutable captures persist.
        auto& handler = item->widget.onEvent;
        try
        {
            auto reply = handler(owner, id, event);
            if (!notification && (reply.handled || reply.requestFocus || reply.capturePointer))
                dirty = dirty | Invalidation::Paint;
            if (!notification && eligible(id))
            {
                if (reply.requestFocus && focusable(id))
                    requestFocus(id);
                if (reply.capturePointer && event.type == EventType::PointerDown &&
                    event.button != PointerButton::None && !capture)
                    capture = Capture{id, event.pointer, event.button};
            }
            return notification ? EventReply{} : reply;
        }
        catch (...)
        {
            if (!callbackError)
                callbackError = std::current_exception();
            cancellation = CancelReason::HandlerFailed;
            return {true};
        }
    }

    void settle()
    {
        // Notifications can request more changes; cap pathological callback loops.
        for (int transition = 0; transition < 1024; ++transition)
        {
            for (auto& slot : slots)
                if (slot.item)
                    slot.item->pending = false;
            auto invalid =
                std::find_if(modals.begin(), modals.end(), [&](const Modal& modal) { return !available(modal.root); });
            if (invalid != modals.end())
            {
                const auto previous = invalid->previousFocus;
                modals.erase(invalid, modals.end());
                requestFocus(focusable(previous) ? previous : firstFocus());
                cancellation = CancelReason::ModalChanged;
            }
            if (capture && (cancellation || !eligible(capture->id)))
            {
                const auto previous = *capture;
                capture.reset();
                Event event;
                event.type = EventType::Cancel;
                event.pointer = previous.pointer;
                event.button = previous.button;
                event.reason = cancellation.value_or(CancelReason::Unavailable);
                cancellation.reset();
                invoke(previous.id, event, true);
                dirty = dirty | Invalidation::Paint;
                continue;
            }
            cancellation.reset();
            if (focusPending || (focus && !focusable(focus)))
            {
                WidgetId next = focusPending && !focusFirst ? desiredFocus : firstFocus();
                focusPending = false;
                focusFirst = false;
                if (!focusable(next))
                    next = {};
                if (next != focus)
                {
                    const auto previous = focus;
                    focus = {};
                    Event event;
                    event.type = EventType::FocusLost;
                    invoke(previous, event, true);
                    // A FocusLost callback can redirect focus or remove the next widget.
                    if (focusPending)
                        continue;
                    if (focusable(next))
                    {
                        focus = next;
                        event.type = EventType::FocusGained;
                        invoke(next, event, true);
                    }
                    dirty = dirty | Invalidation::Paint | Invalidation::Semantics;
                    continue;
                }
            }
            // Storage is reclaimed only after lost-focus/cancel notifications.
            for (auto& slot : slots)
                if (slot.item && slot.item->removed)
                    slot.item.reset();
            std::erase_if(order, [&](WidgetId id) { return !get(id); });
            return;
        }
        capture.reset();
        focus = {};
        focusPending = false;
        for (auto& slot : slots)
            if (slot.item && slot.item->removed)
                slot.item.reset();
        std::erase_if(order, [&](WidgetId id) { return !get(id); });
        throw std::logic_error("UI focus/capture notification cycle exceeded 1024 transitions");
    }

    template <class Function> void transaction(Function&& function)
    {
        if (processing)
        {
            function();
            return;
        }
        processing = true;
        try
        {
            function();
            settle();
        }
        catch (...)
        {
            processing = false;
            callbackError = nullptr;
            throw;
        }
        processing = false;
        if (auto error = std::exchange(callbackError, nullptr))
            std::rethrow_exception(error);
    }

    WidgetId hit(WidgetId parent, const Event& event) const
    {
        // Visit children in reverse paint order, then their parent. This initial
        // fixed-rectangle tree is intentionally independent of Scene::pick().
        for (auto it = order.rbegin(); it != order.rend(); ++it)
        {
            auto* item = get(*it);
            if (!item || item->widget.parent != parent || !available(*it))
                continue;
            const bool inside = item->widget.bounds.contains(event.x, event.y);
            if (!item->widget.clipChildren || inside)
                if (auto child = hit(*it, event))
                    return child;
            if (inside && eligible(*it))
                return *it;
        }
        return {};
    }

    bool route(WidgetId target, const Event& event)
    {
        while (auto* item = get(target))
        {
            const auto parent = item->widget.parent;
            if (!eligible(target))
                break;
            const auto reply = invoke(target, event);
            if (reply.handled || reply.requestFocus || reply.capturePointer)
                return true;
            if (!modals.empty() && target == modals.back().root)
                break;
            target = parent;
        }
        return false;
    }
};

Context::Context(Services services) : impl_(std::make_unique<Impl>(*this, std::move(services))) {}
Context::~Context() = default;

WidgetId Context::add(Widget widget)
{
    validateBounds(widget.bounds);
    if (widget.parent && !contains(widget.parent))
        throw std::invalid_argument("UI parent is stale or belongs to another Context");
    int depth = 0;
    for (auto* parent = impl_->get(widget.parent); parent; parent = impl_->get(parent->widget.parent))
        if (++depth >= 256)
            throw std::length_error("UI tree exceeds 256 levels");
    WidgetId id;
    impl_->transaction(
        [&]
        {
            std::size_t index = 0;
            while (index < impl_->slots.size() &&
                   (impl_->slots[index].item ||
                    impl_->slots[index].generation == std::numeric_limits<std::uint64_t>::max()))
                ++index;
            if (index >= std::numeric_limits<std::uint32_t>::max())
                throw std::length_error("UI registration limit exceeded");
            auto registration = std::make_unique<Impl::Registration>();
            registration->widget = std::move(widget);
            registration->pending = true;
            if (index == impl_->slots.size())
                impl_->slots.emplace_back();
            id = {impl_->identity, impl_->slots[index].generation + 1, static_cast<std::uint32_t>(index)};
            impl_->order.push_back(id);
            impl_->slots[index].generation = id.generation;
            impl_->slots[index].item = std::move(registration);
            invalidate(Invalidation::All);
        });
    return id;
}

bool Context::contains(WidgetId id) const
{
    return impl_->get(id) != nullptr;
}
void Context::remove(WidgetId id)
{
    impl_->transaction(
        [&]
        {
            if (!contains(id))
                return;
            // Determine descendants before marking parents, since IDs are invalidated immediately.
            for (auto candidate : impl_->order)
                if (impl_->descendant(candidate, id))
                    if (auto* item = impl_->get(candidate, true))
                        item->removed = true;
            invalidate(Invalidation::All);
        });
}
void Context::setBounds(WidgetId id, Bounds bounds)
{
    validateBounds(bounds);
    impl_->transaction(
        [&]
        {
            if (auto* item = impl_->get(id))
            {
                item->widget.bounds = bounds;
                invalidate(Invalidation::Arrange | Invalidation::Semantics);
            }
        });
}
void Context::setVisible(WidgetId id, bool visible)
{
    impl_->transaction(
        [&]
        {
            if (auto* item = impl_->get(id))
            {
                item->widget.visible = visible;
                invalidate(Invalidation::All);
            }
        });
}
void Context::setEnabled(WidgetId id, bool enabled)
{
    impl_->transaction(
        [&]
        {
            if (auto* item = impl_->get(id))
            {
                item->widget.enabled = enabled;
                invalidate(Invalidation::Paint | Invalidation::Semantics);
            }
        });
}

bool Context::dispatch(const Event& event)
{
    if (impl_->processing)
        throw std::logic_error("UI dispatch cannot be reentered from a callback");
    if (event.type == EventType::FocusGained || event.type == EventType::FocusLost || event.type == EventType::Timer)
        throw std::invalid_argument("UI notification events are generated by Context");
    if (pointerEvent(event.type) && (!std::isfinite(event.x) || !std::isfinite(event.y) ||
                                     !std::isfinite(event.wheelX) || !std::isfinite(event.wheelY)))
        throw std::invalid_argument("UI pointer coordinates/deltas must be finite");
    bool handled = false;
    impl_->transaction(
        [&]
        {
            if (event.type == EventType::WindowFocusLost || event.type == EventType::PointerLeave ||
                event.type == EventType::Cancel ||
                ((event.type == EventType::KeyDown || event.type == EventType::KeyRepeat) && event.key == Key::Escape &&
                 impl_->capture))
            {
                handled = impl_->capture.has_value() || !impl_->modals.empty();
                impl_->cancellation = event.type == EventType::WindowFocusLost ? CancelReason::WindowFocusLost
                                      : event.type == EventType::PointerLeave  ? CancelReason::PointerLeftWindow
                                                                               : event.reason;
                if (event.type == EventType::WindowFocusLost)
                    impl_->requestFocus({});
                return;
            }
            if ((event.type == EventType::KeyDown || event.type == EventType::KeyRepeat ||
                 event.type == EventType::KeyUp) &&
                event.key == Key::Tab && !event.modifiers.control && !event.modifiers.alt && !event.modifiers.meta)
            {
                const auto list = impl_->focusList();
                if (!list.empty())
                {
                    if (event.type != EventType::KeyUp)
                    {
                        const auto it = std::find(list.begin(), list.end(), impl_->focus);
                        const auto index = it == list.end() ? (event.modifiers.shift ? list.size() - 1 : 0)
                                                            : (static_cast<std::size_t>(it - list.begin()) +
                                                               (event.modifiers.shift ? list.size() - 1 : 1)) %
                                                                  list.size();
                        impl_->requestFocus(list[index]);
                    }
                    handled = true;
                    return;
                }
            }
            WidgetId target = impl_->focus;
            if (pointerEvent(event.type))
            {
                target = impl_->capture && impl_->capture->pointer == event.pointer && event.type != EventType::Wheel
                             ? impl_->capture->id
                             : impl_->hit({}, event);
                if (event.type == EventType::PointerDown)
                    for (auto id = target; auto* item = impl_->get(id); id = item->widget.parent)
                        if (impl_->focusable(id))
                        {
                            impl_->requestFocus(id);
                            break;
                        }
            }
            handled = impl_->route(target, event) || !impl_->modals.empty();
            if (event.type == EventType::PointerUp && !impl_->callbackError && impl_->capture &&
                impl_->capture->pointer == event.pointer && impl_->capture->button == event.button)
            {
                impl_->capture.reset();
                invalidate();
            }
        });
    return handled;
}

bool Context::focus(WidgetId id)
{
    if (id)
    {
        auto* item = impl_->get(id);
        if (!item || !item->widget.focusable || !impl_->available(id, true) ||
            (!impl_->modals.empty() && !impl_->descendant(id, impl_->modals.back().root)))
            return false;
    }
    impl_->transaction([&] { impl_->requestFocus(id); });
    return true;
}
WidgetId Context::focused() const
{
    return impl_->focus;
}
WidgetId Context::captured() const
{
    return impl_->capture ? impl_->capture->id : WidgetId{};
}
void Context::cancelCapture(CancelReason reason)
{
    impl_->transaction([&] { impl_->cancellation = reason; });
}
void Context::pushModal(WidgetId root)
{
    if (!impl_->available(root, true) ||
        (!impl_->modals.empty() && !impl_->descendant(root, impl_->modals.back().root)))
        throw std::invalid_argument("UI modal root must be available inside the current scope");
    impl_->transaction(
        [&]
        {
            impl_->modals.push_back({root, impl_->focus});
            impl_->cancellation = CancelReason::ModalChanged;
            impl_->requestFocus(impl_->firstFocus());
            impl_->focusFirst = true; // Include registrations published when this callback unwinds.
            invalidate(Invalidation::Paint | Invalidation::Semantics);
        });
}
void Context::popModal()
{
    if (impl_->modals.empty())
        throw std::logic_error("UI modal stack is empty");
    impl_->transaction(
        [&]
        {
            auto previous = impl_->modals.back().previousFocus;
            impl_->modals.pop_back();
            impl_->cancellation = CancelReason::ModalChanged;
            impl_->requestFocus(impl_->focusable(previous) ? previous : impl_->firstFocus());
            invalidate(Invalidation::Paint | Invalidation::Semantics);
        });
}
WidgetId Context::modalRoot() const
{
    return impl_->modals.empty() ? WidgetId{} : impl_->modals.back().root;
}
void Context::invalidate(Invalidation flags)
{
    const auto bits = static_cast<unsigned>(flags);
    if (bits & static_cast<unsigned>(Invalidation::Measure))
        flags = flags | Invalidation::Arrange;
    if (static_cast<unsigned>(flags) & static_cast<unsigned>(Invalidation::Arrange))
        flags = flags | Invalidation::Paint;
    impl_->dirty = impl_->dirty | flags;
}
Invalidation Context::takeInvalidation()
{
    return std::exchange(impl_->dirty, Invalidation::None);
}
double Context::now() const
{
    const double time = impl_->services.clock->now();
    if (!std::isfinite(time))
        throw std::invalid_argument("UI clock must return finite monotonic seconds");
    return time;
}
void Context::requestWakeup(WidgetId id, double deadline)
{
    if (!std::isfinite(deadline))
        throw std::invalid_argument("UI deadline must be finite");
    if (auto* item = impl_->get(id))
        item->wakeup = deadline;
}
void Context::cancelWakeup(WidgetId id)
{
    if (auto* item = impl_->get(id))
        item->wakeup.reset();
}
std::optional<double> Context::nextWakeup() const
{
    std::optional<double> next;
    for (auto id : impl_->order)
        if (auto* item = impl_->get(id); item && impl_->eligible(id) && item->wakeup)
            if (!next || *item->wakeup < *next)
                next = item->wakeup;
    return next;
}
void Context::pumpTimers()
{
    if (impl_->processing)
        throw std::logic_error("UI timers cannot be pumped from a callback");
    const double time = now();
    impl_->transaction(
        [&]
        {
            std::vector<std::pair<WidgetId, double>> due;
            for (auto id : impl_->order)
                if (auto* item = impl_->get(id); item && impl_->eligible(id) && item->wakeup && *item->wakeup <= time)
                {
                    due.emplace_back(id, *item->wakeup);
                }
            for (auto [id, deadline] : due)
                if (auto* item = impl_->get(id); item && impl_->eligible(id) && item->wakeup == deadline)
                {
                    item->wakeup.reset();
                    Event event;
                    event.type = EventType::Timer;
                    impl_->invoke(id, event, true);
                    invalidate();
                }
        });
}
const Services& Context::services() const
{
    return impl_->services;
}
} // namespace vectorgl::ui
