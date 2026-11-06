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
#include <vectorgl/text_box.hpp>
#include <vectorgl/ui/containers.hpp>
#include <vectorgl/ui/glfw_adapter.hpp>
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

class TextView : public WidgetPainter
{
public:
    TextView(Canvas& canvas, std::string value, float scale = 1, bool surface = false,
             SurfaceRole role = SurfaceRole::Panel)
        : text(std::move(value)), canvas_(canvas), scale_(scale), surface_(surface), role_(role)
    {
    }
    std::string text;
    Size measure(const MeasureContext& c)
    {
        if (!canvas_.setFont(c.theme.fontPath, c.theme.fontSize * scale_))
            throw std::runtime_error("Could not load the workspace demo font");
        TextLayoutOptions options;
        const float padding = surface_ ? 24.0f : 0.0f;
        if (std::isfinite(c.constraints.maximum.width))
            options.maxWidth = std::max(1.0f, c.constraints.maximum.width - padding);
        const auto result = canvas_.layoutText(text, options);
        return {result.width + padding, result.height + (surface_ ? 20.0f : 0.0f)};
    }
    void paint(PaintContext& p) override
    {
        if (surface_)
            SurfacePainter{role_}.paint(p);
        const auto b = p.bounds();
        const float padding = surface_ ? 12.0f : 0.0f;
        auto& canvas = p.canvas();
        if (!canvas.setFont(p.theme().fontPath, p.theme().fontSize * scale_))
            throw std::runtime_error("Could not load the workspace demo font");
        canvas.setFillColor(p.theme().text);
        TextLayoutOptions options;
        options.maxWidth = std::max(1.0f, b.width - padding * 2);
        canvas.fillText(text, b.x + padding, b.y + (surface_ ? 10 : 0), options);
    }

private:
    Canvas& canvas_;
    float scale_;
    bool surface_;
    SurfaceRole role_;
};

