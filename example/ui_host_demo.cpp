#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
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

class MemoryClipboard final : public Clipboard
{
public:
    bool failWrites = false;
    ClipboardResult read() override
    {
        return {{ServiceStatus::Success, {}}, text_};
    }
    ServiceResult write(std::string_view text) override
    {
        if (failWrites)
            return {ServiceStatus::Error, "Injected clipboard failure"};
        text_ = text;
        return {ServiceStatus::Success, {}};
    }

private:
    std::string text_;
};
class FieldView final : public WidgetPainter
{
public:
    explicit FieldView(Canvas& canvas) : canvas_(canvas) {}
    TextBox editor;
    EventReply event(Context& ui, WidgetId id, const Event& e)
    {
        if (e.type == EventType::FocusGained)
        {
            editor.setFocused(true);
            ui.requestWakeup(id, ui.now() + 0.5);
        }
        if (e.type == EventType::FocusLost)
        {
            editor.setFocused(false);
            ui.cancelWakeup(id);
        }
        if (e.type == EventType::Timer && editor.focused())
        {
            elapsed_ += 0.5f;
            ui.requestWakeup(id, ui.now() + 0.5);
        }
        if (e.type == EventType::PointerDown)
        {
            editor.handlePointerDown(e.localX, e.localY, canvas_);
            return {true, true, true};
        }
        if (e.type == EventType::PointerMove && ui.captured() == id)
        {
            editor.handlePointerDrag(e.localX, e.localY, canvas_);
            return {true};
        }
        if (e.type == EventType::TextInput)
        {
            editor.pasteText(e.text);
            return {true};
        }
        if (e.type != EventType::KeyDown && e.type != EventType::KeyRepeat)
            return {};
        if (e.modifiers.control || e.modifiers.meta)
        {
            if (e.key == Key::A)
            {
                editor.selectAll();
                return {true};
            }
            if (e.key == Key::C || e.key == Key::X || e.key == Key::V)
            {
                auto clipboard = ui.services().clipboard;
                if (clipboard)
                {
                    std::string selected;
                    if (e.key == Key::V)
                    {
                        auto data = clipboard->read();
                        if (data.result.status == ServiceStatus::Success)
                            editor.pasteText(data.text);
                    }
                    else if (editor.copySelection(selected) &&
                             clipboard->write(selected).status == ServiceStatus::Success && e.key == Key::X)
                        editor.cutSelection(selected);
                }
                return {true};
            }
        }
        TextBoxKey mapped;
        switch (e.key)
        {
        case Key::Backspace:
            mapped = TextBoxKey::Backspace;
            break;
        case Key::Delete:
            mapped = TextBoxKey::Delete;
            break;
        case Key::Left:
            mapped = TextBoxKey::Left;
            break;
        case Key::Right:
            mapped = TextBoxKey::Right;
            break;
        case Key::Home:
            mapped = TextBoxKey::Home;
            break;
        case Key::End:
            mapped = TextBoxKey::End;
            break;
        case Key::Enter:
            mapped = TextBoxKey::Enter;
            break;
        default:
            return {};
        }
        editor.handleKey(mapped, e.modifiers.shift);
        return {true};
    }
    void paint(PaintContext& p) override
    {
        const auto b = p.bounds();
        editor.setBounds(b.x, b.y, b.width, b.height);
        editor.setFont(p.theme().fontPath, p.theme().fontSize);
        editor.style().backgroundColor = p.enabled() ? p.theme().surface : p.theme().disabled;
        editor.style().textColor = p.theme().text;
        editor.style().focusedBorderColor = p.theme().accent;
        editor.render(p.canvas(), elapsed_);
        elapsed_ = 0;
    }

private:
    Canvas& canvas_;
    float elapsed_ = 0;
};
class Caption final : public WidgetPainter
{
public:
    explicit Caption(std::string text, bool heading = false) : text(std::move(text)), heading_(heading) {}
    std::string text;
    void paint(PaintContext& p) override
    {
        auto& c = p.canvas();
        c.setFont(p.theme().fontPath, heading_ ? 28 : 15);
        c.setFillColor(heading_ ? p.theme().text : p.theme().muted);
        c.fillText(text, p.bounds().x, p.bounds().y);
    }

private:
    bool heading_;
};
class ButtonView final : public SurfacePainter
{
public:
    std::function<void()> activate;
    EventReply event(Context&, WidgetId, const Event& e)
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
        SurfacePainter::paint(p);
        if (pressed_)
            p.fillRect(bounds_, p.theme().accent);
        auto& c = p.canvas();
        c.setFont(p.theme().fontPath, p.theme().fontSize);
        c.setFillColor(p.theme().text);
        c.fillText("Change palette", bounds_.x + 16, bounds_.y + 12);
    }

