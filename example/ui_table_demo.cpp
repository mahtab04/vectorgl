#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <vectorgl/canvas.hpp>
#include <vectorgl/ui/containers.hpp>
#include <vectorgl/ui/glfw_adapter.hpp>
#include <vectorgl/ui/table_view.hpp>
#include <vectorgl/ui/workspace_controls.hpp>
using namespace vectorgl;
using namespace vectorgl::ui;
namespace
{
void screenshot(const std::string& path, int width, int height)
{
    std::vector<unsigned char> pixels(size_t(width) * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    for (size_t i = 0; i < pixels.size(); i += 4)
        std::swap(pixels[i], pixels[i + 2]);
    unsigned char header[54]{};
    auto u32 = [&](int offset, uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
            header[offset + i] = static_cast<unsigned char>(value >> (i * 8));
    };
    header[0] = 'B';
    header[1] = 'M';
    u32(2, static_cast<uint32_t>(54 + pixels.size()));
    u32(10, 54);
    u32(14, 40);
    u32(18, width);
    u32(22, height);
    header[26] = 1;
    header[28] = 32;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(header), sizeof(header));
    file.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    if (!file)
        throw std::runtime_error("Could not write screenshot");
}

class TagCells final : public TableCellPainter
{
public:
    std::size_t calls = 0;
    void paint(PaintContext& p, const TableModel& model, const TableCell& cell) override
    {
        ++calls;
        auto& canvas = p.canvas();
        if (!canvas.setFont(p.theme().fontPath, p.theme().fontSize))
            throw std::runtime_error("Could not load table demo font");
        const auto text = std::string(model.cellText(cell.row.index, cell.column));
        auto color = cell.row.enabled ? p.theme().text : p.theme().muted;
        if (cell.column == 5 && text == "Alarm")
            color = p.theme().error;
        else if (cell.column == 5 && cell.row.enabled)
            color = p.theme().accent;
        const auto b = cell.bounds;
        if (b.width <= 20)
            return;
        canvas.clipRect(b.x + 10, b.y, b.width - 20, b.height);
        canvas.setFillColor(color);
        TextLayoutOptions layout;
        layout.maxWidth = b.width - 20;
        layout.align = cell.align;
        layout.wrap = TextWrap::None;
        canvas.fillText(text, b.x + 10, b.y + std::max(0.0f, (b.height - canvas.lineHeight()) / 2), layout);
    }
};
class TableDemo
{
public:
    Canvas canvas;
    Context ui;
    GlfwAdapter host;
    std::exception_ptr error;
    TableDemo(GLFWwindow* window, std::string font, bool)
        : ui(Services{{}, std::make_shared<GlfwClipboard>(window)}), host(window, ui)
    {
        canvas.init();
        auto theme = ui.theme();
        theme.fontPath = std::move(font);
        theme.fontSize = 16;
        ui.setTheme(theme);
        Widget root;
        root.layout = std::make_shared<ColumnLayout>(Padding{24, 24, 24, 24}, 12.0f);
        root_ = ui.add(root);
        addLabel(root_, "VectorGL / live tag browser", 1.6f);
        addLabel(root_,
                 "Click headers: sort | Drag dividers: resize | Header Shift+arrows: resize | Ctrl/Shift: select",
                 .85f);
        Widget tools;
        tools.parent = root_;
        tools.layout = std::make_shared<RowLayout>(Padding{}, 12.0f);
        const auto toolbar = ui.add(tools);
        SearchFieldOptions search;
        search.onChanged = [this](std::string_view) { rebuild(); };
        search_ = std::make_unique<SearchField>(ui, canvas, toolbar, search);
        LayoutHints flex;
        flex.flex = 1;
        ui.setLayoutHints(search_->id(), flex);
        reset_ = std::make_unique<Button>(
            ui, toolbar, ButtonOptions{"Reset columns", {160, 40}, [this] { table_->setColumns(defaultColumns()); }});
        painter_ = std::make_shared<TagCells>();
        TableViewOptions options;
        options.rowHeight = 34;
        options.selectionMode = ListSelectionMode::Multiple;
        options.cellPainter = painter_;
        options.onSortRequested = [this](TableSort) { rebuild(); };
        options.onSelectionChanged = [this](const auto&) { updateSelection(); };
        options.onActivated = [this](ItemId id) { action_->setText("Opened tag #" + std::to_string(id)); };
        options.onColumnResized = [this](ColumnId id, float width)
        { action_->setText("Column #" + std::to_string(id) + " width: " + std::to_string(int(width))); };
        table_ = std::make_unique<TableView>(ui, root_, nullptr, defaultColumns(), options);
        ui.setLayoutHints(table_->id(), flex);
        selection_ = std::make_unique<Label>(ui, canvas, root_, LabelOptions{"0 selected"});
        action_ = std::make_unique<Label>(
            ui, canvas, root_,
            LabelOptions{
                "Header Tab stop: arrows choose a column, Enter/Space sorts. Body: arrows navigate, Enter opens."});
        stats_ = std::make_unique<Label>(ui, canvas, root_);
        for (ItemId id = 1; id <= 2000; ++id)
        {
            const auto number = std::to_string(id);
            const auto value = double(id) * .125;
            std::ostringstream formatted;
            formatted << std::fixed << std::setprecision(2) << value;
            rows_.push_back({id,
                             "Plant / Line " + std::to_string((id - 1) / 100 + 1) + " / TAG_" +
                                 std::string(4 - number.size(), '0') + number,
                             formatted.str(),
                             id % 2 ? "\xC2\xB0"
                                      "C"
                                    : "bar",
                             id % 2 ? "Temperature" : "Pressure",
                             id % 37 == 0   ? "Disabled"
                             : id % 13 == 0 ? "Alarm"
                                            : "Active",
                             value, id % 37 != 0});
        }
        table_->setSort(TableSort{1, SortDirection::Ascending});
        rebuild();
    }
    void prepare(const Viewport& viewport)
    {
        ui.layout(root_, {0, 0, viewport.width(), viewport.height()});
        const auto range = table_->visibleRows();
        const auto text = std::to_string(table_->model()->rowCount()) + " results / " +
                          std::to_string(range.end - range.first) + " viewport rows / horizontal offset " +
                          std::to_string(int(table_->horizontalScrollOffset()));
        if (stats_->text() != text)
            stats_->setText(text);
        painter_->calls = 0;
    }
    template <class F> void callback(F&& f) noexcept
    {
        if (error)
            return;
        try
        {
            f();
        }
        catch (...)
        {
            error = std::current_exception();
        }
    }
    void smokeTest()
    {
        const auto range = table_->visibleRows();
        if (!painter_->calls || painter_->calls > (range.end - range.first) * 5 || painter_->calls >= 200)
            throw std::runtime_error("Table painted offscreen rows/columns");
        table_->setSelection({1500});
        table_->setSort(TableSort{2, SortDirection::Descending});
        rebuild();
        if (table_->selection() != std::vector<ItemId>{1500})
            throw std::runtime_error("Sorting lost table selection");
        table_->setColumnWidth(1, 700);
        if (auto v = host.metrics().viewport())
            prepare(*v);
        table_->setHorizontalScrollOffset(10000);
        if (table_->horizontalScrollOffset() <= 0)
            throw std::runtime_error("Table horizontal scrolling failed");
        ui.focus(table_->bodyId());
        host.key(GLFW_KEY_END, GLFW_PRESS, 0);
        if (table_->currentItem() != 1 || table_->scrollOffset() <= 0 || table_->horizontalScrollOffset() <= 0)
            throw std::runtime_error("Table keyboard reveal reset columns/failed");
        host.focusChanged(false);
        if (ui.captured())
            throw std::runtime_error("Table capture survived focus loss");
    }

private:
    struct Tag
    {
        ItemId id;
        std::string name, value, unit, type, status;
        double numeric;
        bool enabled;
    };
    WidgetId root_;
    std::vector<Tag> rows_;
    std::shared_ptr<TagCells> painter_;
    std::unique_ptr<SearchField> search_;
    std::unique_ptr<Button> reset_;
    std::unique_ptr<TableView> table_;
    std::vector<std::unique_ptr<Label>> labels_;
    std::unique_ptr<Label> selection_, action_, stats_;
    static std::vector<TableColumn> defaultColumns()
    {
        return {{1, "Tag", 380, 180, 700},
                {2, "Value", 140, 80, 300, true, true, TextAlign::Right},
                {3, "Unit", 100, 60, 180, true, true, TextAlign::Center},
                {4, "Type", 150, 100, 300},
                {5, "State", 140, 90, 240}};
    }
    void addLabel(WidgetId parent, std::string text, float scale)
    {
        LabelOptions o;
        o.text = std::move(text);
        o.fontScale = scale;
        labels_.push_back(std::make_unique<Label>(ui, canvas, parent, o));
    }
    static std::string folded(std::string text)
    {
        for (auto& c : text)
            if (c >= 'A' && c <= 'Z')
                c = char(c - 'A' + 'a');
        return text;
    }
    void rebuild()
    {
        std::vector<const Tag*> visible;
        const auto query = folded(search_->query());
        for (const auto& row : rows_)
            if (folded(row.name + " " + row.type + " " + row.status).find(query) != std::string::npos)
                visible.push_back(&row);
        const auto sort = table_->sort();
        if (sort)
            std::stable_sort(visible.begin(), visible.end(),
                             [&](const Tag* a, const Tag* b)
                             {
                                 if (sort->direction == SortDirection::Descending)
                                     std::swap(a, b);
                                 if (sort->column == 2)
                                     return a->numeric < b->numeric;
                                 if (sort->column == 3)
                                     return a->unit < b->unit;
                                 if (sort->column == 4)
                                     return a->type < b->type;
                                 if (sort->column == 5)
                                     return a->status < b->status;
                                 return a->name < b->name;
                             });
        std::vector<TableItem> items;
        for (const auto* row : visible)
            items.push_back({row->id,
                             {{1, row->name}, {2, row->value}, {3, row->unit}, {4, row->type}, {5, row->status}},
                             row->enabled});
        table_->setModel(std::make_shared<VectorTableModel>(std::move(items)));
        updateSelection();
    }
    void updateSelection()
    {
        selection_->setText(std::to_string(table_->selection().size()) +
                            " selected / stable IDs survive filtering and sorting");
    }
};
} // namespace
int main(int argc, char** argv)
{
    bool smoke = false;
    std::string font, capture;
    int width = 1280, height = 860;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--width" && i + 1 < argc)
            width = std::stoi(argv[++i]);
        else if (arg == "--height" && i + 1 < argc)
            height = std::stoi(argv[++i]);
        else if (arg == "--smoke-test")
            smoke = true;
        else if (arg == "--font" && i + 1 < argc)
            font = argv[++i];
        else if (arg == "--screenshot" && i + 1 < argc)
            capture = argv[++i];
    }
    if (font.empty())
        for (auto path : {"C:/Windows/Fonts/segoeui.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"})
            if (std::filesystem::exists(path))
            {
                font = path;
                break;
            }
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, smoke ? GLFW_FALSE : GLFW_TRUE);
    auto* window = glfwCreateWindow(width, height, "VectorGL - virtualized table", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    int result = 0;
    try
    {
        if (!gladLoadGL(glfwGetProcAddress))
            throw std::runtime_error("GLAD initialization failed");
        TableDemo demo(window, font, smoke);
        glfwSetWindowUserPointer(window, &demo);
        struct CallbackScope
        {
            GLFWwindow* window;
            ~CallbackScope()
            {
                glfwSetCursorPosCallback(window, nullptr);
                glfwSetMouseButtonCallback(window, nullptr);
                glfwSetScrollCallback(window, nullptr);
                glfwSetKeyCallback(window, nullptr);
                glfwSetCharCallback(window, nullptr);
                glfwSetWindowFocusCallback(window, nullptr);
                glfwSetCursorEnterCallback(window, nullptr);
                glfwSetWindowSizeCallback(window, nullptr);
                glfwSetFramebufferSizeCallback(window, nullptr);
                glfwSetWindowContentScaleCallback(window, nullptr);
                glfwSetWindowRefreshCallback(window, nullptr);
                glfwSetWindowUserPointer(window, nullptr);
            }
        } callbacks{window};
        // Callbacks belong to this example, not the adapter. Catch at every native boundary.
        glfwSetCursorPosCallback(window,
                                 [](GLFWwindow* w, double x, double y)
                                 {
                                     auto& d = *static_cast<TableDemo*>(glfwGetWindowUserPointer(w));
                                     d.callback([&] { d.host.cursorPosition(x, y); });
                                 });
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow* w, int b, int a, int m)
                                   {
                                       auto& d = *static_cast<TableDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.mouseButton(b, a, m); });
                                   });
        glfwSetScrollCallback(window,
                              [](GLFWwindow* w, double x, double y)
                              {
                                  auto& d = *static_cast<TableDemo*>(glfwGetWindowUserPointer(w));
                                  d.callback([&] { d.host.scroll(x, y); });
                              });
        glfwSetKeyCallback(window,
                           [](GLFWwindow* w, int k, int, int a, int m)
                           {
                               auto& d = *static_cast<TableDemo*>(glfwGetWindowUserPointer(w));
                               d.callback(
                                   [&]
                                   {
                                       if (!d.host.key(k, a, m) && k == GLFW_KEY_ESCAPE && a == GLFW_PRESS)
                                           glfwSetWindowShouldClose(w, true);
                                   });
                           });
        glfwSetCharCallback(window,
                            [](GLFWwindow* w, unsigned int c)
                            {
                                auto& d = *static_cast<TableDemo*>(glfwGetWindowUserPointer(w));
                                d.callback([&] { d.host.character(c); });
                            });
        glfwSetWindowFocusCallback(window,
                                   [](GLFWwindow* w, int f)
                                   {
                                       auto& d = *static_cast<TableDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.focusChanged(f != 0); });
                                   });
        glfwSetCursorEnterCallback(window,
                                   [](GLFWwindow* w, int e)
                                   {
                                       auto& d = *static_cast<TableDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.cursorEntered(e != 0); });
                                   });
        glfwSetWindowSizeCallback(window,
                                  [](GLFWwindow* w, int, int)
                                  {
                                      auto& d = *static_cast<TableDemo*>(glfwGetWindowUserPointer(w));
                                      d.callback([&] { d.host.refreshMetrics(); });
                                  });
        glfwSetFramebufferSizeCallback(window,
                                       [](GLFWwindow* w, int, int)
                                       {
                                           auto& d = *static_cast<TableDemo*>(glfwGetWindowUserPointer(w));
                                           d.callback([&] { d.host.refreshMetrics(); });
                                       });
        glfwSetWindowContentScaleCallback(window,
                                          [](GLFWwindow* w, float, float)
                                          {
                                              auto& d = *static_cast<TableDemo*>(glfwGetWindowUserPointer(w));
                                              d.callback([&] { d.host.refreshMetrics(); });
                                          });
        glfwSetWindowRefreshCallback(window,
                                     [](GLFWwindow* w)
                                     {
                                         auto& d = *static_cast<TableDemo*>(glfwGetWindowUserPointer(w));
                                         d.callback([&] { d.ui.invalidate(); });
                                     });
        int frames = 0;
        while (!glfwWindowShouldClose(window))
        {
            if (demo.error)
                std::rethrow_exception(demo.error);
            demo.host.refreshMetrics();
            demo.ui.pumpTimers();
            auto viewport = demo.host.metrics().viewport();
            if (viewport)
                demo.prepare(*viewport);
            if (viewport && (demo.ui.takeInvalidation() != Invalidation::None || frames == 0 || smoke))
            {
                const auto bg = demo.ui.theme().background;
                glClearColor(bg.r, bg.g, bg.b, bg.a);
                glClear(GL_COLOR_BUFFER_BIT);
                demo.canvas.beginFrame(viewport->framebufferWidth(), viewport->framebufferHeight());
                demo.ui.paint(demo.canvas, *viewport);
                demo.canvas.endFrame();
                if (frames == 0 && !capture.empty())
                    screenshot(capture, viewport->framebufferWidth(), viewport->framebufferHeight());
                if (frames == 0 && smoke)
                    demo.smokeTest();
                if (glGetError() != GL_NO_ERROR)
                    throw std::runtime_error("UI host OpenGL error");
                glfwSwapBuffers(window);
                ++frames;
            }
            if (smoke)
            {
                if (frames >= 3)
                    break;
                glfwPollEvents();
            }
            else if (viewport && demo.ui.pendingInvalidation() != Invalidation::None)
                glfwPollEvents();
            else if (auto wakeup = demo.ui.nextWakeup())
                glfwWaitEventsTimeout(std::max(0.001, std::min(0.5, *wakeup - demo.ui.now())));
            else
                glfwWaitEvents();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        result = 1;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
