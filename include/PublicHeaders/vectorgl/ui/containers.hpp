#pragma once

#include "vectorgl/ui/context.hpp"

namespace vectorgl::ui
{
enum class SplitAxis
{
    Horizontal,
    Vertical
};
struct SplitOptions
{
    SplitAxis axis = SplitAxis::Horizontal;
    float fraction = 0.35f, divider = 8, keyboardStep = 16;
    float firstMinimum = 120, secondMinimum = 120;
    float firstMaximum = std::numeric_limits<float>::infinity();
    float secondMaximum = std::numeric_limits<float>::infinity();
};
/*! Owns a registered split subtree. Context must outlive the controller.
 * first()/second() are pane containers for app layouts and children. Keep internal
 * panes/divider registered; remove/hide application content beneath them instead.
 * Destruction removes the subtree and suppresses client callback exceptions;
 * explicit Context::remove(id()) permits handling such exceptions.
 */
class SplitPanel
{
public:
    SplitPanel(Context& context, WidgetId parent = {}, SplitOptions options = {});
    ~SplitPanel();
    SplitPanel(const SplitPanel&) = delete;
    SplitPanel& operator=(const SplitPanel&) = delete;
    WidgetId id() const;
    WidgetId first() const;
    WidgetId second() const;
    WidgetId divider() const;
    float fraction() const;           // Requested normalized position; layout applies pane limits.
    float firstExtent() const;        // Actual logical size after layout.
    void setFraction(float fraction); // Cancels an active drag before changing the request.
    void setOnDragFinished(std::function<void(bool cancelled)> callback);

private:
    struct State;
    std::shared_ptr<State> state_;
};

/*! Pure bounded scroll geometry, independent of painting and input hosts. */
class ScrollModel
{
public:
    ScrollModel(bool horizontal = false, bool vertical = true);
    void setGeometry(Size viewport, Size content);
    Vec2 setOffset(Vec2 offset); // Returns actual offset change after clamping.
    Vec2 scrollBy(Vec2 delta);
    bool reveal(Bounds target, float margin = 4);
    Size viewport() const
    {
        return viewport_;
    }
    Size content() const
    {
        return content_;
    }
    Vec2 offset() const
    {
        return offset_;
    }
    Vec2 maximumOffset() const;
    Bounds thumb(bool horizontal, float trackLength, float thickness, float minimumLength = 24) const;

private:
    bool horizontal_, vertical_;
    Size viewport_, content_;
    Vec2 offset_;
};
enum class ScrollChaining
{
    Ancestors,
    Contain
};
struct ScrollOptions
{
    bool horizontal = false, vertical = true;
    float lineStep = 40, scrollbar = 12, minimumThumb = 24;
    Size preferredSize{240, 180};
    ScrollChaining chaining = ScrollChaining::Ancestors;
    bool focusable = true; // Composite controls may route keyboard input through their enclosing scope.
};
/*! Owns a clipped viewport, content container, and reserved scrollbar lanes.
 * Add application children/layouts to content(). Context must outlive this object.
 * LayoutChanged updates geometry automatically; FocusWithin reveals focused
 * descendants through nested views. Content transform/internal child layouts are
 * owned by this controller. Wheel residuals bubble to UI ancestors by default;
 * unconsumed overscroll never escapes into application commands.
 */
class ScrollView
{
public:
    ScrollView(Context& context, WidgetId parent = {}, ScrollOptions options = {});
    ~ScrollView();
    ScrollView(const ScrollView&) = delete;
    ScrollView& operator=(const ScrollView&) = delete;
    WidgetId id() const;
    WidgetId content() const;
    WidgetId viewportId() const;
    WidgetId horizontalBar() const;
    WidgetId verticalBar() const;
    void setContentLayout(std::shared_ptr<const Layout> layout);
    void setContentSize(Size size); // Manual content extent; clears the content strategy.
    Vec2 offset() const;
    Vec2 maximumOffset() const;
    bool setOffset(Vec2 offset);
    bool scrollBy(Vec2 delta);
    bool scrollToVisible(WidgetId descendant, float margin = 4);

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace vectorgl::ui
