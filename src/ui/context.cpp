#include "vectorgl/ui/context.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "geometry.hpp"
#include "vectorgl/canvas.hpp"

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
class CanvasScope
{
public:
    explicit CanvasScope(Canvas& canvas) : canvas_(canvas)
    {
        canvas_.save();
    }
    ~CanvasScope()
    {
        if (active_)
            try
            {
                canvas_.restore();
            }
            catch (...)
            {
            }
    }
    void restore()
    {
        active_ = false;
        canvas_.restore();
    }

private:
    Canvas& canvas_;
    bool active_ = true;
};
class MeasurementScope
{
public:
    explicit MeasurementScope(bool& flag) : flag_(flag)
    {
        flag_ = true;
    }
    ~MeasurementScope()
    {
        flag_ = false;
    }

private:
    bool& flag_;
};
} // namespace

struct Context::Impl
{
    struct Registration
    {
        Widget widget;
        Vec2 layoutOffset;
        Size naturalSize;
        struct MeasureCache
        {
            Constraints constraints;
            Size size;
            std::uint64_t revision;
        };
        std::vector<MeasureCache> measures;
        std::optional<Bounds> lastArea;
        std::uint64_t lastRevision = 0, lastGeometry = 0;
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
    WidgetId hover;
    std::optional<Vec2> pointerPosition;
    Theme theme;
    bool focusPending = false, focusFirst = false, processing = false, measuring = false;
    std::uint64_t layoutRevision = 1, geometryRevision = 1;
    LayoutStats layoutStats;

    void requireMutable() const
    {
        if (measuring)
            throw std::logic_error("UI mutation is forbidden during measurement/layout");
    }
    static Mat3x3 localTransform(const Registration& item)
    {
        return Mat3x3::translation(item.layoutOffset.x, item.layoutOffset.y) * item.widget.transform;
    }
    std::optional<Capture> capture;
    struct DragSession
    {
        DragState state;
        std::uint64_t pointer = 0;
        Modifiers modifiers;
        bool update = true;
        std::function<void(const DragResult&)> finished;
    };
    std::shared_ptr<DragSession> drag;
    WidgetId invoking;
    const Event* invocation = nullptr;
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
        return available(id) && detail::unmap(world(id), {}).has_value() &&
               (modals.empty() || descendant(id, modals.back().root));
    }

    Mat3x3 world(WidgetId id, bool includeRemoved = false) const
    {
        std::vector<const Registration*> chain;
        for (auto* item = get(id, includeRemoved); item; item = get(item->widget.parent, includeRemoved))
            chain.push_back(item);
        Mat3x3 result;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
            result = result * localTransform(**it);
        return result;
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
            if (focusable(id) && get(id)->widget.tabStop)
                result.push_back(id);
        return result;
    }

