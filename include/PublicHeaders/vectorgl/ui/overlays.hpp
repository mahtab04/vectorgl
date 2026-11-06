#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "vectorgl/ui/controls.hpp"

namespace vectorgl::ui
{
struct OverlayId
{
    std::uint64_t value = 0;
    explicit operator bool() const
    {
        return value != 0;
    }
    bool operator==(const OverlayId&) const = default;
};
enum class DismissReason
{
    Selected,
    Escape,
    OutsidePointer,
    OwnerUnavailable,
    WindowFocusLost,
    Programmatic
};
struct PopupItem
{
    std::string text;
    bool enabled = true, separator = false;
};
struct PopupOptions
{
    std::vector<PopupItem> items;
    std::optional<std::size_t> selected;
    float width = 240, rowHeight = 40, maximumHeight = 280;
    std::function<void(std::size_t)> onSelected;
    std::function<void(DismissReason)> onClosed;
};
struct DialogAction
{
    std::string label;
    int value = 0;
    bool enabled = true;
};
struct DialogOptions
{
    std::string title, message;
    std::vector<DialogAction> actions;
    std::optional<std::size_t> defaultAction, cancelAction;
    Size preferredSize{460, 260};
    std::function<void(std::optional<int>)> onFinished;
};
struct TooltipOptions
{
    double delay = 0.6, duration = 5;
    Size preferredSize{280, 80};
};
/*! One manager per Context. Context must outlive it. Call update() before/after
 * input and after timers/layout, outside Context callbacks. Interactive overlays
 * are modal input scopes; outside presses dismiss popups without click-through.
 * Dialogs require an action/Escape. Tooltip bindings are noninteractive.
 */
class OverlayManager
{
public:
    explicit OverlayManager(Context& context);
    ~OverlayManager();
    OverlayManager(const OverlayManager&) = delete;
    OverlayManager& operator=(const OverlayManager&) = delete;
    void update(Bounds viewport);
    OverlayId openPopup(WidgetId owner, PopupOptions options);
    OverlayId openContextMenu(WidgetId owner, Vec2 position, PopupOptions options);
    OverlayId openDialog(DialogOptions options);
    void close(OverlayId id, DismissReason reason = DismissReason::Programmatic);
    void closeAll();
    bool contains(OverlayId id) const;
    WidgetId root(OverlayId id) const;
    std::optional<Bounds> contentBounds(OverlayId id) const;
    void setTooltip(WidgetId owner, std::string text, TooltipOptions options = {});
    void removeTooltip(WidgetId owner);
    bool tooltipVisible() const;

private:
    struct State;
    std::shared_ptr<State> state_;
};
struct ComboBoxOptions
{
    std::vector<PopupItem> items;
    std::optional<std::size_t> selected;
    Size preferredSize{240, 44};
    std::function<void(std::size_t)> onChanged;
};
/*! Owns its trigger and popup. Context and OverlayManager must outlive it. */
class ComboBox
{
public:
    ComboBox(Context& context, OverlayManager& overlays, WidgetId parent = {}, ComboBoxOptions options = {});
    ~ComboBox();
    ComboBox(const ComboBox&) = delete;
    ComboBox& operator=(const ComboBox&) = delete;
    WidgetId id() const;
    std::optional<std::size_t> selected() const;
    void setSelected(std::optional<std::size_t> index); // No user notification.
    void open();

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace vectorgl::ui