class Artboard final : public WidgetPainter
{
    void paint(PaintContext& p) override
    {
        auto& c = p.canvas();
        p.fillRect(p.bounds(), p.theme().background);
        c.setFillColor(p.theme().border);
        for (int y = 24; y < 900; y += 24)
            for (int x = 24; x < 1200; x += 24)
                c.fillCircle(float(x), float(y), 1);
        c.setFillColor(p.theme().surface);
        c.fillRoundedRect(70, 80, 340, 220, 16);
        c.setFillColor(p.theme().accent);
        c.fillRoundedRect(92, 102, 296, 10, 5);
        c.setFont(p.theme().fontPath, 24);
        c.setFillColor(p.theme().text);
        c.fillText("Your canvas, your layout", 92, 143);
        c.setFont(p.theme().fontPath, 16);
        c.fillText("Two-axis scrolling / clipped drawing", 92, 186);
        c.setFillColor(Color{0.56f, 0.42f, 0.92f, 1});
        c.fillCircle(490, 360, 68);
        c.setFillColor(p.theme().accent);
        c.fillRoundedRect(180, 390, 210, 90, 12);
    }
};
class WorkspaceDemo
{
public:
    Canvas canvas;
    Context ui;
    GlfwAdapter host;
    std::exception_ptr error;
    WorkspaceDemo(GLFWwindow* window, std::string font, bool) : host(window, ui)
    {
        canvas.init();
        auto theme = ui.theme();
        theme.fontPath = std::move(font);
        theme.fontSize = 16;
        ui.setTheme(theme);
        Widget root;
        root.layout = std::make_shared<ColumnLayout>(Padding{20, 20, 20, 20}, 12.0f);
        root.clipChildren = true;
        root_ = ui.add(root);
        text(root_, "VectorGL / workspace", 1.65f);
        text(root_, "Drag dividers | Tab: focus + reveal | Arrows / Page Up / Down: scroll | Esc: close", 0.85f);
        SplitOptions left;
        left.fraction = 0.20f;
        left.firstMinimum = 180;
        left.firstMaximum = 360;
        left.secondMinimum = 520;
        outer_ = std::make_unique<SplitPanel>(ui, root_, left);
        LayoutHints flex;
        flex.flex = 1;
        ui.setLayoutHints(outer_->id(), flex);
        column(outer_->second());
        SplitOptions right;
        right.fraction = 0.73f;
        right.firstMinimum = 300;
        right.secondMinimum = 200;
        right.secondMaximum = 420;
        middle_ = std::make_unique<SplitPanel>(ui, outer_->second(), right);
        ui.setLayoutHints(middle_->id(), flex);
        column(middle_->first());
        SplitOptions bottom;
        bottom.axis = SplitAxis::Vertical;
        bottom.fraction = 0.70f;
        bottom.firstMinimum = 160;
        bottom.secondMinimum = 100;
        bottom.secondMaximum = 280;
        center_ = std::make_unique<SplitPanel>(ui, middle_->first(), bottom);
        ui.setLayoutHints(center_->id(), flex);

        toolbox_ = panel(outer_->first(), "Toolbox");
        text(toolbox_->content(), "Nested list: scroll here, then keep going at its edge.", 0.85f);
        nested_ = std::make_unique<ScrollView>(ui, toolbox_->content());
        LayoutHints nestedHints;
        nestedHints.height = 180.0f;
        ui.setLayoutHints(nested_->id(), nestedHints);
        nested_->setContentLayout(std::make_shared<ColumnLayout>(Padding{8, 8, 8, 8}, 6.0f));
        for (int i = 0; i < 14; ++i)
            text(nested_->content(), "Symbol " + std::to_string(i + 1), 1, true);
        for (int i = 0; i < 18; ++i)
            text(toolbox_->content(), "Library group " + std::to_string(i + 1), 1, true);

        inspector_ = panel(middle_->second(), "Inspector");
        for (int i = 0; i < 24; ++i)
            lastField_ = text(inspector_->content(), "Property " + std::to_string(i + 1) + "   /   Default", 1, true);
        console_ = panel(center_->second(), "Activity");
        for (int i = 0; i < 30; ++i)
            text(console_->content(), "[" + std::to_string(i + 1) + "] Workspace ready / layout resolved", 0.85f);
        column(center_->first());
        text(center_->first(), "Artboard / 1200 x 900", 1.1f);
        ScrollOptions both;
        both.horizontal = true;
        artboard_ = std::make_unique<ScrollView>(ui, center_->first(), both);
        ui.setLayoutHints(artboard_->id(), flex);
        artboard_->setContentSize({1200, 900});
        Widget drawing;
        drawing.parent = artboard_->content();
        drawing.bounds = {0, 0, 1200, 900};
        drawing.painter = std::make_shared<Artboard>();
        ui.add(drawing);
        text(root_, "4 regions / constrained splitters / nested scrolling / no Qt", 0.8f);
    }
    void prepare(const Viewport& viewport)
    {
        ui.layout(root_, {0, 0, viewport.width(), viewport.height()});
    }
    template <class F> void callback(F&& f) noexcept
    {
        try
        {
            host.refreshMetrics();
            if (auto v = host.metrics().viewport())
                prepare(*v);
            f();
            if (auto v = host.metrics().viewport())
                prepare(*v);
        }
        catch (...)
        {
            error = std::current_exception();
        }
    }
    void smokeTest()
    {
        ui.focus(lastField_);
        if (inspector_->offset().y <= 0)
            throw std::runtime_error("Workspace focus reveal failed");
        inspector_->setOffset({0, 0});
        ui.focus(outer_->divider());
        host.key(GLFW_KEY_RIGHT, GLFW_PRESS, 0);
        if (auto v = host.metrics().viewport())
            prepare(*v);
        ui.focus({});
        host.focusChanged(false);
        if (ui.captured())
            throw std::runtime_error("Workspace capture survived focus loss");
    }

private:
    WidgetId root_, lastField_;
    // Context precedes controllers: their registered subtrees are removed first.
    std::unique_ptr<SplitPanel> outer_, middle_, center_;
    std::unique_ptr<ScrollView> toolbox_, nested_, inspector_, console_, artboard_;
    void column(WidgetId id)
    {
        ui.setLayout(id, std::make_shared<ColumnLayout>(Padding{12, 12, 12, 12}, 10.0f));
    }
    std::unique_ptr<ScrollView> panel(WidgetId pane, const std::string& title)
    {
        column(pane);
        text(pane, title, 1.15f);
        auto view = std::make_unique<ScrollView>(ui, pane);
        LayoutHints hints;
        hints.flex = 1;
        ui.setLayoutHints(view->id(), hints);
        view->setContentLayout(std::make_shared<ColumnLayout>(Padding{8, 8, 8, 8}, 8.0f));
        return view;
    }
    WidgetId text(WidgetId parent, std::string label, float scale = 1, bool focusable = false)
    {
        auto view = std::make_shared<TextView>(canvas, std::move(label), scale, focusable);
        Widget w;
        w.parent = parent;
        w.focusable = focusable;
        w.painter = view;
        w.onMeasure = [view](const MeasureContext& c) { return view->measure(c); };
        w.onEvent = [](Context&, WidgetId, const Event& e)
        {
            const bool down = e.type == EventType::PointerDown && e.button == PointerButton::Primary;
            return EventReply{down, down};
        };
        return ui.add(w);
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
    auto* window = glfwCreateWindow(width, height, "VectorGL - split workspace", nullptr, nullptr);
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
        WorkspaceDemo demo(window, font, smoke);
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
                                     auto& d = *static_cast<WorkspaceDemo*>(glfwGetWindowUserPointer(w));
                                     d.callback([&] { d.host.cursorPosition(x, y); });
                                 });
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow* w, int b, int a, int m)
                                   {
                                       auto& d = *static_cast<WorkspaceDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.mouseButton(b, a, m); });
                                   });
        glfwSetScrollCallback(window,
                              [](GLFWwindow* w, double x, double y)
                              {
                                  auto& d = *static_cast<WorkspaceDemo*>(glfwGetWindowUserPointer(w));
                                  d.callback([&] { d.host.scroll(x, y); });
                              });
        glfwSetKeyCallback(window,
                           [](GLFWwindow* w, int k, int, int a, int m)
                           {
                               auto& d = *static_cast<WorkspaceDemo*>(glfwGetWindowUserPointer(w));
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
                                auto& d = *static_cast<WorkspaceDemo*>(glfwGetWindowUserPointer(w));
                                d.callback([&] { d.host.character(c); });
                            });
        glfwSetWindowFocusCallback(window,
                                   [](GLFWwindow* w, int f)
                                   {
                                       auto& d = *static_cast<WorkspaceDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.focusChanged(f != 0); });
                                   });
        glfwSetCursorEnterCallback(window,
                                   [](GLFWwindow* w, int e)
                                   {
                                       auto& d = *static_cast<WorkspaceDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.cursorEntered(e != 0); });
                                   });
        glfwSetWindowSizeCallback(window,
                                  [](GLFWwindow* w, int, int)
                                  {
                                      auto& d = *static_cast<WorkspaceDemo*>(glfwGetWindowUserPointer(w));
                                      d.callback([&] { d.host.refreshMetrics(); });
                                  });
        glfwSetFramebufferSizeCallback(window,
                                       [](GLFWwindow* w, int, int)
                                       {
                                           auto& d = *static_cast<WorkspaceDemo*>(glfwGetWindowUserPointer(w));
                                           d.callback([&] { d.host.refreshMetrics(); });
                                       });
        glfwSetWindowContentScaleCallback(window,
                                          [](GLFWwindow* w, float, float)
                                          {
                                              auto& d = *static_cast<WorkspaceDemo*>(glfwGetWindowUserPointer(w));
                                              d.callback([&] { d.host.refreshMetrics(); });
                                          });
        glfwSetWindowRefreshCallback(window,
                                     [](GLFWwindow* w)
                                     {
                                         auto& d = *static_cast<WorkspaceDemo*>(glfwGetWindowUserPointer(w));
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
