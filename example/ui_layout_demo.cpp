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
            throw std::runtime_error("Could not load the layout demo font");
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
            throw std::runtime_error("Could not load the layout demo font");
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
class ActionView final : public TextView
{
public:
    ActionView(Canvas& canvas, std::string text) : TextView(canvas, std::move(text), 0.85f, true, SurfaceRole::Control)
    {
    }
    std::function<void()> activate;
    EventReply event(const Event& e)
    {
        if (e.type == EventType::Cancel || e.type == EventType::FocusLost)
            pressed_ = false;
        if (e.type == EventType::PointerDown && e.button == PointerButton::Primary)
        {
            pressed_ = true;
            return {true, true, true};
        }
        if (e.type == EventType::PointerUp && e.button == PointerButton::Primary)
        {
            const bool click = pressed_ && bounds_.contains(e.localX, e.localY);
            pressed_ = false;
            if (click && activate)
                activate();
            return {true};
        }
        if (e.key == Key::Enter || e.key == Key::Space)
        {
            if (e.type == EventType::KeyDown)
                pressed_ = true;
            if (e.type == EventType::KeyUp)
            {
                const bool click = pressed_;
                pressed_ = false;
                if (click && activate)
                    activate();
            }
            return {true};
        }
        return {};
    }
    void paint(PaintContext& p) override
    {
        bounds_ = p.bounds();
        TextView::paint(p);
    }

private:
    Bounds bounds_;
    bool pressed_ = false;
};
class LayoutDemo
{
public:
    Canvas canvas;
    Context ui;
    GlfwAdapter host;
    std::exception_ptr error;
    LayoutDemo(GLFWwindow* window, std::string font, bool) : host(window, ui)
    {
        canvas.init();
        auto theme = ui.theme();
        theme.fontPath = std::move(font);
        ui.setTheme(theme);
        Widget root;
        root.layout = std::make_shared<ColumnLayout>(Padding{24, 24, 24, 24}, 18);
        root.clipChildren = true;
        root_ = ui.add(root);
        Widget heading;
        heading.parent = root_;
        heading.layout = std::make_shared<RowLayout>(Padding{}, 16, Alignment::Center);
        const auto header = ui.add(heading);
        auto title = text(header, "Responsive UI / layout", 1.65f);
        auto titleHints = LayoutHints{};
        titleHints.flex = 1;
        ui.setLayoutHints(title, titleHints);
        const auto type = action(header, "Change type size");
        type.second->activate = [this]
        {
            auto t = ui.theme();
            t.fontSize = t.fontSize > 18 ? 18 : 22;
            ui.setTheme(t);
        };
        text(root_, "Row, column and grid strategies. Resize the window; Tab through the property form. Esc closes.",
             0.85f);
        Widget body;
        body.parent = root_;
        body.layoutHints.flex = 1;
        body.layoutHints.minimum.height = 420;
        body_ = ui.add(body);
        Widget inspector;
        inspector.parent = body_;
        inspector.layout = std::make_shared<ColumnLayout>(Padding{20, 20, 20, 20}, 14);
        inspector.painter = std::make_shared<SurfacePainter>(SurfaceRole::Panel);
        inspector.clipChildren = true;
        inspector_ = ui.add(inspector);
        text(inspector_, "Properties", 1.3f);
        auto description = std::make_shared<TextView>(
            canvas, "Width-dependent text measurement keeps this explanation wrapped as the inspector changes size.",
            0.85f);
        description_ = registerText(inspector_, description);
        Widget form;
        form.parent = inspector_;
        form.layout =
            std::make_shared<GridLayout>(std::vector<GridTrack>{GridTrack::automatic(), GridTrack::fraction()},
                                         std::vector<GridTrack>(4, GridTrack::automatic()), Padding{}, 14, 12);
        const auto formId = ui.add(form);
        const std::vector<std::string> labels{"Name", "Type", "Value", "Unit"};
        const std::vector<std::string> values{"Mixer 07", "Numeric", "142.5", "rpm"};
        for (std::size_t row = 0; row < labels.size(); ++row)
        {
            const auto label = text(formId, labels[row], 0.85f);
            LayoutHints hint;
            hint.cell = {row, 0};
            hint.align = Alignment::Center;
            ui.setLayoutHints(label, hint);
            auto view = std::make_shared<TextView>(canvas, values[row], 0.85f, true);
            const auto value = registerText(formId, view, true);
            hint = {};
            hint.cell = {row, 1};
            hint.minimum.width = 110;
            hint.height = 44;
            ui.setLayoutHints(value, hint);
            fields_.push_back(value);
        }
        auto toggle = action(inspector_, "Toggle explanation");
        toggle.second->activate = [this, description]
        {
            expanded_ = !expanded_;
            description->text =
                expanded_
                    ? "A measurement callback receives the available width and shared font style. Longer content "
                      "changes its intrinsic height, invalidates parent measurement, and moves the rows below it. The "
                      "app never assigns each field a screen coordinate."
                    : "Width-dependent text measurement keeps this explanation wrapped as the inspector changes size.";
            ui.invalidate(Invalidation::Measure);
        };
        Widget preview;
        preview.parent = body_;
        preview.layout = std::make_shared<ColumnLayout>(Padding{20, 20, 20, 20}, 16);
        preview.layoutHints.flex = 1;
        preview.layoutHints.minimum = {240, 250};
        preview.painter = std::make_shared<SurfacePainter>(SurfaceRole::Panel);
        preview.clipChildren = true;
        preview_ = ui.add(preview);
        text(preview_, "Flexible space and spans", 1.3f);
        text(preview_,
             "The inspector has a bounded width beside this flexible panel. Narrow windows stack both panels without "
             "rebuilding the tree.",
             0.85f);
        Widget grid;
        grid.parent = preview_;
        grid.layout = std::make_shared<GridLayout>(
            std::vector<GridTrack>{GridTrack::fraction(), GridTrack::fraction(), GridTrack::fraction(2)},
            std::vector<GridTrack>{GridTrack::fixed(68), GridTrack::fixed(68)}, Padding{}, 12, 12);
        auto gridId = ui.add(grid);
        for (std::size_t i = 0; i < 4; ++i)
        {
            auto view = std::make_shared<TextView>(
                canvas, i < 3 ? (i == 2 ? "2 fr" : "1 fr") : "Spans all three columns", 0.9f, true);
            auto id = registerText(gridId, view, true);
            LayoutHints hints;
            hints.cell = i < 3 ? GridCell{0, i} : GridCell{1, 0, 1, 3};
            ui.setLayoutHints(id, hints);
        }
        text(preview_,
             "Minimum sizes stay valid when space is exhausted. See the workspace demo for scrolling and split panels.",
             0.85f);
        text(root_,
             "Focus, painting and hit testing use the same arranged geometry. Unchanged layouts reuse bounded "
             "measurement caches.",
             0.8f);
    }
    void prepare(const Viewport& viewport)
    {
        const bool narrow = viewport.width() < 900;
        if (!narrow_ || *narrow_ != narrow)
        {
            narrow_ = narrow;
            if (narrow)
                ui.setLayout(body_, std::make_shared<ColumnLayout>(Padding{}, 18));
            else
                ui.setLayout(body_, std::make_shared<RowLayout>(Padding{}, 20));
            LayoutHints hints;
            hints.minimum = {280, 390};
            if (!narrow)
                hints.width = 340;
            ui.setLayoutHints(inspector_, hints);
        }
        ui.layout(root_, {0, 0, viewport.width(), viewport.height()});
    }
    template <class F> void callback(F&& f) noexcept
    {
        try
        {
            host.refreshMetrics();
            if (auto viewport = host.metrics().viewport())
                prepare(*viewport);
            f();
            if (auto viewport = host.metrics().viewport())
                prepare(*viewport);
        }
        catch (...)
        {
            error = std::current_exception();
        }
    }
    void smokeTest()
    {
        const auto narrow = ui.measure(description_, {{}, {140, std::numeric_limits<float>::infinity()}});
        const auto wide = ui.measure(description_, {{}, {280, std::numeric_limits<float>::infinity()}});
        if (narrow.height < wide.height)
            throw std::runtime_error("Wrapped measurement height regressed");
        ui.focus(fields_.front());
        host.key(GLFW_KEY_TAB, GLFW_PRESS, 0);
        if (!ui.focused())
            throw std::runtime_error("Arranged fields lost Tab navigation");
        ui.layout(root_, {0, 0, 1280, 780});
        ui.layout(root_, {0, 0, 1280, 780});
        if (ui.layoutStats().measurements || ui.layoutStats().arrangedWidgets)
            throw std::runtime_error("Unchanged layout did work");
        host.focusChanged(false);
    }

private:
    WidgetId root_, body_, inspector_, preview_, description_;
    std::vector<WidgetId> fields_;
    std::optional<bool> narrow_;
    bool expanded_ = false;
    WidgetId registerText(WidgetId parent, std::shared_ptr<TextView> view, bool focusable = false)
    {
        Widget widget;
        widget.parent = parent;
        widget.focusable = focusable;
        widget.painter = view;
        widget.onMeasure = [view](const MeasureContext& c) { return view->measure(c); };
        widget.onEvent = [](Context&, WidgetId, const Event& e)
        { return EventReply{e.type == EventType::PointerDown, e.type == EventType::PointerDown}; };
        return ui.add(widget);
    }
    WidgetId text(WidgetId parent, std::string value, float scale = 1)
    {
        return registerText(parent, std::make_shared<TextView>(canvas, std::move(value), scale));
    }
    std::pair<WidgetId, std::shared_ptr<ActionView>> action(WidgetId parent, std::string label)
    {
        auto view = std::make_shared<ActionView>(canvas, std::move(label));
        Widget widget;
        widget.parent = parent;
        widget.focusable = true;
        widget.painter = view;
        widget.onMeasure = [view](const MeasureContext& c) { return view->measure(c); };
        widget.onEvent = [view](Context&, WidgetId, const Event& e) { return view->event(e); };
        return {ui.add(widget), view};
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
    auto* window = glfwCreateWindow(width, height, "VectorGL - responsive layout", nullptr, nullptr);
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
        LayoutDemo demo(window, font, smoke);
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
                                     auto& d = *static_cast<LayoutDemo*>(glfwGetWindowUserPointer(w));
                                     d.callback([&] { d.host.cursorPosition(x, y); });
                                 });
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow* w, int b, int a, int m)
                                   {
                                       auto& d = *static_cast<LayoutDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.mouseButton(b, a, m); });
                                   });
        glfwSetScrollCallback(window,
                              [](GLFWwindow* w, double x, double y)
                              {
                                  auto& d = *static_cast<LayoutDemo*>(glfwGetWindowUserPointer(w));
                                  d.callback([&] { d.host.scroll(x, y); });
                              });
        glfwSetKeyCallback(window,
                           [](GLFWwindow* w, int k, int, int a, int m)
                           {
                               auto& d = *static_cast<LayoutDemo*>(glfwGetWindowUserPointer(w));
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
                                auto& d = *static_cast<LayoutDemo*>(glfwGetWindowUserPointer(w));
                                d.callback([&] { d.host.character(c); });
                            });
        glfwSetWindowFocusCallback(window,
                                   [](GLFWwindow* w, int f)
                                   {
                                       auto& d = *static_cast<LayoutDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.focusChanged(f != 0); });
                                   });
        glfwSetCursorEnterCallback(window,
                                   [](GLFWwindow* w, int e)
                                   {
                                       auto& d = *static_cast<LayoutDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.cursorEntered(e != 0); });
                                   });
        glfwSetWindowSizeCallback(window,
                                  [](GLFWwindow* w, int, int)
                                  {
                                      auto& d = *static_cast<LayoutDemo*>(glfwGetWindowUserPointer(w));
                                      d.callback([&] { d.host.refreshMetrics(); });
                                  });
        glfwSetFramebufferSizeCallback(window,
                                       [](GLFWwindow* w, int, int)
                                       {
                                           auto& d = *static_cast<LayoutDemo*>(glfwGetWindowUserPointer(w));
                                           d.callback([&] { d.host.refreshMetrics(); });
                                       });
        glfwSetWindowContentScaleCallback(window,
                                          [](GLFWwindow* w, float, float)
                                          {
                                              auto& d = *static_cast<LayoutDemo*>(glfwGetWindowUserPointer(w));
                                              d.callback([&] { d.host.refreshMetrics(); });
                                          });
        glfwSetWindowRefreshCallback(window,
                                     [](GLFWwindow* w)
                                     {
                                         auto& d = *static_cast<LayoutDemo*>(glfwGetWindowUserPointer(w));
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
