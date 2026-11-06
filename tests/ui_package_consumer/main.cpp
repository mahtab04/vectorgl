#include <vectorgl/canvas.hpp>
#include <vectorgl/ui/containers.hpp>
#include <vectorgl/ui/context.hpp>
#include <vectorgl/ui/controls.hpp>
#include <vectorgl/ui/file_dialogs.hpp>
#ifdef VECTORGL_HAS_NATIVE_DIALOGS
#include <vectorgl/ui/native_dialogs.hpp>
#endif
#include <vectorgl/ui/list_view.hpp>
#include <vectorgl/ui/overlays.hpp>
#include <vectorgl/ui/table_view.hpp>
#include <vectorgl/ui/tree_view.hpp>
#include <vectorgl/ui/workspace_controls.hpp>
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
        vectorgl::ui::FileDialogService service(ui);
        bool completed = false;
        service.request({}, [&](const auto& result)
                        { completed = result.result.status == vectorgl::ui::ServiceStatus::Unavailable; });
        service.pump();
        if (!completed || service.pending())
            return 1;
#ifdef VECTORGL_HAS_NATIVE_DIALOGS
        auto native = vectorgl::ui::createNativeFileDialogs();
        if (!native)
            return 1; // Do not enter OS dialogs in an installed-consumer test.
#endif
    }
    {
        auto payload = vectorgl::ui::DragPayload::make(std::string("owned"));
        if (!payload.get<std::string>() || payload.get<int>())
            return 1;
        vectorgl::ui::ListView list(ui, {},
                                    std::make_shared<vectorgl::ui::VectorListModel>(
                                        std::vector<vectorgl::ui::ListItem>{{1, "One"}, {2, "Two"}}));
        list.setSelection({2});
        list.setProjection(
            std::make_shared<vectorgl::ui::VectorListModel>(std::vector<vectorgl::ui::ListItem>{{1, "One"}}), {1, 2});
        if (list.selection() != std::vector<vectorgl::ui::ItemId>{2} || ui.dragState())
            return 1;
    }
    {
        auto model = std::make_shared<vectorgl::ui::VectorTableModel>(
            std::vector<vectorgl::ui::TableItem>{{100, {{1, "Name"}, {2, "42"}}}});
        vectorgl::ui::TableView table(ui, {}, model, {{1, "Name"}, {2, "Value"}});
        ui.layout(table.id(), {0, 0, 320, 240});
        table.setSelection({100});
        table.setSort(vectorgl::ui::TableSort{2, vectorgl::ui::SortDirection::Descending});
        table.setColumnWidth(1, 200);
        if (table.currentItem() != 100 || table.columns()[0].width != 200 || table.sort()->column != 2)
            return 1;
    }
    {
        auto model = std::make_shared<vectorgl::ui::VectorTreeModel>(
            std::vector<vectorgl::ui::TreeItem>{{10, 0, "Root"}, {20, 10, "Child"}});
        vectorgl::ui::TreeView tree(ui, {}, model);
        tree.setExpanded(10, true);
        tree.setSelection({20});
        ui.layout(tree.id(), {0, 0, 320, 240});
        if (tree.visibleItemCount() != 2 || tree.currentItem() != 20 || !tree.isExpanded(10))
            return 1;
    }
    {
        auto model = std::make_shared<vectorgl::ui::VectorListModel>(
            std::vector<vectorgl::ui::ListItem>{{100, "First"}, {200, "Second"}});
        vectorgl::ui::ListView list(ui, {}, model);
        ui.layout(list.id(), {0, 0, 320, 240});
        list.setSelection({200});
        if (list.currentItem() != 200 || list.selection() != std::vector<vectorgl::ui::ItemId>{200})
            return 1;
    }
    {
        vectorgl::Canvas canvas;
        vectorgl::ui::TabView tabs(ui, {}, {{{"One"}, {"Two"}}, 0});
        vectorgl::ui::CollapsibleSection section(ui, tabs.page(0), {"Section"});
        vectorgl::ui::SearchField search(ui, canvas, section.content());
        vectorgl::ui::Label label(ui, canvas, tabs.page(1), {"Installed label"});
        vectorgl::ui::Separator line(ui, tabs.page(1));
        ui.layout(tabs.id(), {0, 0, 640, 480});
        ui.focus(search.fieldId());
        search.setQuery("query");
        tabs.setSelected(1);
        if (ui.isAvailable(search.fieldId()) || ui.focused() != tabs.tabId(1))
            return 1;
        section.setExpanded(false);
    }
    {
        vectorgl::ui::OverlayManager overlays(ui);
        overlays.update({0, 0, 640, 480});
        vectorgl::ui::ComboBox combo(ui, overlays, {}, {{{"One"}, {"Two"}}, 0});
        ui.layout(combo.id(), {20, 20, 180, 44});
        combo.open();
        overlays.update({0, 0, 640, 480});
        if (!ui.modalRoot())
            return 1;
        overlays.closeAll();
        if (ui.modalRoot())
            return 1;
    }
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