    WidgetId firstFocus() const
    {
        for (auto id : order)
            if (focusable(id) && get(id)->widget.tabStop)
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
            Event mapped = event;
            if (pointerEvent(event.type))
                if (auto point = detail::unmap(world(id), {event.x, event.y}))
                {
                    mapped.localX = point->x;
                    mapped.localY = point->y;
                }
            const auto previousId = invoking;
            const auto* previousEvent = invocation;
            invoking = id;
            invocation = &mapped;
            EventReply reply;
            try
            {
                reply = handler(owner, id, mapped);
            }
            catch (...)
            {
                invoking = previousId;
                invocation = previousEvent;
                throw;
            }
            invoking = previousId;
            invocation = previousEvent;
            if (!notification && reply.remainingWheel)
            {
                auto valid = [](float original, float remaining)
                {
                    return std::isfinite(remaining) && std::abs(remaining) <= std::abs(original) &&
                           (original >= 0 ? remaining >= 0 : remaining <= 0);
                };
                if (event.type != EventType::Wheel || !valid(event.wheelX, reply.remainingWheel->x) ||
                    !valid(event.wheelY, reply.remainingWheel->y))
                    throw std::invalid_argument("Wheel residual must preserve sign and not exceed the input delta");
            }
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

    bool dragNotify(WidgetId id, DragPhase phase, const std::shared_ptr<DragSession>& session)
    {
        auto* item = get(id, phase == DragPhase::Leave);
        if (!item || !item->widget.onDrag)
            return false;
        const auto point = detail::unmap(world(id, phase == DragPhase::Leave), {session->state.x, session->state.y});
        DragEvent event{phase,
                        session->state.source,
                        id,
                        session->state.payload,
                        session->state.x,
                        session->state.y,
                        point ? point->x : 0,
                        point ? point->y : 0,
                        session->modifiers};
        try
        {
            return item->widget.onDrag(owner, id, event);
        }
        catch (...)
        {
            if (!callbackError)
                callbackError = std::current_exception();
            cancellation = CancelReason::HandlerFailed;
            return false;
        }
    }
    void finishDrag(DragOutcome outcome, CancelReason reason = CancelReason::Explicit)
    {
        auto session = std::exchange(drag, {});
        if (!session)
            return;
        const DragResult result{outcome, session->state.source, session->state.target, session->state.payload, reason};
        if (capture && capture->id == session->state.source)
            capture.reset();
        if (session->state.target)
            dragNotify(session->state.target, DragPhase::Leave, session);
        Event cancel;
        cancel.type = EventType::Cancel;
        cancel.pointer = session->pointer;
        cancel.button = PointerButton::Primary;
        cancel.reason = reason;
        invoke(session->state.source, cancel, true);
        if (session->finished)
            try
            {
                session->finished(result);
            }
            catch (...)
            {
                if (!callbackError)
                    callbackError = std::current_exception();
            }
        dirty = dirty | Invalidation::Paint | Invalidation::Semantics;
    }
    WidgetId dragTarget(const std::shared_ptr<DragSession>& session) const
    {
        Event point;
        point.x = session->state.x;
        point.y = session->state.y;
        auto target = hit({}, point);
        for (auto* item = get(target); item && !item->widget.onDrag; item = get(target))
            target = item->widget.parent;
        return eligible(target) ? target : WidgetId{};
    }
    void updateDrag()
    {
        const auto session = drag;
        if (!session)
            return;
        auto target = dragTarget(session);
        if (target != session->state.target)
        {
            const auto old = std::exchange(session->state.target, {});
            session->state.accepted = false;
            if (old)
                dragNotify(old, DragPhase::Leave, session);
            if (drag != session || cancellation || callbackError)
                return;
            if (!eligible(target))
                target = {};
            session->state.target = target;
            if (target)
                session->state.accepted = dragNotify(target, DragPhase::Enter, session);
            session->update = false;
            dirty = dirty | Invalidation::Paint;
        }
        else if (session->update)
        {
            session->update = false;
            if (target)
                session->state.accepted = dragNotify(target, DragPhase::Move, session);
            dirty = dirty | Invalidation::Paint;
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
            auto invalid = std::find_if(modals.begin(), modals.end(), [&](const Modal& modal)
                                        { return !available(modal.root) || !detail::unmap(world(modal.root), {}); });
            if (invalid != modals.end())
            {
                const auto previous = invalid->previousFocus;
                modals.erase(invalid, modals.end());
                requestFocus(focusable(previous) ? previous : firstFocus());
                cancellation = CancelReason::ModalChanged;
            }
            if (drag &&
                (cancellation || !eligible(drag->state.source) || !capture || capture->id != drag->state.source))
            {
                const auto reason = cancellation.value_or(CancelReason::Unavailable);
                cancellation.reset();
                finishDrag(DragOutcome::Cancelled, reason);
                continue;
            }
            if (drag)
            {
                const auto session = drag;
                updateDrag();
                if (cancellation || drag != session || !eligible(session->state.source) ||
                    dragTarget(session) != session->state.target)
                    continue;
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
            Event pointerState;
            if (pointerPosition)
            {
                pointerState.x = pointerPosition->x;
                pointerState.y = pointerPosition->y;
            }
            const auto newHover = pointerPosition ? hit({}, pointerState) : WidgetId{};
            if (hover != newHover)
            {
                hover = newHover;
                dirty = dirty | Invalidation::Paint;
            }
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
                        event.type = EventType::FocusWithin;
                        event.related = next;
                        auto* focusedItem = get(next);
                        for (auto parent = focusedItem ? focusedItem->widget.parent : WidgetId{};
                             auto* ancestor = get(parent);)
                        {
                            const auto nextParent = ancestor->widget.parent;
                            if (!eligible(parent))
                                break;
                            invoke(parent, event, true);
                            if (!modals.empty() && parent == modals.back().root)
                                break;
                            parent = nextParent;
                        }
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
        if (drag)
            finishDrag(DragOutcome::Cancelled, CancelReason::HandlerFailed);
        capture.reset();
        focus = {};
        focusPending = false;
        for (auto& slot : slots)
            if (slot.item && slot.item->removed)
                slot.item.reset();
        std::erase_if(order, [&](WidgetId id) { return !get(id); });
        throw std::logic_error("UI focus/capture/drag notification cycle exceeded 1024 transitions");
    }

    template <class Function> void transaction(Function&& function)
    {
        requireMutable();
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

    WidgetId hit(WidgetId parent, const Event& event, const Mat3x3& inherited = {}) const
    {
        // Visit children in reverse paint order, then their parent. This initial
        // fixed-rectangle tree is intentionally independent of Scene::pick().
        for (auto it = order.rbegin(); it != order.rend(); ++it)
        {
            auto* item = get(*it);
            if (!item || item->widget.parent != parent || !available(*it) || !item->widget.hitTestVisible)
                continue;
            const auto transform = inherited * localTransform(*item);
            const auto point = detail::unmap(transform, {event.x, event.y});
            if (!point)
                continue;
            const bool inside = item->widget.bounds.contains(point->x, point->y);
            if (!item->widget.clipChildren || inside)
                if (auto child = hit(*it, event, transform))
                    return child;
            if (inside && eligible(*it))
                return *it;
        }
        return {};
    }

    bool route(WidgetId target, const Event& event)
    {
        Event remaining = event;
        bool consumed = false;
        while (auto* item = get(target))
        {
            const auto parent = item->widget.parent;
            if (!eligible(target))
                break;
            const auto reply = invoke(target, remaining);
            if (remaining.type == EventType::Wheel)
            {
                consumed |= reply.blockApplication;
                if (reply.remainingWheel)
                {
                    consumed |=
                        reply.remainingWheel->x != remaining.wheelX || reply.remainingWheel->y != remaining.wheelY;
                    remaining.wheelX = reply.remainingWheel->x;
                    remaining.wheelY = reply.remainingWheel->y;
                    if (remaining.wheelX == 0 && remaining.wheelY == 0)
                        return consumed || reply.handled;
                }
            }
            if (reply.handled || reply.requestFocus || reply.capturePointer)
                return true;
            if (!modals.empty() && target == modals.back().root)
                break;
            target = parent;
        }
        return consumed;
    }

    std::vector<WidgetId> layoutChildren(WidgetId parent) const
    {
        std::vector<WidgetId> result;
        for (auto id : order)
            if (auto* item = get(id); item && !item->pending && item->widget.parent == parent && item->widget.visible)
                result.push_back(id);
        return result;
    }
    std::vector<LayoutItem> layoutItems(const std::vector<WidgetId>& children)
    {
        std::vector<LayoutItem> result;
        for (auto id : children)
            result.push_back({get(id)->widget.layoutHints, [this, id](Constraints c) { return measureNode(id, c); }});
        return result;
    }
    Size measureNode(WidgetId id, Constraints constraints)
    {
        auto* item = get(id);
        if (!item || !item->widget.visible)
            return {};
        for (const auto& cached : item->measures)
            if (cached.revision == layoutRevision && cached.constraints == constraints)
            {
                ++layoutStats.cacheHits;
                return cached.size;
            }
        ++layoutStats.measurements;
        Size desired = item->naturalSize;
        if (item->widget.layout)
            desired = item->widget.layout->measure(layoutItems(layoutChildren(id)), constraints);
        if (item->widget.onMeasure)
        {
            const auto intrinsic = constraints.constrain(item->widget.onMeasure({constraints, theme}));
            desired = item->widget.layout
                          ? Size{std::max(desired.width, intrinsic.width), std::max(desired.height, intrinsic.height)}
                          : intrinsic;
        }
        desired = constraints.constrain(desired);
        if (item->measures.size() == 4)
            item->measures.erase(item->measures.begin());
        item->measures.push_back({constraints, desired, layoutRevision});
        return desired;
    }
    void arrangeChildren(WidgetId parent, Size area, std::vector<std::pair<WidgetId, Bounds>>& staged,
                         std::vector<WidgetId>& arranged)
    {
        arranged.push_back(parent);
        auto* item = get(parent);
        const auto children = layoutChildren(parent);
        if (item->widget.layout)
        {
            const auto boxes = item->widget.layout->arrange(layoutItems(children), area);
            if (boxes.size() != children.size())
                throw std::invalid_argument("UI layout returned an incorrect box count");
            for (std::size_t i = 0; i < children.size(); ++i)
            {
                validateBounds(boxes[i]);
                staged.emplace_back(children[i], boxes[i]);
                arrangeChildren(children[i], {boxes[i].width, boxes[i].height}, staged, arranged);
            }
        }
        else
            for (auto child : children)
            {
                const auto bounds = get(child)->widget.bounds;
                arrangeChildren(child, {bounds.width, bounds.height}, staged, arranged);
            }
    }

    void applyChildClip(Canvas& canvas, Bounds bounds, const Mat3x3& transform)
    {
        canvas.setTransform(transform);
        if (transform.m[1] == 0 && transform.m[3] == 0)
            canvas.clipRect(bounds.x, bounds.y, bounds.width, bounds.height);
        else
        {
            GLint stencilBits = 0;
            GLint framebuffer = 0;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
            const GLenum attachment = framebuffer ? GL_STENCIL_ATTACHMENT : GL_STENCIL;
            GLint type = GL_NONE;
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment,
                                                  GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
            if (type != GL_NONE)
                glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment,
                                                      GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilBits);
            if (stencilBits < 8)
                throw std::runtime_error("Transformed UI clips require at least 8 framebuffer stencil bits");
            canvas.clipRoundedRect(bounds.x, bounds.y, bounds.width, bounds.height, 0);
        }
    }

    void paintChildren(WidgetId parent, const Mat3x3& inherited, const Mat3x3& pixels,
                       const std::vector<WidgetId>& snapshot, Canvas& canvas, const Theme& frameTheme,
                       const std::vector<WidgetId>& modalRoots, WidgetId only = {})
    {
        for (auto id : snapshot)
        {
            if (only && id != only)
                continue;
            if (id != only && std::find(modalRoots.begin(), modalRoots.end(), id) != modalRoots.end())
                continue;
            auto* item = get(id);
            if (!item || item->pending || item->widget.parent != parent || !item->widget.visible)
                continue;
            const auto worldTransform = inherited * localTransform(*item);
            if (!detail::unmap(worldTransform, {}))
                continue;
            const auto transform = pixels * worldTransform;
            if (!detail::unmap(transform, {}))
                continue;
            const auto bounds = item->widget.bounds;
            const bool clip = item->widget.clipChildren;
            auto painter = item->widget.painter;
            CanvasScope branch(canvas);
            canvas.setTransform(transform);
            if (painter)
            {
                CanvasScope local(canvas);
                PaintContext data(canvas, frameTheme, bounds, eligible(id), focus == id, hover == id);
                painter->paint(data);
                local.restore();
            }
            if (!get(id))
            {
                branch.restore();
                continue;
            }
            if (clip)
                applyChildClip(canvas, bounds, transform);
            paintChildren(id, worldTransform, pixels, snapshot, canvas, frameTheme, modalRoots);
            branch.restore();
        }
    }
};

Context::Context(Services services) : impl_(std::make_unique<Impl>(*this, std::move(services))) {}
Context::~Context() = default;

WidgetId Context::add(Widget widget)
{
    impl_->requireMutable();
    validateBounds(widget.bounds);
    widget.layoutHints.validate();
    if (!detail::finiteAffine(widget.transform))
        throw std::invalid_argument("UI transform must be finite and affine");
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
            registration->naturalSize = {widget.bounds.width, widget.bounds.height};
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
bool Context::isAvailable(WidgetId id) const
{
    return impl_->available(id);
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
                item->naturalSize = {bounds.width, bounds.height};
                item->layoutOffset = {};
                invalidate(Invalidation::Measure | Invalidation::Semantics);
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

void Context::setTabStop(WidgetId id, bool tabStop)
{
    impl_->transaction(
        [&]
        {
            if (auto* item = impl_->get(id); item && item->widget.tabStop != tabStop)
            {
                item->widget.tabStop = tabStop;
                invalidate(Invalidation::Semantics);
            }
        });
}
std::optional<Bounds> Context::bounds(WidgetId id) const
{
    if (auto* item = impl_->get(id))
        return item->widget.bounds;
    return {};
}
std::optional<Bounds> Context::layoutBounds(WidgetId id) const
{
    if (auto* item = impl_->get(id))
    {
        auto bounds = item->widget.bounds;
        bounds.x += item->layoutOffset.x;
        bounds.y += item->layoutOffset.y;
        return bounds;
    }
    return {};
}
void Context::setLayout(WidgetId id, std::shared_ptr<const Layout> layout)
{
    impl_->transaction(
        [&]
        {
            if (auto* item = impl_->get(id))
            {
                item->widget.layout = std::move(layout);
                invalidate(Invalidation::Measure);
            }
        });
}
void Context::setLayoutHints(WidgetId id, LayoutHints hints)
{
    hints.validate();
    impl_->transaction(
        [&]
        {
            if (auto* item = impl_->get(id))
            {
                item->widget.layoutHints = std::move(hints);
                invalidate(Invalidation::Measure);
            }
        });
}
void Context::setMeasureCallback(WidgetId id, std::function<Size(const MeasureContext&)> callback)
{
    impl_->transaction(
        [&]
        {
            if (auto* item = impl_->get(id))
            {
                item->widget.onMeasure = std::move(callback);
                invalidate(Invalidation::Measure);
            }
        });
}
Size Context::measure(WidgetId id, Constraints constraints)
{
    if (impl_->processing)
        throw std::logic_error("UI measurement cannot be reentered from a callback");
    if (!contains(id))
        throw std::invalid_argument("UI measurement requires a live widget ID");
    constraints.validate();
    Size result;
    impl_->transaction(
        [&]
        {
            impl_->layoutStats = {};
            MeasurementScope scope(impl_->measuring);
            result = LayoutItem{
                impl_->get(id)->widget.layoutHints, [this, id](Constraints c) {
                    return impl_->measureNode(id, c);
                }}.measure(constraints);
        });
    return result;
}
void Context::layout(WidgetId root, Bounds area)
{
    if (impl_->processing)
        throw std::logic_error("UI layout cannot be reentered from a callback");
    validateBounds(area);
    if (!contains(root))
        throw std::invalid_argument("UI layout requires a live root ID");
    impl_->layoutStats = {};
    auto* item = impl_->get(root);
    if (item->lastArea == area && item->lastRevision == impl_->layoutRevision &&
        item->lastGeometry == impl_->geometryRevision)
    {
        ++impl_->layoutStats.cacheHits;
        return;
    }
    impl_->transaction(
        [&]
        {
            std::vector<std::pair<WidgetId, Bounds>> staged;
            std::vector<WidgetId> arranged;
            {
                MeasurementScope scope(impl_->measuring);
                const Size offered{area.width, area.height};
                const auto size = LayoutItem{
                    item->widget.layoutHints, [this, root](Constraints c) {
                        return impl_->measureNode(root, c);
                    }}.measure({offered, offered});
                staged.emplace_back(root, Bounds{area.x, area.y, size.width, size.height});
                impl_->arrangeChildren(root, size, staged, arranged);
            }
            for (auto [id, box] : staged)
            {
                auto* target = impl_->get(id);
                target->layoutOffset = {box.x, box.y};
                target->widget.bounds = {0, 0, box.width, box.height};
            }
            impl_->layoutStats.arrangedWidgets = staged.size();
            ++impl_->geometryRevision;
            item->lastArea = area;
            item->lastRevision = impl_->layoutRevision;
            item->lastGeometry = impl_->geometryRevision;
            invalidate(Invalidation::Paint | Invalidation::Semantics);
            Event event;
            event.type = EventType::LayoutChanged;
            for (auto id : arranged)
                if (contains(id))
                    impl_->invoke(id, event, true);
        });
}
LayoutStats Context::layoutStats() const
{
    return impl_->layoutStats;
}

void Context::setTransform(WidgetId id, const Mat3x3& transform)
{
    if (!detail::finiteAffine(transform))
        throw std::invalid_argument("UI transform must be finite and affine");
    impl_->transaction(
        [&]
        {
            if (auto* item = impl_->get(id))
            {
                item->widget.transform = transform;
                invalidate(Invalidation::Paint | Invalidation::Semantics);
            }
        });
}
std::optional<Vec2> Context::mapFromRoot(WidgetId id, Vec2 point) const
{
    return contains(id) ? detail::unmap(impl_->world(id), point) : std::nullopt;
}
std::optional<Vec2> Context::mapToRoot(WidgetId id, Vec2 point) const
{
    if (!contains(id))
        return {};
    const auto mapped = impl_->world(id).transformPoint(point);
    if (!std::isfinite(mapped.x) || !std::isfinite(mapped.y))
        return {};
    return mapped;
}
bool Context::isDescendant(WidgetId id, WidgetId ancestor) const
{
    return contains(id) && contains(ancestor) && impl_->descendant(id, ancestor);
}
WidgetId Context::hitTest(Vec2 point) const
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y))
        return {};
    Event event;
    event.x = point.x;
    event.y = point.y;
    return impl_->hit({}, event);
}
WidgetId Context::hovered() const
{
    return impl_->hover;
}
void Context::setTheme(Theme theme)
{
    impl_->requireMutable();
    for (auto color : {theme.background, theme.surface, theme.hover, theme.text, theme.muted, theme.accent,
                       theme.border, theme.disabled, theme.pressed, theme.error})
        if (!std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b) || !std::isfinite(color.a))
            throw std::invalid_argument("UI theme colors must be finite");
    if (!std::isfinite(theme.fontSize) || theme.fontSize <= 0 || !std::isfinite(theme.borderWidth) ||
        theme.borderWidth < 0)
        throw std::invalid_argument(
            "UI theme metrics must be finite with positive font size and nonnegative border width");
    impl_->theme = std::move(theme);
    invalidate(Invalidation::All);
}
const Theme& Context::theme() const
{
    return impl_->theme;
}
void Context::paint(Canvas& canvas, const Viewport& viewport)
{
    if (impl_->processing)
        throw std::logic_error("UI painting cannot be reentered from a callback");
    if (!canvas.renderer().isFrameActive())
        throw std::logic_error("UI painting requires an active Canvas frame");
    impl_->transaction(
        [&]
        {
            CanvasScope scope(canvas);
            const auto snapshot = impl_->order;
            const auto theme = impl_->theme;
            const auto pixels = Mat3x3::scaling(viewport.scaleX(), viewport.scaleY());
            try
            {
                std::vector<WidgetId> modalRoots;
                for (auto id : snapshot)
                    if (auto* item = impl_->get(id); item && item->widget.overlay && !item->widget.parent)
                        modalRoots.push_back(id);
                for (const auto& modal : impl_->modals)
                    if (std::find(modalRoots.begin(), modalRoots.end(), modal.root) == modalRoots.end())
                        modalRoots.push_back(modal.root);
                impl_->paintChildren({}, {}, pixels, snapshot, canvas, theme, modalRoots);
                for (auto root : modalRoots)
                {
                    auto* item = impl_->get(root);
                    if (!item || !impl_->available(root) || !detail::unmap(impl_->world(root), {}))
                        continue;
                    const auto parent = item->widget.parent;
                    CanvasScope layer(canvas);
                    std::vector<WidgetId> ancestors;
                    for (auto id = parent; auto* ancestor = impl_->get(id); id = ancestor->widget.parent)
                        ancestors.push_back(id);
                    for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it)
                        if (auto* ancestor = impl_->get(*it); ancestor && ancestor->widget.clipChildren)
                            impl_->applyChildClip(canvas, ancestor->widget.bounds, pixels * impl_->world(*it));
                    impl_->paintChildren(parent, parent ? impl_->world(parent) : Mat3x3{}, pixels, snapshot, canvas,
                                         theme, modalRoots, root);
                    layer.restore();
                }
                scope.restore();
            }
            catch (...)
            {
                impl_->callbackError = std::current_exception();
                impl_->cancellation = CancelReason::HandlerFailed;
            }
        });
}

