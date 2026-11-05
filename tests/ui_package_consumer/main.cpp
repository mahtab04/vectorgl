#include <vectorgl/ui/context.hpp>
#ifdef VECTORGL_TEST_GLFW
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <vectorgl/ui/glfw_adapter.hpp>
#endif

int main()
{
#ifdef VECTORGL_TEST_GLFW
    vectorgl::ui::WindowMetrics metrics(800, 600, 1600, 1200, 2, 2);
    if (!metrics.viewport() || metrics.viewport()->width() != 800 || !glfwGetVersionString())
        return 1;
#endif
    vectorgl::ui::Context ui;
    vectorgl::ui::Widget widget;
    widget.painter = std::make_shared<vectorgl::ui::SurfacePainter>();
    widget.focusable = true;
    auto id = ui.add(widget);
    if (!ui.focus(id) || ui.focused() != id)
        return 1;
    ui.remove(id);
    return ui.contains(id) || static_cast<bool>(ui.focused()) ? 1 : 0;
}
