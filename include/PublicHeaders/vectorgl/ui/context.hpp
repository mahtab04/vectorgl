#pragma once

#include <memory>
#include <optional>

#include "vectorgl/ui/painting.hpp"
#include "vectorgl/ui/services.hpp"
#include "vectorgl/ui/widget.hpp"

namespace vectorgl::ui
{
/*! Optional, single-UI-thread input foundation; does not initialize OpenGL.
 * IDs become invalid immediately on removal; storage/callback destruction is
 * deferred until dispatch unwinds. Adds made by callbacks become routable after
 * that dispatch. Reentrant dispatch is rejected. Callbacks may mutate the tree.
 * Callback exceptions propagate after cleanup; hosts must catch them at native
 * callback boundaries. A failed callback consumes its event and cancels capture.
 */
class Context
{
public:
    explicit Context(Services services = {});
    ~Context();
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    WidgetId add(Widget widget);
    void remove(WidgetId id); // Removes the subtree. Stale IDs are a no-op.
    bool contains(WidgetId id) const;
    void setBounds(WidgetId id, Bounds bounds);
    void setVisible(WidgetId id, bool visible);
    void setEnabled(WidgetId id, bool enabled);
    std::optional<Bounds> bounds(WidgetId id) const;
    std::optional<Bounds> layoutBounds(WidgetId id) const; // Placement before the application transform.
    void setLayout(WidgetId id, std::shared_ptr<const Layout> layout);
    void setLayoutHints(WidgetId id, LayoutHints hints);
    void setMeasureCallback(WidgetId id, std::function<Size(const MeasureContext&)> callback);
    Size measure(WidgetId id, Constraints constraints = {});
    /*! Explicit measure/arrange boundary; call before hit tests/painting after
     * layout-affecting input. Publishes geometry atomically on success. Area is in
     * the root's parent coordinates; its application transform remains intact.
     * Measurement callbacks are pure: Context mutation/reentry is rejected.
     */
    void layout(WidgetId root, Bounds area);
    LayoutStats layoutStats() const;
    void setTransform(WidgetId id, const Mat3x3& transform);
    std::optional<Vec2> mapFromRoot(WidgetId id, Vec2 point) const;
    std::optional<Vec2> mapToRoot(WidgetId id, Vec2 point) const;
    bool isDescendant(WidgetId id, WidgetId ancestor) const;
    WidgetId hitTest(Vec2 point) const;
    WidgetId hovered() const;
    void setTheme(Theme theme);
    const Theme& theme() const;
    /*! Paint into an active Canvas frame with identity root UI coordinates.
     * Preserves Canvas style/transform/clip state. Does not clear or bind FBOs.
     * Non-axis-aligned ancestor clips require at least 8 framebuffer stencil bits.
     */
    void paint(Canvas& canvas, const Viewport& viewport);

    /*! Returns true if consumed, including a modal barrier or focus traversal.
     * Pointer input bubbles through ancestors; keys/text begin at focus owner.
     * Unconsumed input can be offered to application shortcuts/artboard actions.
     */
    bool dispatch(const Event& event);
    bool focus(WidgetId id); // Empty ID clears focus; returns false if ineligible.
    WidgetId focused() const;
    WidgetId captured() const;
    void cancelCapture(CancelReason reason = CancelReason::Explicit);

    /*! Modal scope is a registered subtree. Push cancels capture and focuses its
     * first eligible widget. Pop restores the previous eligible focus owner.
     * Invalid/hidden/disabled scope roots are automatically popped with nested scopes.
     */
    void pushModal(WidgetId root);
    void popModal();
    WidgetId modalRoot() const;

    void invalidate(Invalidation flags = Invalidation::Paint);
    Invalidation takeInvalidation();
    Invalidation pendingInvalidation() const;
    double now() const;
    void requestWakeup(WidgetId id, double deadline); // Replaces this widget's deadline.
    void cancelWakeup(WidgetId id);
    std::optional<double> nextWakeup() const;
    void pumpTimers(); // Delivers each currently due Timer once; callbacks may reschedule.
    const Services& services() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace vectorgl::ui
