#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>

#include "vectorgl/node.hpp"
#include "vectorgl/ui/events.hpp"

namespace vectorgl::ui
{
class Context;
class WidgetPainter;
/*! Generation-checked identity scoped to its originating Context. */
struct WidgetId
{
    std::uint64_t context = 0, generation = 0;
    std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
    explicit operator bool() const
    {
        return context != 0 && generation != 0;
    }
    bool operator==(const WidgetId&) const = default;
};
struct Bounds
{
    float x = 0, y = 0, width = 0, height = 0;
    bool contains(float px, float py) const
    {
        return px >= x && py >= y && px < x + width && py < y + height;
    }
};
/*! Registration descriptor with optional polymorphic painting. Bounds are local
 * geometry, mapped through the composed ancestor transforms. Later siblings paint/hit above earlier
 * siblings; parents receive unhandled events after children. Ancestor visibility,
 * enabled state and optional rectangular child clipping apply to the subtree.
 * Context owns a copy of the callback; user captures must outlive registration.
 */
struct Widget
{
    WidgetId parent;
    Bounds bounds;
    bool visible = true, enabled = true, focusable = false, clipChildren = false;
    std::function<EventReply(Context&, WidgetId, const Event&)> onEvent;
    // Transforms compose through ancestors. Bounds are geometry, not a child-origin
    // translation: identity transforms preserve PR 1's absolute rectangle behavior.
    Mat3x3 transform = Mat3x3::identity();
    std::shared_ptr<WidgetPainter> painter;
};
enum class Invalidation : unsigned
{
    None = 0,
    Paint = 1,
    Arrange = 2,
    Measure = 4,
    Semantics = 8,
    All = 15
};
constexpr Invalidation operator|(Invalidation a, Invalidation b)
{
    return static_cast<Invalidation>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
} // namespace vectorgl::ui
