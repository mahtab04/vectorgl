#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <vectorgl/canvas.hpp>
#include <vectorgl/ui/containers.hpp>
#include <vectorgl/ui/glfw_adapter.hpp>
#include <vectorgl/ui/list_view.hpp>
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

class CatalogPainter final : public ListRowPainter
{
public:
    std::size_t calls = 0;
    void paint(PaintContext& p, const ListModel& model, const ListRow& row) override
    {
        ++calls;
        const auto b = row.bounds;
        const auto& theme = p.theme();
        p.fillRect(b, row.selected ? theme.pressed : (row.index % 2 ? theme.surface : theme.background));
        if (row.current && row.focused)
            p.fillRect({b.x, b.y, 3, b.height}, theme.accent);
        auto& c = p.canvas();
        if (!c.setFont(theme.fontPath, theme.fontSize))
            throw std::runtime_error("Could not load list demo font");
        c.setFillColor(row.enabled ? theme.text : theme.muted);
        c.fillText(std::string(model.text(row.index)), b.x + 14, b.y + (b.height - c.lineHeight()) / 2);
        c.setFillColor(row.enabled ? theme.accent : theme.muted);
        c.fillCircle(b.x + b.width - 17, b.y + b.height / 2, 3);
    }
};
class ListDemo
{
public:
    Canvas canvas;
    Context ui;
    GlfwAdapter host;
    std::exception_ptr error;
    ListDemo(GLFWwindow* window, std::string font, bool)
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
        addLabel(root_, "VectorGL / virtualized catalog", 1.6f);
        addLabel(root_, "2,000 stable IDs | Ctrl: toggle | Shift: range | Enter: activate", .9f);
        Widget toolbar;
        toolbar.parent = root_;
        toolbar.layout = std::make_shared<RowLayout>(Padding{}, 10.0f);
        toolbar_ = ui.add(toolbar);
        SearchFieldOptions searchOptions;
        searchOptions.onChanged = [this](std::string_view) { filter(); };
        search_ = std::make_unique<SearchField>(ui, canvas, toolbar_, searchOptions);
        LayoutHints flex;
        flex.flex = 1;
        ui.setLayoutHints(search_->id(), flex);
        sort_ = std::make_unique<Button>(ui, toolbar_,
                                         ButtonOptions{"Reverse order",
                                                       {150, 40},
                                                       [this]
                                                       {
                                                           descending_ = !descending_;
                                                           filter();
                                                       }});
        reset_ = std::make_unique<Button>(ui, toolbar_,
                                          ButtonOptions{"Reset catalog",
                                                        {150, 40},
                                                        [this]
                                                        {
                                                            populate();
                                                            search_->setQuery("");
                                                            filter();
                                                        }});
        Widget body;
        body.parent = root_;
        body.layout = std::make_shared<RowLayout>(Padding{}, 18.0f);
        body_ = ui.add(body);
        ui.setLayoutHints(body_, flex);
        painter_ = std::make_shared<CatalogPainter>();
        ListViewOptions options;
        options.rowHeight = 38;
        options.selectionMode = ListSelectionMode::Multiple;
        options.rowPainter = painter_;
        options.onSelectionChanged = [this](const auto&) { updateSelection(); };
        options.onActivated = [this](ItemId id) { activation_->setText("Activated item #" + std::to_string(id)); };
        list_ = std::make_unique<ListView>(ui, body_, nullptr, options);
        ui.setLayoutHints(list_->id(), flex);
        Widget detail;
        detail.parent = body_;
        detail.layout = std::make_shared<ColumnLayout>(Padding{16, 16, 16, 16}, 14.0f);
        detail.painter = std::make_shared<SurfacePainter>(SurfaceRole::Panel);
        auto detailId = ui.add(detail);
        LayoutHints panel;
        panel.width = 280.0f;
        ui.setLayoutHints(detailId, panel);
        addLabel(detailId, "Selection by identity", 1.15f);
        LabelOptions wrap;
        wrap.text = "Filter and reverse the catalog. Selected items keep their identity while they remain in the "
                    "results. Grey rows are unavailable.";
        wrap.overflow = TextOverflow::Wrap;
        labels_.push_back(std::make_unique<Label>(ui, canvas, detailId, wrap));
        selection_ = std::make_unique<Label>(ui, canvas, detailId, LabelOptions{"No selection"});
        activation_ = std::make_unique<Label>(ui, canvas, detailId, LabelOptions{"Enter activates the current row"});
        remove_ = std::make_unique<Button>(
            ui, detailId,
            ButtonOptions{"Remove selected",
                          {240, 40},
                          [this]
                          {
                              const auto ids = list_->selection();
                              std::erase_if(catalog_, [&](const auto& item)
                                            { return std::find(ids.begin(), ids.end(), item.id) != ids.end(); });
                              filter();
                          }});
        LabelOptions hint = wrap;
        hint.text = "One Tab stop. Arrows, Home/End and Page Up/Down navigate. Ctrl+arrows move the current row "
                    "without changing selection. Ctrl+A selects enabled results.";
        labels_.push_back(std::make_unique<Label>(ui, canvas, detailId, hint));
        stats_ = std::make_unique<Label>(ui, canvas, root_);
        populate();
        filter();
    }
    void prepare(const Viewport& viewport)
    {
        ui.layout(root_, {0, 0, viewport.width(), viewport.height()});
        const auto range = list_->visibleRows();
        const auto text = std::to_string(list_->model()->rowCount()) + " results / " +
                          std::to_string(range.end - range.first) + " visible rows / first row " +
                          std::to_string(range.first == range.end ? 0 : range.first + 1);
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
        const auto range = list_->visibleRows();
        if (painter_->calls != range.end - range.first || painter_->calls >= 100)
            throw std::runtime_error("List painting did not scale with visible rows");
        list_->setSelection({1500});
        descending_ = true;
        filter();
        if (list_->selection() != std::vector<ItemId>{1500})
            throw std::runtime_error("Reorder lost selection");
        if (auto v = host.metrics().viewport())
            prepare(*v);
        ui.focus(list_->id());
        host.key(GLFW_KEY_END, GLFW_PRESS, 0);
        if (list_->currentItem() != 1 || list_->scrollOffset() <= 0)
            throw std::runtime_error("Virtual list end navigation failed");
        host.focusChanged(false);
        if (ui.captured())
            throw std::runtime_error("List capture survived focus loss");
    }