bool Context::dispatch(const Event& event)
{
    if (impl_->processing)
        throw std::logic_error("UI dispatch cannot be reentered from a callback");
    if (event.type == EventType::FocusGained || event.type == EventType::FocusLost || event.type == EventType::Timer ||
        event.type == EventType::LayoutChanged || event.type == EventType::FocusWithin)
        throw std::invalid_argument("UI notification events are generated by Context");
    if (pointerEvent(event.type) && (!std::isfinite(event.x) || !std::isfinite(event.y) ||
                                     !std::isfinite(event.wheelX) || !std::isfinite(event.wheelY)))
        throw std::invalid_argument("UI pointer coordinates/deltas must be finite");
    bool handled = false;
    impl_->transaction(
        [&]
        {
            if (event.type == EventType::KeyDown && event.key == Key::Escape && !impl_->capture &&
                impl_->modals.empty())
            {
                const auto snapshot = impl_->order;
                for (auto it = snapshot.rbegin(); it != snapshot.rend(); ++it)
                    if (auto* item = impl_->get(*it); item && item->widget.overlay && !item->widget.hitTestVisible)
                        if (impl_->invoke(*it, event).handled)
                        {
                            handled = true;
                            return;
                        }
            }
            if (event.type == EventType::WindowFocusLost || event.type == EventType::PointerLeave ||
                event.type == EventType::Cancel ||
                ((event.type == EventType::KeyDown || event.type == EventType::KeyRepeat) && event.key == Key::Escape &&
                 impl_->capture))
            {
                impl_->hover = {};
                impl_->pointerPosition.reset();
                invalidate();
                handled = impl_->capture.has_value() || !impl_->modals.empty();
                impl_->cancellation = event.type == EventType::WindowFocusLost ? CancelReason::WindowFocusLost
                                      : event.type == EventType::PointerLeave  ? CancelReason::PointerLeftWindow
                                                                               : event.reason;
                if (event.type == EventType::WindowFocusLost)
                {
                    const auto snapshot = impl_->order;
                    for (auto id : snapshot)
                        if (auto* item = impl_->get(id); item && item->widget.overlay)
                            impl_->invoke(id, event, true);
                    impl_->requestFocus({});
                }
                return;
            }
            if ((event.type == EventType::KeyDown || event.type == EventType::KeyRepeat ||
                 event.type == EventType::KeyUp) &&
                event.key == Key::Tab && !impl_->drag && !event.modifiers.control && !event.modifiers.alt &&
                !event.modifiers.meta)
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
            if (impl_->drag && (event.type == EventType::KeyDown || event.type == EventType::KeyRepeat ||
                                event.type == EventType::KeyUp || event.type == EventType::TextInput))
            {
                handled = true;
                return;
            }
            if (impl_->drag && (event.type == EventType::PointerMove || event.type == EventType::PointerUp ||
                                event.type == EventType::PointerDown))
            {
                const auto session = impl_->drag;
                handled = true;
                if (event.pointer != session->pointer)
                    return;
                session->state.x = event.x;
                session->state.y = event.y;
                session->modifiers = event.modifiers;
                session->update = true;
                impl_->pointerPosition = Vec2{event.x, event.y};
                impl_->updateDrag();
                if (impl_->drag != session)
                    return;
                if (event.type == EventType::PointerUp && event.button == PointerButton::Primary)
                {
                    if (impl_->cancellation || impl_->callbackError || !impl_->eligible(session->state.source))
                        impl_->finishDrag(DragOutcome::Cancelled,
                                          impl_->cancellation.value_or(CancelReason::Unavailable));
                    else
                    {
                        bool accepted = false;
                        if (session->state.accepted && impl_->eligible(session->state.target))
                            accepted = impl_->dragNotify(session->state.target, DragPhase::Drop, session);
                        if (impl_->drag == session)
                            impl_->finishDrag(impl_->callbackError ? DragOutcome::Cancelled
                                              : accepted           ? DragOutcome::Dropped
                                                                   : DragOutcome::Rejected,
                                              impl_->callbackError ? CancelReason::HandlerFailed
                                                                   : CancelReason::Explicit);
                    }
                }
                return;
            }
            WidgetId target = impl_->focus;
            if (pointerEvent(event.type))
            {
                impl_->pointerPosition = Vec2{event.x, event.y};
                const auto underPointer = impl_->hit({}, event);
                if (underPointer != impl_->hover)
                {
                    impl_->hover = underPointer;
                    invalidate();
                }
                target = impl_->capture && impl_->capture->pointer == event.pointer && event.type != EventType::Wheel
                             ? impl_->capture->id
                             : underPointer;
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
        if (!item || !item->widget.focusable || !impl_->available(id, true) || !detail::unmap(impl_->world(id), {}) ||
            (!impl_->modals.empty() && !impl_->descendant(id, impl_->modals.back().root)))
            return false;
    }
    impl_->transaction([&] { impl_->requestFocus(id); });
    return true;
}
bool Context::isProcessing() const
{
    return impl_->processing;
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
bool Context::beginDrag(WidgetId source, DragPayload payload, std::function<void(const DragResult&)> finished)
{
    impl_->requireMutable();
    if (!payload)
        throw std::invalid_argument("Drag payload must own a value");
    const auto* event = impl_->invocation;
    if (impl_->drag || impl_->invoking != source || !event || !impl_->eligible(source))
        return false;
    const bool down = event->type == EventType::PointerDown && event->button == PointerButton::Primary;
    const bool moving = event->type == EventType::PointerMove && impl_->capture && impl_->capture->id == source &&
                        impl_->capture->pointer == event->pointer && impl_->capture->button == PointerButton::Primary;
    if ((!down && !moving) || (impl_->capture && impl_->capture->id != source))
        return false;
    auto session = std::make_shared<Impl::DragSession>();
    session->state = {source, {}, std::move(payload), event->x, event->y, false};
    session->pointer = event->pointer;
    session->modifiers = event->modifiers;
    session->finished = std::move(finished);
    impl_->drag = std::move(session);
    impl_->capture = Impl::Capture{source, event->pointer, PointerButton::Primary};
    impl_->pointerPosition = Vec2{event->x, event->y};
    invalidate(Invalidation::Paint | Invalidation::Semantics);
    return true;
}
void Context::cancelDrag(CancelReason reason)
{
    impl_->transaction([&] { impl_->finishDrag(DragOutcome::Cancelled, reason); });
}
std::optional<DragState> Context::dragState() const
{
    return impl_->drag ? std::optional<DragState>(impl_->drag->state) : std::nullopt;
}
void Context::pushModal(WidgetId root)
{
    if (!impl_->available(root, true) || !detail::unmap(impl_->world(root), {}) ||
        (!impl_->modals.empty() && !impl_->descendant(root, impl_->modals.back().root)))
        throw std::invalid_argument("UI modal root must be available inside the current scope");
    impl_->transaction(
        [&]
        {
            impl_->modals.push_back(
                {root, impl_->focusPending && !impl_->focusFirst ? impl_->desiredFocus : impl_->focus});
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
    impl_->requireMutable();
    if (static_cast<unsigned>(flags) &
        (static_cast<unsigned>(Invalidation::Measure) | static_cast<unsigned>(Invalidation::Arrange)))
        ++impl_->layoutRevision;
    const auto bits = static_cast<unsigned>(flags);
    if (bits & static_cast<unsigned>(Invalidation::Measure))
        flags = flags | Invalidation::Arrange;
    if (static_cast<unsigned>(flags) & static_cast<unsigned>(Invalidation::Arrange))
        flags = flags | Invalidation::Paint;
    impl_->dirty = impl_->dirty | flags;
}
Invalidation Context::takeInvalidation()
{
    impl_->requireMutable();
    return std::exchange(impl_->dirty, Invalidation::None);
}
Invalidation Context::pendingInvalidation() const
{
    return impl_->dirty;
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
    impl_->requireMutable();
    if (!std::isfinite(deadline))
        throw std::invalid_argument("UI deadline must be finite");
    if (auto* item = impl_->get(id))
        item->wakeup = deadline;
}
void Context::cancelWakeup(WidgetId id)
{
    impl_->requireMutable();
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
