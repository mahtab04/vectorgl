#include <vectorgl/ui/context.hpp>

int main()
{
    vectorgl::ui::Context ui;
    vectorgl::ui::Widget widget;
    widget.focusable = true;
    auto id = ui.add(widget);
    if (!ui.focus(id) || ui.focused() != id)
        return 1;
    ui.remove(id);
    return ui.contains(id) || static_cast<bool>(ui.focused()) ? 1 : 0;
}
