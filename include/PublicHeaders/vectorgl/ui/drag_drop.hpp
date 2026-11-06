#pragma once

#include <any>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "vectorgl/ui/events.hpp"

namespace vectorgl::ui
{
/*! Immutable owned in-process value. Copies share ownership; get<T>() succeeds
 * only for the exact stored type. No borrowed pointers/native OS drag formats. */
class DragPayload
{
public:
    template <class T>
        requires(std::is_copy_constructible_v<T> && !std::is_pointer_v<T>)
    static DragPayload make(T value)
    {
        DragPayload result;
        result.value_ = std::make_shared<const std::any>(std::move(value));
        return result;
    }
    template <class T> const T* get() const
    {
        return value_ ? std::any_cast<T>(value_.get()) : nullptr;
    }
    explicit operator bool() const
    {
        return bool(value_);
    }

private:
    std::shared_ptr<const std::any> value_;
};
enum class DragPhase
{
    Enter,
    Move,
    Leave,
    Drop
};
struct DragEvent
{
    DragPhase phase = DragPhase::Move;
    WidgetId source, target;
    DragPayload payload;
    float x = 0, y = 0, localX = 0, localY = 0;
    Modifiers modifiers;
};
enum class DragOutcome
{
    Dropped,
    Rejected,
    Cancelled
};
struct DragResult
{
    DragOutcome outcome = DragOutcome::Cancelled;
    WidgetId source, target;
    DragPayload payload;
    CancelReason reason = CancelReason::Explicit;
};
struct DragState
{
    WidgetId source, target;
    DragPayload payload;
    float x = 0, y = 0;
    bool accepted = false;
};
} // namespace vectorgl::ui
