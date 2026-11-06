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
#include <vectorgl/ui/controls.hpp>
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

class ControlsDemo
{
public:
    Canvas canvas;
    Context ui;
    GlfwAdapter host;
    std::exception_ptr error;
    ControlsDemo(GLFWwindow* window, std::string font, bool) : host(window, ui)
    {
        canvas.init();
        auto theme = ui.theme();
        theme.fontPath = std::move(font);
        theme.fontSize = 18;
        ui.setTheme(theme);
        Widget root;
        root.layout = std::make_shared<ColumnLayout>(Padding{24, 24, 24, 24}, 16.0f);
        root.clipChildren = true;
        root_ = ui.add(root);
        label(root_, "Controls / editable inspector", 1.5f);
        label(root_, "Tab to navigate. Enter commits; Escape restores. Try an empty name or a value outside 0-100.",
              0.9f);
        Widget body;
        body.parent = root_;
        body.layoutHints.flex = 1;
        body.clipChildren = true;
        body_ = ui.add(body);
        Widget inspector;
        inspector.parent = body_;
        inspector.layout = std::make_shared<ColumnLayout>(Padding{20, 20, 20, 20}, 12.0f);
        inspector.painter = std::make_shared<SurfacePainter>(SurfaceRole::Panel);
        inspector.clipChildren = true;
        inspector_ = ui.add(inspector);
        label(inspector_, "Properties", 1.2f);
        label(inspector_, "Name / required");
        TextFieldOptions name;
        name.value = "Orbit card";
        name.placeholder = "Give this item a name";
        name.validate = [](std::string_view value) { return value.empty() ? "Name is required" : ""; };
        name.onEdit = [this](const EditEvent& e)
        {
            editStatus(e.kind, e.error);
            refreshPreview();
        };
        name.onServiceError = [this](const ServiceResult& e)
        {
            status_->text = "Clipboard: " + e.message;
            ui.invalidate(Invalidation::Measure);
        };
        name_ = std::make_unique<TextField>(ui, canvas, inspector_, name);
        label(inspector_, "Opacity / 0-100 / step 0.5");
        NumericOptions number;
        number.value = 42;
        number.step = .5;
        number.precision = 1;
        number.onEdit = [this](const NumberEditEvent& e)
        {
            editStatus(e.kind, e.error);
            refreshPreview();
        };
        number.onServiceError = name.onServiceError;
        opacity_ = std::make_unique<NumericEditor>(ui, canvas, inspector_, number);
        visible_ = std::make_unique<CheckBox>(
            ui, inspector_, CheckBoxOptions{"Show preview", true, CheckStyle::Checkbox, {240, 44}, [this](bool value) {
                                                ui.setVisible(previewTextId_, value);
                                            }});
        enabled_ = std::make_unique<CheckBox>(
            ui, inspector_, CheckBoxOptions{"Allow Apply", true, CheckStyle::Toggle, {240, 44}, [this](bool value) {
                                                ui.setEnabled(apply_->id(), value);
                                            }});
        Widget actions;
        actions.parent = inspector_;
        actions.layout = std::make_shared<RowLayout>(Padding{}, 10.0f);
        const auto actionRow = ui.add(actions);
        apply_ = std::make_unique<Button>(ui, actionRow,
                                          ButtonOptions{"Apply",
                                                        {140, 44},
                                                        [this]
                                                        {
                                                            status_->text =
                                                                "Applied: " + name_->committedText() + " / opacity " +
                                                                InvariantNumberFormat{}.format(opacity_->value(), 1);
                                                            ui.invalidate(Invalidation::Measure);
                                                        }});
        reset_ = std::make_unique<Button>(ui, actionRow,
                                          ButtonOptions{"Reset",
                                                        {140, 44},
                                                        [this]
                                                        {
                                                            name_->setText("Orbit card");
                                                            opacity_->setValue(42);
                                                            refreshPreview();
                                                            status_->text = "Defaults restored";
                                                            ui.invalidate(Invalidation::Measure);
                                                        }});
        unavailable_ = std::make_unique<Button>(ui, inspector_, ButtonOptions{"Unavailable action", {240, 44}});
        ui.setEnabled(unavailable_->id(), false);
        Widget preview;
        preview.parent = body_;
        preview.layoutHints.flex = 1;
        preview.layout = std::make_shared<ColumnLayout>(Padding{24, 24, 24, 24}, 18.0f);
        preview.painter = std::make_shared<SurfacePainter>(SurfaceRole::Panel);
        preview.clipChildren = true;
        preview_ = ui.add(preview);
        label(preview_, "Committed values", 1.2f);
        label(preview_, "An edit stays in the field until it is valid and committed. Invalid values have a border and "
                        "an explanation below.");
        previewText_ = std::make_shared<TextView>(canvas, "", 1.2f, true);
        previewTextId_ = registerText(preview_, previewText_);
        label(preview_, "Mouse and keyboard share one activation path. Disabled actions leave the Tab order. Selection "
                        "uses Unicode codepoints.");
        label(preview_,
              "Numeric policy: invariant decimal point, no grouping. IME, grapheme editing, and undo are later work.",
              0.9f);
        status_ = std::make_shared<TextView>(canvas, "Ready / edit a property, then press Enter", 0.9f);
        registerText(root_, status_);
        refreshPreview();
    }
    void prepare(const Viewport& viewport)
    {
        const bool narrow = viewport.width() < 960;
        if (!narrow_ || *narrow_ != narrow)
        {
            narrow_ = narrow;
            if (narrow)
                ui.setLayout(body_, std::make_shared<ColumnLayout>(Padding{}, 16.0f));
            else
                ui.setLayout(body_, std::make_shared<RowLayout>(Padding{}, 20.0f));
            LayoutHints hints;
            if (!narrow)
                hints.width = 430.0f;
            ui.setLayoutHints(inspector_, hints);
        }
        ui.layout(root_, {0, 0, viewport.width(), viewport.height()});
    }
    ~ControlsDemo()
    {
        // Deliver blur/cancel callbacks while all example state is still alive.
        try
        {
            ui.remove(root_);
        }
        catch (...)
        {
        }
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
        ui.focus(name_->id());
        name_->selectAll();
        Event text;
        text.type = EventType::TextInput;
        text.text = "Edited card";
        ui.dispatch(text);
        host.key(GLFW_KEY_ENTER, GLFW_PRESS, 0);
        if (name_->committedText() != "Edited card")
            throw std::runtime_error("Inspector text commit failed");
        ui.focus(opacity_->fieldId());
        host.key(GLFW_KEY_UP, GLFW_PRESS, 0);
        if (opacity_->value() != 42.5)
            throw std::runtime_error("Inspector numeric step failed");
        ui.focus(apply_->id());
        host.key(GLFW_KEY_SPACE, GLFW_PRESS, 0);
        host.key(GLFW_KEY_SPACE, GLFW_RELEASE, 0);
        if (status_->text.find("Applied:") != 0)
            throw std::runtime_error("Inspector activation failed");
        host.focusChanged(false);
    }

private:
    WidgetId root_, body_, inspector_, preview_, previewTextId_;
    std::optional<bool> narrow_;
    std::shared_ptr<TextView> previewText_, status_;
    std::unique_ptr<TextField> name_;
    std::unique_ptr<NumericEditor> opacity_;
    std::unique_ptr<CheckBox> visible_, enabled_;
    std::unique_ptr<Button> apply_, reset_, unavailable_;
    WidgetId registerText(WidgetId parent, std::shared_ptr<TextView> view)
    {
        Widget w;
        w.parent = parent;
        w.painter = view;
        w.onMeasure = [view](const MeasureContext& c) { return view->measure(c); };
        return ui.add(w);
    }
    void label(WidgetId parent, std::string text, float scale = 1)
    {
        registerText(parent, std::make_shared<TextView>(canvas, std::move(text), scale));
    }
    void editStatus(EditKind kind, const std::string& error)
    {
        status_->text = !error.empty()                ? "Validation: " + error
                        : kind == EditKind::Changed   ? "Editing / Enter to commit, Escape to restore"
                        : kind == EditKind::Committed ? "Value committed"
                                                      : "Edit cancelled / previous value restored";
        ui.invalidate(Invalidation::Measure);
    }
    void refreshPreview()
    {
        if (!previewText_)
            return;
        previewText_->text =
            name_->committedText() + "\nOpacity: " + InvariantNumberFormat{}.format(opacity_->value(), 1);
        ui.invalidate(Invalidation::Measure);
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
    auto* window = glfwCreateWindow(width, height, "VectorGL - editable inspector", nullptr, nullptr);
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
        ControlsDemo demo(window, font, smoke);
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
                                     auto& d = *static_cast<ControlsDemo*>(glfwGetWindowUserPointer(w));
                                     d.callback([&] { d.host.cursorPosition(x, y); });
                                 });
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow* w, int b, int a, int m)
                                   {
                                       auto& d = *static_cast<ControlsDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.mouseButton(b, a, m); });
                                   });
        glfwSetScrollCallback(window,
                              [](GLFWwindow* w, double x, double y)
                              {
                                  auto& d = *static_cast<ControlsDemo*>(glfwGetWindowUserPointer(w));
                                  d.callback([&] { d.host.scroll(x, y); });
                              });
        glfwSetKeyCallback(window,
                           [](GLFWwindow* w, int k, int, int a, int m)
                           {
                               auto& d = *static_cast<ControlsDemo*>(glfwGetWindowUserPointer(w));
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
                                auto& d = *static_cast<ControlsDemo*>(glfwGetWindowUserPointer(w));
                                d.callback([&] { d.host.character(c); });
                            });
        glfwSetWindowFocusCallback(window,
                                   [](GLFWwindow* w, int f)
                                   {
                                       auto& d = *static_cast<ControlsDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.focusChanged(f != 0); });
                                   });
        glfwSetCursorEnterCallback(window,
                                   [](GLFWwindow* w, int e)
                                   {
                                       auto& d = *static_cast<ControlsDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.cursorEntered(e != 0); });
                                   });
        glfwSetWindowSizeCallback(window,
                                  [](GLFWwindow* w, int, int)
                                  {
                                      auto& d = *static_cast<ControlsDemo*>(glfwGetWindowUserPointer(w));
                                      d.callback([&] { d.host.refreshMetrics(); });
                                  });
        glfwSetFramebufferSizeCallback(window,
                                       [](GLFWwindow* w, int, int)
                                       {
                                           auto& d = *static_cast<ControlsDemo*>(glfwGetWindowUserPointer(w));
                                           d.callback([&] { d.host.refreshMetrics(); });
                                       });
        glfwSetWindowContentScaleCallback(window,
                                          [](GLFWwindow* w, float, float)
                                          {
                                              auto& d = *static_cast<ControlsDemo*>(glfwGetWindowUserPointer(w));
                                              d.callback([&] { d.host.refreshMetrics(); });
                                          });
        glfwSetWindowRefreshCallback(window,
                                     [](GLFWwindow* w)
                                     {
                                         auto& d = *static_cast<ControlsDemo*>(glfwGetWindowUserPointer(w));
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
