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
#include <vectorgl/ui/tree_view.hpp>
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

class TreeDemo
{
public:
    Canvas canvas;
    Context ui;
    GlfwAdapter host;
    std::exception_ptr error;
    TreeDemo(GLFWwindow* window, std::string font, bool)
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
        addLabel(root_, "VectorGL / project hierarchy", 1.6f);
        addLabel(root_, "Click arrows: expand | Left/Right: hierarchy | Ctrl/Shift: selection | Enter: open", .85f);
        Widget toolbar;
        toolbar.parent = root_;
        toolbar.layout = std::make_shared<RowLayout>(Padding{}, 10.0f);
        const auto tools = ui.add(toolbar);
        buttons_.push_back(std::make_unique<Button>(ui, tools,
                                                    ButtonOptions{"Expand all",
                                                                  {150, 40},
                                                                  [this]
                                                                  {
                                                                      std::vector<ItemId> branches{1, 900000};
                                                                      for (ItemId group = 0; group < 20; ++group)
                                                                          branches.push_back(100 + group);
                                                                      tree_->setExpandedItems(std::move(branches));
                                                                      updateSelection();
                                                                  }}));
        buttons_.push_back(std::make_unique<Button>(ui, tools,
                                                    ButtonOptions{"Collapse all",
                                                                  {150, 40},
                                                                  [this]
                                                                  {
                                                                      tree_->setExpandedItems({});
                                                                      updateSelection();
                                                                  }}));
        buttons_.push_back(std::make_unique<Button>(ui, tools,
                                                    ButtonOptions{"Reverse siblings",
                                                                  {160, 40},
                                                                  [this]
                                                                  {
                                                                      reversed_ = !reversed_;
                                                                      rebuild();
                                                                  }}));
        Widget body;
        body.parent = root_;
        body.layout = std::make_shared<RowLayout>(Padding{}, 18.0f);
        const auto bodyId = ui.add(body);
        LayoutHints flex;
        flex.flex = 1;
        ui.setLayoutHints(bodyId, flex);
        TreeViewOptions options;
        options.selectionMode = ListSelectionMode::Multiple;
        options.rowHeight = 34;
        options.onSelectionChanged = [this](const auto&) { updateSelection(); };
        options.onExpansionChanged = [this](ItemId id, bool expanded)
        {
            action_->setText(std::string(expanded ? "Expanded #" : "Collapsed #") + std::to_string(id));
            updateSelection();
        };
        options.onActivated = [this](ItemId id) { action_->setText("Opened item #" + std::to_string(id)); };
        tree_ = std::make_unique<TreeView>(ui, bodyId, nullptr, options);
        ui.setLayoutHints(tree_->id(), flex);
        ScrollOptions detailOptions;
        detailOptions.preferredSize = {260, 320};
        details_ = std::make_unique<ScrollView>(ui, bodyId, detailOptions);
        details_->setContentLayout(std::make_shared<ColumnLayout>(Padding{16, 16, 16, 16}, 16.0f));
        const auto sideId = details_->content();
        LayoutHints width;
        width.width = 260.0f;
        ui.setLayoutHints(details_->id(), width);
        addLabel(sideId, "Workspace explorer", 1.15f);
        LabelOptions wrap;
        wrap.overflow = TextOverflow::Wrap;
        wrap.text = "A project with 20 screens and 2,000 resources. Only viewport rows are painted. Unavailable "
                    "resources remain visible in grey.";
        labels_.push_back(std::make_unique<Label>(ui, canvas, sideId, wrap));
        selection_ = std::make_unique<Label>(ui, canvas, sideId, LabelOptions{"0 selected"});
        current_ = std::make_unique<Label>(ui, canvas, sideId, LabelOptions{"Current: project"});
        action_ = std::make_unique<Label>(ui, canvas, sideId, LabelOptions{"Expand a screen to explore"});
        wrap.text = "Expansion and selection survive sibling reordering by ID. Collapsing a branch retains hidden "
                    "selection and moves the current row to an enabled ancestor.";
        labels_.push_back(std::make_unique<Label>(ui, canvas, sideId, wrap));
        wrap.text = "Up/Down skip disabled rows. Home/End and Page Up/Down reveal the current item. Left collapses or "
                    "visits a parent; Right expands or enters a child.";
        labels_.push_back(std::make_unique<Label>(ui, canvas, sideId, wrap));
        stats_ = std::make_unique<Label>(ui, canvas, root_);
        rebuild();
        tree_->setExpandedItems({1, 100, 900000});
    }
    void prepare(const Viewport& viewport)
    {
        ui.layout(root_, {0, 0, viewport.width(), viewport.height()});
        const auto range = tree_->visibleRows();
        const auto value = std::to_string(tree_->visibleItemCount()) + " expanded rows / " +
                           std::to_string(range.end - range.first) + " viewport rows / 2,024 total nodes";
        if (stats_->text() != value)
            stats_->setText(value);
        const auto current = tree_->currentItem();
        const auto currentText = current ? "Current ID: " + std::to_string(*current) : "No current item";
        if (current_->text() != currentText)
            current_->setText(currentText);
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
        tree_->setSelection({1001});
        reversed_ = true;
        rebuild();
        if (tree_->selection() != std::vector<ItemId>{1001} || !tree_->isExpanded(100))
            throw std::runtime_error("Tree reordering lost state");
        tree_->setExpandedItems({1, 100});
        if (auto v = host.metrics().viewport())
            prepare(*v);
        ui.focus(tree_->id());
        host.key(GLFW_KEY_LEFT, GLFW_PRESS, 0);
        if (tree_->currentItem() != 100)
            throw std::runtime_error("Tree parent navigation failed");
        host.key(GLFW_KEY_LEFT, GLFW_PRESS, 0);
        if (tree_->isExpanded(100))
            throw std::runtime_error("Tree collapse failed");
        host.focusChanged(false);
        if (ui.captured())
            throw std::runtime_error("Tree capture survived focus loss");
    }

