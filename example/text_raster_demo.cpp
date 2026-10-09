#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <vectorgl/canvas.hpp>
#include <vectorgl/glfw_text_box.hpp>

namespace
{
std::string utf8(std::u8string_view text)
{
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

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

struct App
{
    vectorgl::Canvas canvas;
    vectorgl::TextBox field;
    vectorgl::GlfwTextBoxController input;
    std::string fontPath;
    bool pixelSnap = true;
    vectorgl::TextRenderingMode fieldMode = vectorgl::TextRenderingMode::Auto;
    bool hinted = false;

    bool findFont(const std::string& requested)
    {
        if (!requested.empty())
        {
            fontPath = requested;
            return canvas.setFont(fontPath, 24);
        }
        for (const char* path : {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
                                 "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                 "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
                                 "/System/Library/Fonts/Supplemental/Arial.ttf"})
            if (canvas.setFont(path, 24))
            {
                fontPath = path;
                return true;
            }
        return false;
    }

    void label(const std::string& text, float x, float y, float size = 18)
    {
        (void)canvas.setFont(fontPath, size);
        canvas.setFillColor({0.8f, 0.86f, 0.95f, 1});
        canvas.fillText(text, x, y);
    }

    void draw(int width, int height, int fbWidth, int fbHeight, float dt)
    {
        using namespace vectorgl;
        canvas.beginFrame(fbWidth, fbHeight);
        canvas.scale(float(fbWidth) / width, float(fbHeight) / height);
        canvas.setTextPixelSnap(pixelSnap);
        label("Small text: SDF and hinted coverage", 40, 24, 30);
        label("F2: pixel snap | F3: input mode | Ctrl+A/C/X/V: edit | Esc: close", 40, 68, 16);
        label(hinted ? "FreeType light hinting enabled; layout metrics are identical in every mode."
                     : "FreeType disabled: bitmap requests fall back to SDF.",
              40, 94, 14);
        const float columnWidth = (width - 120.0f) / 3;
        int column = 0;
        for (auto mode : {TextRenderingMode::Sdf, TextRenderingMode::Bitmap, TextRenderingMode::Auto})
        {
            const float x = 40 + column * (columnWidth + 20);
            canvas.setFillColor({0.1f, 0.13f, 0.19f, 1});
            canvas.fillRoundedRect(x, 132, columnWidth, 546, 12);
            label(column == 0 ? "SDF" : column == 1 ? "Hinted bitmap" : "Auto (up to 24 device px)", x + 12, 144, 16);
            float y = 182;
            for (float size : {12.0f, 14.0f, 16.0f, 18.0f, 24.0f, 32.0f})
            {
                label(std::to_string(int(size)) + " px", x + 12, y + 4, 12);
                (void)canvas.setFont(fontPath, size);
                canvas.setTextRenderingMode(mode);
                canvas.setFillColor(Color::White);
                canvas.fillText(size >= 32 ? utf8(u8"AV To Caf\u00E9 \u03A9") : utf8(u8"AVATAR To Caf\u00E9 \u03A9"),
                                x + 66, y);
                y += 42;
            }
            canvas.setTextRenderingMode(TextRenderingMode::Auto);
            label("12 px at framebuffer scales:", x + 12, 450, 14);
            int row = 0;
            for (float scale : {1.0f, 1.5f, 2.0f})
            {
                label(scale == 1 ? "1x" : scale == 1.5f ? "1.5x" : "2x", x + 12, 484 + row * 52, 12);
                canvas.save();
                canvas.translate(x + 66, 480 + row * 52);
                canvas.scale(scale, scale);
                canvas.setTextRenderingMode(mode);
                (void)canvas.setFont(fontPath, 12);
                canvas.setFillColor(Color::White);
                canvas.fillText(utf8(u8"AV To Caf\u00E9 \u03A9"), 0, 0);
                canvas.restore();
                ++row;
            }
            ++column;
        }
        canvas.setTextRenderingMode(TextRenderingMode::Auto);
        const char* fieldName = fieldMode == TextRenderingMode::Auto  ? "Auto"
                                : fieldMode == TextRenderingMode::Sdf ? "SDF"
                                                                      : "Bitmap";
        label(std::string("16 px Unicode input - ") + fieldName, 40, 698, 16);
        field.setBounds(40, 730, width - 80.0f, 54);
        canvas.setTextRenderingMode(fieldMode);
        (void)field.render(canvas, dt);
        canvas.setTextRenderingMode(TextRenderingMode::Auto);
        label("Rotated/nonuniform text uses SDF. Bitmap output is grayscale, not LCD/ClearType.", 40, 806, 14);
        canvas.endFrame();
    }
};
} // namespace

int main(int argc, char** argv)
{
    bool smoke = false;
    std::string requestedFont;
    std::string screenshotPath;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--smoke-test")
            smoke = true;
        else if (arg == "--font" && i + 1 < argc)
            requestedFont = argv[++i];
        else if (arg == "--screenshot" && i + 1 < argc)
            screenshotPath = argv[++i];
    }
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (smoke)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    auto* window = glfwCreateWindow(1120, 840, "VectorGL - Small text rendering", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwSetWindowSizeLimits(window, 1000, 840, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    if (!gladLoadGL(glfwGetProcAddress))
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glfwSwapInterval(smoke ? 0 : 1);
    int result = 0;
    try
    {
        App app;
        app.canvas.init();
        if (!app.findFont(requestedFont))
            throw std::runtime_error("No font loaded; run with --font path/to/font.ttf");
        app.field.setFont(app.fontPath, 16);
        vectorgl::Font probe;
        if (probe.load(app.fontPath, 16))
            app.hinted = probe.hasBitmapSupport();
        app.field.setText(utf8(u8"Caf\u00E9 | \u03A9 | \u041F\u0440\u0438\u0432\u0435\u0442"));
        app.field.setFocused(true);
        app.input.bind(app.field, app.canvas, window);
        glfwSetWindowUserPointer(window, &app);
        glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int cp)
                            { static_cast<App*>(glfwGetWindowUserPointer(w))->input.handleChar(cp); });
        glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y)
                                 { static_cast<App*>(glfwGetWindowUserPointer(w))->input.handleCursorPos(x, y); });
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow* w, int button, int action, int)
                                   {
                                       auto& input = static_cast<App*>(glfwGetWindowUserPointer(w))->input;
                                       double x, y;
                                       glfwGetCursorPos(w, &x, &y);
                                       input.handleCursorPos(x, y);
                                       input.handleMouseButton(button, action);
                                   });
        glfwSetKeyCallback(window,
                           [](GLFWwindow* w, int key, int, int action, int mods)
                           {
                               auto& app = *static_cast<App*>(glfwGetWindowUserPointer(w));
                               if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
                                   glfwSetWindowShouldClose(w, GLFW_TRUE);
                               if (key == GLFW_KEY_F2 && action == GLFW_PRESS)
                                   app.pixelSnap = !app.pixelSnap;
                               if (key == GLFW_KEY_F3 && action == GLFW_PRESS)
                                   app.fieldMode = app.fieldMode == vectorgl::TextRenderingMode::Auto
                                                       ? vectorgl::TextRenderingMode::Sdf
                                                   : app.fieldMode == vectorgl::TextRenderingMode::Sdf
                                                       ? vectorgl::TextRenderingMode::Bitmap
                                                       : vectorgl::TextRenderingMode::Auto;
                               app.input.handleKey(key, action, mods);
                           });
        if (smoke)
        {
            app.field.selectAll();
            app.field.pasteText(utf8(u8"A\u03A9\U0001F600"));
            app.field.handleKey(vectorgl::TextBoxKey::Backspace);
            if (app.field.text() != utf8(u8"A\u03A9"))
                result = 1;
        }
        int frames = 0;
        double previous = glfwGetTime();
        while (!glfwWindowShouldClose(window) && (!smoke || frames < 3))
        {
            glfwPollEvents();
            int width, height, fbWidth, fbHeight;
            glfwGetWindowSize(window, &width, &height);
            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            const double now = glfwGetTime();
            const float dt = float(now - previous);
            previous = now;
            if (width <= 0 || height <= 0 || fbWidth <= 0 || fbHeight <= 0)
                continue;
            glClearColor(0.04f, 0.06f, 0.1f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            app.draw(width, height, fbWidth, fbHeight, dt);
            if (frames == 0 && !screenshotPath.empty())
                screenshot(screenshotPath, fbWidth, fbHeight);
            glfwSwapBuffers(window);
            ++frames;
        }
        if (glGetError() != GL_NO_ERROR)
            result = 1;
        app.canvas.destroy();
        glfwSetWindowUserPointer(window, nullptr);
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