private:
    Bounds bounds_;
    bool pressed_ = false;
};
class RowView final : public SurfacePainter
{
public:
    explicit RowView(int number) : label_("Clipped row " + std::to_string(number)) {}
    void paint(PaintContext& p) override
    {
        SurfacePainter::paint(p);
        auto& c = p.canvas();
        c.setFont(p.theme().fontPath, 16);
        c.setFillColor(p.focused() ? p.theme().accent : p.theme().text);
        c.fillText(label_, p.bounds().x + 48, p.bounds().y + 10);
    }

private:
    std::string label_;
};

class HostDemo
{
public:
    Canvas canvas;
    Context ui;
    GlfwAdapter host;
    std::shared_ptr<FieldView> first, second;
    WidgetId firstId;
    std::exception_ptr error;
    HostDemo(GLFWwindow* window, std::string font, bool smoke)
        : ui(Services{{},
                      smoke ? std::shared_ptr<Clipboard>(std::make_shared<MemoryClipboard>())
                            : std::make_shared<GlfwClipboard>(window)}),
          host(window, ui)
    {
        canvas.init();
        auto theme = ui.theme();
        theme.fontPath = std::move(font);
        ui.setTheme(theme);
        auto caption = [&](std::string text, Bounds bounds, bool heading = false)
        {
            Widget w;
            w.bounds = bounds;
            w.painter = std::make_shared<Caption>(std::move(text), heading);
            ui.add(std::move(w));
        };
        caption("VectorGL / application UI", {30, 22, 700, 42}, true);
        caption("Tab: focus  |  type and select  |  Ctrl+A/C/X/V  |  Esc: close", {30, 70, 700, 24});
        Widget panel;
        panel.bounds = {0, 0, 370, 320};
        panel.transform = Mat3x3::translation(30, 124);
        panel.clipChildren = true;
        panel.painter = std::make_shared<SurfacePainter>();
        auto panelId = ui.add(panel);
        for (int i = 0; i < 2; ++i)
        {
            auto view = std::make_shared<FieldView>(canvas);
            view->editor.setText(i ? "Unicode: Caf\xc3\xa9 / \xce\xa9" : "Select and edit this field");
            Widget w;
            w.parent = panelId;
            w.bounds = {16, 30.0f + i * 82, 338, 54};
            w.focusable = true;
            w.painter = view;
            w.onEvent = [view](Context& ui, WidgetId id, const Event& e) { return view->event(ui, id, e); };
            auto id = ui.add(w);
            if (i)
                second = view;
            else
            {
                first = view;
                firstId = id;
            }
        }
        auto button = std::make_shared<ButtonView>();
        button->activate = [this]
        {
            auto t = ui.theme();
            t.accent = t.accent.b > 0.5f ? Color{0.95f, 0.52f, 0.23f, 1} : Color{0.22f, 0.75f, 0.94f, 1};
            t.surface = t.accent.b > 0.5f ? Color{0.09f, 0.12f, 0.18f, 1} : Color{0.16f, 0.11f, 0.10f, 1};
            ui.setTheme(t);
        };
        Widget w;
        w.parent = panelId;
        w.bounds = {16, 222, 220, 52};
        w.focusable = true;
        w.painter = button;
        w.onEvent = [button](Context& ui, WidgetId id, const Event& e) { return button->event(ui, id, e); };
        ui.add(w);
        Widget clip;
        clip.bounds = {0, 0, 230, 200};
        clip.transform = Mat3x3::translation(465, 158) * Mat3x3::rotation(0.13f);
        clip.clipChildren = true;
        clip.painter = std::make_shared<SurfacePainter>();
        auto clipId = ui.add(clip);
        for (int i = 0; i < 4; ++i)
        {
            Widget row;
            row.parent = clipId;
            row.bounds = {-26, 16.0f + 52 * i, 300, 40};
            row.painter = std::make_shared<RowView>(i + 1);
            row.focusable = true;
            row.onEvent = [](Context&, WidgetId, const Event& e)
            { return EventReply{e.type == EventType::PointerDown, e.type == EventType::PointerDown}; };
            ui.add(row);
        }
        caption("Rotated clip / matching input", {448, 392, 340, 26});
        caption("Fixed logical rectangles. Scale follows the display; artboard zoom stays separate.",
                {30, 480, 900, 24});
        caption("Window exit or focus loss cancels capture. Rendering sleeps until input or a caret deadline.",
                {30, 512, 900, 24});
        host.setCursor(Cursor::Arrow);
    }
    template <class F> void callback(F&& fn) noexcept
    {
        try
        {
            fn();
        }
        catch (...)
        {
            error = std::current_exception();
        }
    }
    void smokeTest()
    {
        ui.focus(firstId);
        host.key(GLFW_KEY_A, GLFW_PRESS, GLFW_MOD_CONTROL);
        host.character(0x03a9);
        host.key(GLFW_KEY_A, GLFW_PRESS, GLFW_MOD_CONTROL);
        host.key(GLFW_KEY_C, GLFW_PRESS, GLFW_MOD_CONTROL);
        host.key(GLFW_KEY_TAB, GLFW_PRESS, 0);
        host.key(GLFW_KEY_A, GLFW_PRESS, GLFW_MOD_CONTROL);
        host.key(GLFW_KEY_V, GLFW_PRESS, GLFW_MOD_CONTROL);
        if (!second->editor.focused() || first->editor.text() != "\xce\xa9" || second->editor.text() != "\xce\xa9")
            throw std::runtime_error("UI host routing failed");
        const auto clipboard = std::dynamic_pointer_cast<MemoryClipboard>(ui.services().clipboard);
        if (!clipboard)
            throw std::runtime_error("Smoke test requires an injected clipboard");
        clipboard->failWrites = true;
        host.key(GLFW_KEY_A, GLFW_PRESS, GLFW_MOD_CONTROL);
        host.key(GLFW_KEY_X, GLFW_PRESS, GLFW_MOD_CONTROL);
        if (second->editor.text() != "\xce\xa9" || !second->editor.hasSelection())
            throw std::runtime_error("Failed clipboard write deleted the edit buffer");
        host.focusChanged(false);
        if (ui.focused() || ui.captured())
            throw std::runtime_error("UI host focus cancellation failed");
    }
};
} // namespace