private:
    WidgetId root_, toolbar_, body_;
    std::vector<ListItem> catalog_;
    bool descending_ = false;
    std::shared_ptr<CatalogPainter> painter_;
    std::unique_ptr<SearchField> search_;
    std::unique_ptr<ListView> list_;
    std::unique_ptr<Button> sort_, reset_, remove_;
    std::unique_ptr<Label> selection_, activation_, stats_;
    std::vector<std::unique_ptr<Label>> labels_;
    void addLabel(WidgetId parent, std::string text, float scale)
    {
        LabelOptions o;
        o.text = std::move(text);
        o.fontScale = scale;
        labels_.push_back(std::make_unique<Label>(ui, canvas, parent, o));
    }
    void populate()
    {
        catalog_.clear();
        for (ItemId i = 1; i <= 2000; ++i)
        {
            const auto number = std::to_string(i);
            catalog_.push_back({i,
                                "Resource " + std::string(4 - number.size(), '0') + number +
                                    (i % 3 == 0   ? " / Sensor"
                                     : i % 3 == 1 ? " / Screen"
                                                  : " / Symbol"),
                                i % 29 != 0});
        }
    }
    static std::string folded(std::string value)
    {
        for (auto& c : value)
            if (c >= 'A' && c <= 'Z')
                c = char(c - 'A' + 'a');
        return value;
    }
    void filter()
    {
        std::vector<ListItem> results;
        const auto query = folded(search_->query());
        for (const auto& item : catalog_)
            if (folded(item.text).find(query) != std::string::npos)
                results.push_back(item);
        if (descending_)
            std::reverse(results.begin(), results.end());
        std::vector<ItemId> enabled;
        for (const auto& item : catalog_)
            if (item.enabled)
                enabled.push_back(item.id);
        if (descending_)
            std::reverse(enabled.begin(), enabled.end());
        list_->setProjection(std::make_shared<VectorListModel>(std::move(results)), std::move(enabled));
        updateSelection();
    }
    void updateSelection()
    {
        selection_->setText(std::to_string(list_->selection().size()) + " selected");
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
    auto* window = glfwCreateWindow(width, height, "VectorGL - virtualized list", nullptr, nullptr);
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
        ListDemo demo(window, font, smoke);
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
                                     auto& d = *static_cast<ListDemo*>(glfwGetWindowUserPointer(w));
                                     d.callback([&] { d.host.cursorPosition(x, y); });
                                 });
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow* w, int b, int a, int m)
                                   {
                                       auto& d = *static_cast<ListDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.mouseButton(b, a, m); });
                                   });
        glfwSetScrollCallback(window,
                              [](GLFWwindow* w, double x, double y)
                              {
                                  auto& d = *static_cast<ListDemo*>(glfwGetWindowUserPointer(w));
                                  d.callback([&] { d.host.scroll(x, y); });
                              });
        glfwSetKeyCallback(window,
                           [](GLFWwindow* w, int k, int, int a, int m)
                           {
                               auto& d = *static_cast<ListDemo*>(glfwGetWindowUserPointer(w));
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
                                auto& d = *static_cast<ListDemo*>(glfwGetWindowUserPointer(w));
                                d.callback([&] { d.host.character(c); });
                            });
        glfwSetWindowFocusCallback(window,
                                   [](GLFWwindow* w, int f)
                                   {
                                       auto& d = *static_cast<ListDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.focusChanged(f != 0); });
                                   });
        glfwSetCursorEnterCallback(window,
                                   [](GLFWwindow* w, int e)
                                   {
                                       auto& d = *static_cast<ListDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.cursorEntered(e != 0); });
                                   });
        glfwSetWindowSizeCallback(window,
                                  [](GLFWwindow* w, int, int)
                                  {
                                      auto& d = *static_cast<ListDemo*>(glfwGetWindowUserPointer(w));
                                      d.callback([&] { d.host.refreshMetrics(); });
                                  });
        glfwSetFramebufferSizeCallback(window,
                                       [](GLFWwindow* w, int, int)
                                       {
                                           auto& d = *static_cast<ListDemo*>(glfwGetWindowUserPointer(w));
                                           d.callback([&] { d.host.refreshMetrics(); });
                                       });
        glfwSetWindowContentScaleCallback(window,
                                          [](GLFWwindow* w, float, float)
                                          {
                                              auto& d = *static_cast<ListDemo*>(glfwGetWindowUserPointer(w));
                                              d.callback([&] { d.host.refreshMetrics(); });
                                          });
        glfwSetWindowRefreshCallback(window,
                                     [](GLFWwindow* w)
                                     {
                                         auto& d = *static_cast<ListDemo*>(glfwGetWindowUserPointer(w));
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
