#include <vectorgl/canvas.hpp>
#include <vectorgl/ui/context.hpp>
#include <vectorgl/ui/glfw_adapter.hpp>
#include <vectorgl/ui/native_dialogs.hpp>

int main()
{
    vectorgl::Canvas canvas;
    vectorgl::ui::Context ui;
    vectorgl::ui::Widget widget;
    widget.bounds = {0, 0, 100, 40};
    const auto id = ui.add(widget);
    vectorgl::ui::WindowMetrics metrics(800, 600, 1600, 1200, 2, 2);
    const auto dialogs = vectorgl::ui::createNativeFileDialogs();
    return ui.contains(id) && metrics.viewport() && dialogs ? 0 : 1;
}