int main(int argc, char** argv)
{
    bool smoke = false;
    std::string font, capture;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--smoke-test")
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
    auto* window = glfwCreateWindow(1200, 850, "VectorGL - UI host", nullptr, nullptr);
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
        HostDemo demo(window, font, smoke);
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
                                     auto& d = *static_cast<HostDemo*>(glfwGetWindowUserPointer(w));
                                     d.callback([&] { d.host.cursorPosition(x, y); });
                                 });
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow* w, int b, int a, int m)
                                   {
                                       auto& d = *static_cast<HostDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.mouseButton(b, a, m); });
                                   });
        glfwSetScrollCallback(window,
                              [](GLFWwindow* w, double x, double y)
                              {
                                  auto& d = *static_cast<HostDemo*>(glfwGetWindowUserPointer(w));
                                  d.callback([&] { d.host.scroll(x, y); });
                              });
        glfwSetKeyCallback(window,
                           [](GLFWwindow* w, int k, int, int a, int m)
                           {
                               auto& d = *static_cast<HostDemo*>(glfwGetWindowUserPointer(w));
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
                                auto& d = *static_cast<HostDemo*>(glfwGetWindowUserPointer(w));
                                d.callback([&] { d.host.character(c); });
                            });
        glfwSetWindowFocusCallback(window,
                                   [](GLFWwindow* w, int f)
                                   {
                                       auto& d = *static_cast<HostDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.focusChanged(f != 0); });
                                   });
        glfwSetCursorEnterCallback(window,
                                   [](GLFWwindow* w, int e)
                                   {
                                       auto& d = *static_cast<HostDemo*>(glfwGetWindowUserPointer(w));
                                       d.callback([&] { d.host.cursorEntered(e != 0); });
                                   });
        glfwSetWindowSizeCallback(window,
                                  [](GLFWwindow* w, int, int)
                                  {
                                      auto& d = *static_cast<HostDemo*>(glfwGetWindowUserPointer(w));
                                      d.callback([&] { d.host.refreshMetrics(); });
                                  });
        glfwSetFramebufferSizeCallback(window,
                                       [](GLFWwindow* w, int, int)
                                       {
                                           auto& d = *static_cast<HostDemo*>(glfwGetWindowUserPointer(w));
                                           d.callback([&] { d.host.refreshMetrics(); });
                                       });
        glfwSetWindowContentScaleCallback(window,
                                          [](GLFWwindow* w, float, float)
                                          {
                                              auto& d = *static_cast<HostDemo*>(glfwGetWindowUserPointer(w));
                                              d.callback([&] { d.host.refreshMetrics(); });
                                          });
        glfwSetWindowRefreshCallback(window,
                                     [](GLFWwindow* w)
                                     {
                                         auto& d = *static_cast<HostDemo*>(glfwGetWindowUserPointer(w));
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
