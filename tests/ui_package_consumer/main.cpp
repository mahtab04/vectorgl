#include <vectorgl/canvas.hpp>
#include <vectorgl/ui/containers.hpp>
#include <vectorgl/ui/context.hpp>
#include <vectorgl/ui/controls.hpp>
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
    {
        vectorgl::Canvas canvas;
        vectorgl::ui::TextField field(ui, canvas);
        ui.focus(field.id());
        vectorgl::ui::Event input;
        input.type = vectorgl::ui::EventType::TextInput;
        input.text = "Installed control";
        if (!ui.dispatch(input) || !field.commit() || field.committedText() != input.text)
            return 1;
        ui.focus({});
    }
    {
        vectorgl::ui::SplitPanel split(ui);
        ui.setLayout(split.first(), std::make_shared<vectorgl::ui::ColumnLayout>());
        vectorgl::ui::ScrollView view(ui, split.first());
        vectorgl::ui::LayoutHints hints;
        hints.flex = 1;
        ui.setLayoutHints(view.id(), hints);
        view.setContentSize({100, 900});
        ui.layout(split.id(), {0, 0, 640, 480});
        if (!view.scrollBy({0, 60}) || view.offset().y != 60)
            return 1;
    }
    vectorgl::ui::Widget widget;
    widget.painter = std::make_shared<vectorgl::ui::SurfacePainter>();
    widget.focusable = true;
    auto id = ui.add(widget);
    ui.setLayout(id, std::make_shared<vectorgl::ui::RowLayout>());
    vectorgl::ui::Widget child;
    child.parent = id;
    child.layoutHints.flex = 1;
    const auto childId = ui.add(child);
    ui.layout(id, {0, 0, 120, 40});
    if (!ui.bounds(childId) || ui.bounds(childId)->width != 120)
        return 1;
    if (!ui.focus(id) || ui.focused() != id)
        return 1;
    ui.remove(id);
    return ui.contains(id) || static_cast<bool>(ui.focused()) ? 1 : 0;
}