private:
    WidgetId root_;
    bool reversed_ = false;
    std::unique_ptr<TreeView> tree_;
    std::unique_ptr<ScrollView> details_;
    std::vector<std::unique_ptr<Button>> buttons_;
    std::vector<std::unique_ptr<Label>> labels_;
    std::unique_ptr<Label> selection_, current_, action_, stats_;
    void addLabel(WidgetId parent, std::string value, float scale)
    {
        LabelOptions o;
        o.text = std::move(value);
        o.fontScale = scale;
        labels_.push_back(std::make_unique<Label>(ui, canvas, parent, o));
    }
    void updateSelection()
    {
        selection_->setText(std::to_string(tree_->selection().size()) + " selected");
    }
    void rebuild()
    {
        std::vector<TreeItem> items{{1, 0, "Factory HMI"}};
        for (ItemId g = 0; g < 20; ++g)
        {
            const auto group = reversed_ ? 19 - g : g;
            items.push_back({100 + group, 1, "Screen " + std::to_string(group + 1)});
            for (ItemId r = 0; r < 100; ++r)
            {
                const auto resource = reversed_ ? 99 - r : r;
                const auto id = 1000 + group * 100 + resource;
                items.push_back({id, 100 + group,
                                 "Resource " + std::to_string(resource + 1) +
                                     (resource % 3 == 0   ? " / Temperature"
                                      : resource % 3 == 1 ? " / Pressure"
                                                          : " / Symbol"),
                                 resource % 23 != 22});
            }
        }
        items.push_back({900000, 0, "Shared library"});
        items.push_back({900001, 900000, "Icons / Caf\xC3\xA9"});
        items.push_back({900002, 900000, "Fonts / \xCE\xA9"});
        tree_->setModel(std::make_shared<VectorTreeModel>(std::move(items)));
        updateSelection();
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
    auto* window = glfwCreateWindow(width, height, "VectorGL - virtualized tree", nullptr, nullptr);
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
        TreeDemo demo(window, font, smoke);
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
                                     auto& d = *static_cast<TreeDemo*>(glfwGetWindowUserPointer(w));
                                     d.callback([&] { d.host.cursorPosition(x, y); });
                                 });
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow* w, int b, int a, int m)
                                   {
                                       auto& d = *static_cast<TreeDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.mouseButton(b, a, m); });
                                   });
        glfwSetScrollCallback(window,
                              [](GLFWwindow* w, double x, double y)
                              {
                                  auto& d = *static_cast<TreeDemo*>(glfwGetWindowUserPointer(w));
                                  d.callback([&] { d.host.scroll(x, y); });
                              });
        glfwSetKeyCallback(window,
                           [](GLFWwindow* w, int k, int, int a, int m)
                           {
                               auto& d = *static_cast<TreeDemo*>(glfwGetWindowUserPointer(w));
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
                                auto& d = *static_cast<TreeDemo*>(glfwGetWindowUserPointer(w));
                                d.callback([&] { d.host.character(c); });
                            });
        glfwSetWindowFocusCallback(window,
                                   [](GLFWwindow* w, int f)
                                   {
                                       auto& d = *static_cast<TreeDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.focusChanged(f != 0); });
                                   });
        glfwSetCursorEnterCallback(window,
                                   [](GLFWwindow* w, int e)
                                   {
                                       auto& d = *static_cast<TreeDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.cursorEntered(e != 0); });
                                   });
        glfwSetWindowSizeCallback(window,
                                  [](GLFWwindow* w, int, int)
                                  {
                                      auto& d = *static_cast<TreeDemo*>(glfwGetWindowUserPointer(w));
                                      d.callback([&] { d.host.refreshMetrics(); });
                                  });
        glfwSetFramebufferSizeCallback(window,
                                       [](GLFWwindow* w, int, int)
                                       {
                                           auto& d = *static_cast<TreeDemo*>(glfwGetWindowUserPointer(w));
                                           d.callback([&] { d.host.refreshMetrics(); });
                                       });
        glfwSetWindowContentScaleCallback(window,
                                          [](GLFWwindow* w, float, float)
                                          {
                                              auto& d = *static_cast<TreeDemo*>(glfwGetWindowUserPointer(w));
                                              d.callback([&] { d.host.refreshMetrics(); });
                                          });
        glfwSetWindowRefreshCallback(window,
                                     [](GLFWwindow* w)
                                     {
                                         auto& d = *static_cast<TreeDemo*>(glfwGetWindowUserPointer(w));
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
