#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vectorgl/canvas.hpp>
#include <vectorgl/ui/containers.hpp>
#include <vectorgl/ui/controls.hpp>
#include <vectorgl/ui/glfw_adapter.hpp>
#include <vectorgl/ui/overlays.hpp>
#include <vectorgl/ui/workspace_controls.hpp>

using namespace vectorgl;
using namespace vectorgl::ui;
namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
class Solid final : public WidgetPainter
{
public:
    explicit Solid(Color color) : color_(color) {}
    void paint(PaintContext& p) override
    {
        p.fillRect(p.bounds(), color_);
    }

private:
    Color color_;
};
void pixel(int x, int y, int r, int g, int b)
{
    std::array<unsigned char, 4> value{};
    glReadPixels(x, 255 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, value.data());
    require(std::abs(int(value[0]) - r) < 12 && std::abs(int(value[1]) - g) < 12 && std::abs(int(value[2]) - b) < 12,
            "paint/hit-test pixel mismatch");
}
void paintTests(Canvas& canvas)
{
    for (float scale : {1.0f, 1.25f, 1.5f, 2.0f})
    {
        Context ui;
        auto theme = ui.theme();
        theme.fontPath = VECTORGL_TEST_FONT;
        theme.fontSize = 16;
        ui.setTheme(theme);
        Label label(ui, canvas, {}, LabelOptions{"AAAAAAAAAAAA"});
        ui.layout(label.id(), {10, 10, 60, 28});
        require(canvas.setFont(theme.fontPath, theme.fontSize), "label test font unavailable");
        const auto expected = ellipsizeText(label.text(), 60, [&](std::string_view text)
                                            { return canvas.measureText(std::string(text)); });
        require(canvas.measureText(expected) <= 60.01f && expected != label.text(), "font-backed ellipsis did not fit");
        std::vector<unsigned char> actual(256 * 256 * 4), reference(actual.size());
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.beginFrame(256, 256);
        ui.paint(canvas, Viewport(256 / scale, 256 / scale, 256, 256));
        canvas.endFrame();
        glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.beginFrame(256, 256);
        canvas.save();
        canvas.setTransform(Mat3x3::scaling(scale, scale));
        canvas.clipRect(10, 10, 60, 28);
        canvas.setFillColor(theme.text);
        TextLayoutOptions layout;
        layout.maxWidth = 60;
        layout.wrap = TextWrap::None;
        canvas.fillText(expected, 10, 10, layout);
        canvas.restore();
        canvas.endFrame();
        glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, reference.data());
        require(actual == reference, "label pixels differed from expected clipped ellipsis at DPI scale");
        ui.remove(label.id());
        Separator line(ui, {}, SeparatorOptions{SeparatorAxis::Horizontal, 2, 70, 5});
        ui.layout(line.id(), {10, 50, 70, 4});
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.beginFrame(256, 256);
        ui.paint(canvas, Viewport(256 / scale, 256 / scale, 256, 256));
        canvas.endFrame();
        const auto color = theme.border;
        pixel(int(20 * scale), int(52 * scale), int(color.r * 255), int(color.g * 255), int(color.b * 255));
        pixel(int(11 * scale), int(52 * scale), 0, 0, 0);
    }
    for (float scale : {1.0f, 1.25f, 1.5f, 2.0f})
    {
        Context ui;
        OverlayManager overlays(ui);
        overlays.update({0, 0, 128, 128});
        Button owner(ui);
        ui.layout(owner.id(), {4, 4, 20, 20});
        PopupOptions options;
        options.items = {{""}};
        options.width = 70;
        options.rowHeight = 20;
        const auto popup = overlays.openPopup(owner.id(), options);
        overlays.update({0, 0, 128, 128});
        Widget later;
        later.bounds = {0, 0, 128, 128};
        later.painter = std::make_shared<Solid>(Color{1, 0, 0, 1});
        ui.add(later);
        const auto b = *overlays.contentBounds(popup);
        canvas.beginFrame(256, 256);
        ui.paint(canvas, Viewport(256 / scale, 256 / scale, 256, 256));
        canvas.endFrame();
        const auto color = ui.theme().surface;
        pixel(int((b.x + 20) * scale), int((b.y + 14) * scale), int(color.r * 255), int(color.g * 255),
              int(color.b * 255));
        require(ui.isDescendant(ui.hitTest({b.x + 20, b.y + 14}), overlays.root(popup)),
                "popup painted above content but input hit content");
    }
    for (float scale : {1.0f, 1.25f, 1.5f, 2.0f})
    {
        Context controls;
        auto theme = controls.theme();
        theme.fontPath = VECTORGL_TEST_FONT;
        controls.setTheme(theme);
        Button button(controls);
        controls.layout(button.id(), {10, 10, 100, 44});
        TextField field(controls, canvas, {}, TextFieldOptions{"ABCD"});
        controls.layout(field.id(), {10, 70, 180, 44});
        Event event;
        event.type = EventType::PointerMove;
        event.x = 20;
        event.y = 20;
        controls.dispatch(event);
        for (bool pressed : {false, true})
        {
            if (pressed)
            {
                event.type = EventType::PointerDown;
                event.button = PointerButton::Primary;
                controls.dispatch(event);
            }
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            canvas.beginFrame(256, 256);
            controls.paint(canvas, Viewport(256 / scale, 256 / scale, 256, 256));
            canvas.endFrame();
            const auto color = pressed ? theme.pressed : theme.hover;
            pixel(int(40 * scale), int(35 * scale), int(color.r * 255), int(color.g * 255), int(color.b * 255));
        }
        controls.setEnabled(button.id(), false);
        canvas.beginFrame(256, 256);
        controls.paint(canvas, Viewport(256 / scale, 256 / scale, 256, 256));
        canvas.endFrame();
        pixel(int(40 * scale), int(35 * scale), int(theme.disabled.r * 255), int(theme.disabled.g * 255),
              int(theme.disabled.b * 255));
        require(!controls.captured(), "disabled rendered control retained capture");

        // Pointer caret placement uses the same font/layout and logical coordinates as painting.
        event.type = EventType::PointerDown;
        event.x = 23;
        event.y = 85;
        controls.dispatch(event);
        event.type = EventType::PointerUp;
        controls.dispatch(event);
        event.type = EventType::TextInput;
        event.text = "X";
        controls.dispatch(event);
        require(field.text() == "XABCD", "field pointer placement diverged from rendered text");
    }
    // Scrolled geometry shares clipping and hit coordinates at fractional DPI.
    for (float scale : {1.0f, 1.25f, 2.0f})
    {
        Context ui;
        ScrollView view(ui);
        view.setContentSize({88, 300});
        Widget block;
        block.parent = view.content();
        block.bounds = {0, 80, 88, 80};
        block.painter = std::make_shared<Solid>(Color{0, 1, 0, 1});
        const auto item = ui.add(block);
        ui.layout(view.id(), {10, 10, 100, 80});
        view.setOffset({0, 60});
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.beginFrame(256, 256);
        ui.paint(canvas, Viewport(256 / scale, 256 / scale, 256, 256));
        canvas.endFrame();
        pixel(int(40 * scale), int(50 * scale), 0, 255, 0);
        pixel(int(40 * scale), int(95 * scale), 0, 0, 0);
        require(ui.hitTest({40, 50}) == item && ui.hitTest({40, 95}) != item,
                "scroll viewport painting and clipped picking diverged");
    }
    Context surfaces;
    Widget panel;
    panel.bounds = {0, 0, 180, 180};
    panel.painter = std::make_shared<SurfacePainter>(SurfaceRole::Panel);
    const auto panelId = surfaces.add(panel);
    Widget control;
    control.parent = panelId;
    control.bounds = {60, 60, 60, 40};
    control.focusable = true;
    control.painter = std::make_shared<SurfacePainter>();
    surfaces.add(control);
    const auto surface = surfaces.theme().surface, hover = surfaces.theme().hover;
    for (Vec2 point : {Vec2{30, 30}, Vec2{80, 80}, Vec2{30, 30}})
    {
        Event motion;
        motion.type = EventType::PointerMove;
        motion.x = point.x;
        motion.y = point.y;
        surfaces.dispatch(motion);
        canvas.beginFrame(256, 256);
        surfaces.paint(canvas, Viewport(256, 256, 256, 256));
        canvas.endFrame();
        pixel(30, 30, int(surface.r * 255), int(surface.g * 255), int(surface.b * 255));
        const auto expected = point.x == 80 ? hover : surface;
        pixel(80, 80, int(expected.r * 255), int(expected.g * 255), int(expected.b * 255));
    }
    // Layout placement and framebuffer scaling share the same hit/paint geometry.
    Context arranged;
    Widget row;
    row.layout = std::make_shared<RowLayout>(Padding{8, 8, 8, 8}, 4);
    row.clipChildren = true;
    const auto root = arranged.add(row);
    Widget cell;
    cell.parent = root;
    cell.layoutHints.flex = 1;
    cell.focusable = true;
    cell.painter = std::make_shared<Solid>(Color{0, 1, 0, 1});
    const auto green = arranged.add(cell);
    cell.painter = std::make_shared<Solid>(Color{0, 0, 1, 1});
    const auto blue = arranged.add(cell);
    for (float scale : {1.0f, 1.25f, 1.5f, 2.0f})
    {
        arranged.layout(root, {12, 16, 100, 50});
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.beginFrame(256, 256);
        arranged.paint(canvas, Viewport(256 / scale, 256 / scale, 256, 256));
        canvas.endFrame();
        require(arranged.hitTest({40, 40}) == green && arranged.hitTest({80, 40}) == blue,
                "arranged hit bounds diverged");
        pixel(int(40 * scale), int(40 * scale), 0, 255, 0);
        pixel(int(80 * scale), int(40 * scale), 0, 0, 255);
    }
    Context ui;
    Widget clip;
    clip.bounds = {20, 20, 50, 50};
    clip.clipChildren = true;
    clip.transform.m[3] = 0.5f;
    auto parent = ui.add(clip);
    Widget child;
    child.parent = parent;
    child.bounds = {0, 0, 128, 128};
    child.painter = std::make_shared<Solid>(Color{0, 1, 0, 1});
    const auto id = ui.add(child);
    for (auto scales : {Vec2{1, 1}, Vec2{1.25f, 1.25f}, Vec2{1.5f, 1.5f}, Vec2{2, 2}, Vec2{2, 1.25f}})
    {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.beginFrame(256, 256);
        ui.paint(canvas, Viewport(256 / scales.x, 256 / scales.y, 256, 256));
        canvas.endFrame();
        require(ui.hitTest({45, 30}) == id && !ui.hitTest({32, 65}), "sheared hit clipping failed");
        pixel(int(45 * scales.x), int(30 * scales.y), 0, 255, 0);
        pixel(int(32 * scales.x), int(65 * scales.y), 0, 0, 0);
    }
    const auto reflection = Mat3x3::translation(140, 0) * Mat3x3::scaling(-1, 1) * clip.transform;
    ui.setTransform(parent, reflection);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    canvas.beginFrame(256, 256);
    ui.paint(canvas, Viewport(256, 256, 256, 256));
    canvas.endFrame();
    require(ui.hitTest({95, 30}) == id && !ui.hitTest({108, 65}), "reflected clip hit failed");
    pixel(95, 30, 0, 255, 0);
    pixel(108, 65, 0, 0, 0);

    // Missing stencil produces a controlled failure and retains the caller's target.
    struct ColorTarget
    {
        GLuint framebuffer = 0, texture = 0;
        ColorTarget()
        {
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glGenFramebuffers(1, &framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        }
        ~ColorTarget()
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &framebuffer);
            glDeleteTextures(1, &texture);
        }
    };
    {
        ColorTarget target;
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                "test color framebuffer incomplete");
        canvas.beginFrame(256, 256);
        bool rejected = false;
        try
        {
            ui.paint(canvas, Viewport(256, 256, 256, 256));
        }
        catch (const std::runtime_error&)
        {
            rejected = true;
        }
        canvas.endFrame();
        GLint binding = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &binding);
        require(rejected && binding == static_cast<GLint>(target.framebuffer),
                "missing-stencil failure changed target");
        require(glGetError() == GL_NO_ERROR, "missing-stencil check produced an OpenGL error");
    }

    // Modal layer is painted last even when registered before its background.
    Context layers;
    Widget modal;
    modal.bounds = {10, 10, 80, 80};
    modal.painter = std::make_shared<Solid>(Color{1, 0, 0, 1});
    const auto modalId = layers.add(modal);
    Widget background = modal;
    background.painter = std::make_shared<Solid>(Color{0, 0, 1, 1});
    layers.add(background);
    layers.pushModal(modalId);
    canvas.beginFrame(256, 256);
    layers.paint(canvas, Viewport(256, 256, 256, 256));
    canvas.endFrame();
    pixel(40, 40, 255, 0, 0);
    require(layers.hitTest({40, 40}) == modalId, "modal hit order differed from paint order");
    struct Throwing final : WidgetPainter
    {
        Context& ui;
        WidgetId id;
        explicit Throwing(Context& context) : ui(context) {}
        void paint(PaintContext& p) override
        {
            p.canvas().translate(100, 0);
            p.canvas().setFillColor({1, 0, 0, 1});
            p.canvas().clipRect(0, 0, 1, 1);
            ui.remove(id);
            throw std::runtime_error("simulated painter failure");
        }
    };
    Context failure;
    auto throwing = std::make_shared<Throwing>(failure);
    Widget bad;
    bad.painter = throwing;
    throwing->id = failure.add(bad);
    canvas.beginFrame(256, 256);
    canvas.translate(10, 0);
    canvas.setFillColor({0, 0, 1, 1});
    bool caught = false;
    try
    {
        failure.paint(canvas, Viewport(256, 256, 256, 256));
    }
    catch (const std::runtime_error&)
    {
        caught = true;
    }
    require(caught && !failure.contains(throwing->id), "painter failure did not finalize removal");
    canvas.fillRect(0, 100, 20, 20);
    canvas.endFrame();
    pixel(15, 110, 0, 0, 255);
}
void adapterTests(GLFWwindow* window)
{
    for (auto metrics : {WindowMetrics(800, 600, 800, 600, 1.5f, 1.5f), WindowMetrics(800, 600, 1600, 1200, 2, 2),
                         WindowMetrics(800, 600, 1600, 900, 2, 1.5f)})
    {
        auto viewport = metrics.viewport();
        const auto point = metrics.logicalPoint(400, 300);
        require(viewport && point && std::abs(point->x - viewport->width() / 2) < 0.001f &&
                    std::abs(point->y - viewport->height() / 2) < 0.001f,
                "window/framebuffer/content-scale conversion failed");
    }
    require(!WindowMetrics(0, 0, 0, 0).viewport(), "minimized window remained drawable");
    Context ui;
    std::string text;
    int ups = 0, repeats = 0, cancelled = 0;
    Widget widget;
    widget.bounds = {-10000, -10000, 20000, 20000};
    widget.focusable = true;
    widget.onEvent = [&](Context&, WidgetId, const Event& e)
    {
        if (e.type == EventType::TextInput)
            text += e.text;
        if (e.type == EventType::KeyRepeat)
            ++repeats;
        if (e.type == EventType::KeyUp)
            ++ups;
        if (e.type == EventType::Cancel)
            ++cancelled;
        if (e.type == EventType::Wheel)
            require(e.wheelX == 2 && e.wheelY == -3, "GLFW lost wheel axis");
        return EventReply{true, false, e.type == EventType::PointerDown};
    };
    const auto id = ui.add(widget);
    ui.focus(id);
    int sentinel = 42;
    glfwSetWindowUserPointer(window, &sentinel);
    {
        GlfwAdapter adapter(window, ui);
        require(glfwGetWindowUserPointer(window) == &sentinel, "adapter replaced application user pointer");
        adapter.character(0x03a9);
        adapter.character(0x1f600);
        require(text == "\xce\xa9\xf0\x9f\x98\x80" && !adapter.character(0xd800), "GLFW UTF-8 translation failed");
        adapter.key(GLFW_KEY_DELETE, GLFW_REPEAT, 0);
        adapter.key(GLFW_KEY_DELETE, GLFW_RELEASE, 0);
        require(repeats == 1 && ups == 1, "key repeat/release translation failed");
        adapter.cursorPosition(10, 10);
        require(adapter.scroll(2, -3), "wheel event did not reach UI");
        // Mouse queries the actual native cursor, which may sit outside a hidden window.
        Event down;
        down.type = EventType::PointerDown;
        down.x = 10;
        down.y = 10;
        down.button = PointerButton::Primary;
        ui.dispatch(down);
        adapter.cursorEntered(false);
        require(!ui.captured() && cancelled == 1, "window leave did not cancel capture");
        ui.dispatch(down);
        adapter.focusChanged(false);
        require(!ui.captured() && !ui.focused() && cancelled == 2, "focus loss did not clear state");
        require(adapter.setCursor(Cursor::Text).status == ServiceStatus::Success, "cursor service failed");
        GlfwClipboard clipboard(window);
        require(clipboard.write(std::string_view("a\0b", 3)).status == ServiceStatus::Error,
                "NUL clipboard write was not rejected");
        // Do not read or change the system clipboard in automated tests.
    }
    require(glfwGetWindowUserPointer(window) == &sentinel, "adapter destruction changed user pointer");
    glfwSetWindowUserPointer(window, nullptr);
}
} // namespace
int main()
{
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    auto* window = glfwCreateWindow(256, 256, "UI paint regression", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    int result = 0;
    try
    {
        require(gladLoadGL(glfwGetProcAddress) != 0, "GLAD initialization failed");
        Canvas canvas;
        canvas.init();
        paintTests(canvas);
        adapterTests(window);
        require(glGetError() == GL_NO_ERROR, "UI painting generated an OpenGL error");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
