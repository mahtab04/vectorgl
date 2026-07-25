#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <cmath>
#include <iostream>
#include <string>
#include <vectorgl/canvas.hpp>

namespace
{

constexpr int kWindowWidth = 1180;
constexpr int kWindowHeight = 680;

using vectorgl::Canvas;
using vectorgl::Color;

bool loadFont(Canvas& canvas)
{
    const char* paths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/SFNS.ttf",
    };
    for (const char* path : paths)
    {
        if (canvas.setFont(path, 17.0f))
            return true;
    }
    return false;
}

void drawPanel(Canvas& canvas, float x, float y, float width, float height, const char* title, bool hasFont)
{
    canvas.setFillColor(Color::hex(0x111A2E));
    canvas.fillRoundedRect(x, y, width, height, 18.0f);
    canvas.setStrokeColor(Color::hex(0x293A60));
    canvas.setLineWidth(1.5f);
    canvas.strokeRoundedRect(x, y, width, height, 18.0f);
    if (hasFont)
    {
        canvas.setFillColor(Color::hex(0xDCE8FF));
        canvas.fillText(title, x + 20.0f, y + 16.0f);
    }
}

void drawOverflowGallery(Canvas& canvas, float time, bool hasFont)
{
    constexpr float x = 40.0f;
    constexpr float y = 150.0f;
    constexpr float width = 340.0f;
    constexpr float height = 210.0f;
    drawPanel(canvas, x, y, width, height, "Overflow hidden", hasFont);

    canvas.save();
    canvas.clipRoundedRect(x + 14.0f, y + 54.0f, width - 28.0f, height - 70.0f, 14.0f);
    for (int index = -2; index < 9; ++index)
    {
        const float phase = static_cast<float>(index) * 0.65f + time;
        const float centerX = x + 30.0f + static_cast<float>(index) * 52.0f + std::sin(time) * 30.0f;
        const float centerY = y + 125.0f + std::sin(phase) * 42.0f;
        const float amount = static_cast<float>(index + 2) / 10.0f;
        canvas.setFillColor(Color::lerpOklab(Color::hex(0x22D3EE), Color::hex(0xA855F7), amount));
        canvas.fillCircle(centerX, centerY, 26.0f);
    }
    canvas.restore();
}

void drawNestedClip(Canvas& canvas, float time, bool hasFont)
{
    constexpr float x = 420.0f;
    constexpr float y = 150.0f;
    constexpr float width = 340.0f;
    constexpr float height = 210.0f;
    drawPanel(canvas, x, y, width, height, "Nested intersections", hasFont);

    canvas.save();
    canvas.clipRoundedRect(x + 18.0f, y + 54.0f, width - 36.0f, height - 72.0f, 16.0f);
    for (int stripe = 0; stripe < 7; ++stripe)
    {
        const float amount = static_cast<float>(stripe) / 6.0f;
        canvas.setFillColor(Color::lerpOklab(Color::hex(0x0EA5E9), Color::hex(0xEC4899), amount));
        canvas.fillRect(x + 18.0f + static_cast<float>(stripe) * 48.0f, y + 54.0f, 48.0f, height - 72.0f);
    }

    canvas.save();
    const float inset = 30.0f + 10.0f * std::sin(time * 1.4f);
    canvas.clipRoundedRect(x + 80.0f + inset, y + 78.0f, 130.0f, 88.0f, 22.0f);
    canvas.setFillColor(Color::hex(0xF8FAFC, 0.92f));
    for (int row = 0; row < 5; ++row)
    {
        for (int column = 0; column < 8; ++column)
        {
            canvas.fillCircle(x + 65.0f + static_cast<float>(column) * 36.0f,
                              y + 70.0f + static_cast<float>(row) * 31.0f, 9.0f);
        }
    }
    canvas.restore();
    canvas.restore();
}

void drawScrollingFeed(Canvas& canvas, float time, bool hasFont)
{
    constexpr float x = 800.0f;
    constexpr float y = 150.0f;
    constexpr float width = 340.0f;
    constexpr float height = 450.0f;
    drawPanel(canvas, x, y, width, height, "Scrollable content", hasFont);

    canvas.save();
    canvas.clipRoundedRect(x + 14.0f, y + 54.0f, width - 28.0f, height - 72.0f, 14.0f);
    const float scroll = std::fmod(time * 42.0f, 104.0f);
    for (int index = -1; index < 7; ++index)
    {
        const float cardY = y + 62.0f + static_cast<float>(index) * 104.0f - scroll;
        const Color accent =
            Color::lerpOklab(Color::hex(0x34D399), Color::hex(0x60A5FA), static_cast<float>(index + 1) / 7.0f);
        canvas.setFillColor(Color::hex(0x18243D));
        canvas.fillRoundedRect(x + 18.0f, cardY, width - 36.0f, 86.0f, 13.0f);
        canvas.setFillColor(accent);
        canvas.fillRoundedRect(x + 30.0f, cardY + 14.0f, 58.0f, 58.0f, 12.0f);
        canvas.setFillColor(Color::hex(0x31415F));
        canvas.fillRoundedRect(x + 104.0f, cardY + 19.0f, 170.0f, 12.0f, 6.0f);
        canvas.fillRoundedRect(x + 104.0f, cardY + 43.0f, 126.0f, 10.0f, 5.0f);
    }
    canvas.restore();
}

void drawClipResetDemo(Canvas& canvas, bool hasFont)
{
    constexpr float x = 40.0f;
    constexpr float y = 400.0f;
    constexpr float width = 720.0f;
    constexpr float height = 200.0f;
    drawPanel(canvas, x, y, width, height, "Save / restore clip state", hasFont);

    canvas.save();
    canvas.clipRect(x + 20.0f, y + 58.0f, 300.0f, 110.0f);
    canvas.setFillColor(Color::hex(0xF97316));
    canvas.fillCircle(x + 170.0f, y + 113.0f, 112.0f);

    canvas.save();
    canvas.clipRect(x + 170.0f, y + 58.0f, 150.0f, 110.0f);
    canvas.setFillColor(Color::hex(0xFDE047));
    canvas.fillCircle(x + 245.0f, y + 113.0f, 82.0f);
    canvas.restore();
    canvas.restore();

    canvas.setStrokeColor(Color::hex(0x4CC9F0));
    canvas.setLineWidth(2.0f);
    canvas.strokeRoundedRect(x + 370.0f, y + 68.0f, 300.0f, 90.0f, 16.0f);
    if (hasFont)
    {
        canvas.setFillColor(Color::hex(0x93A4C2));
        canvas.fillText("Drawing continues normally", x + 400.0f, y + 93.0f);
        canvas.fillText("after restore() removes the clip.", x + 400.0f, y + 125.0f);
    }
}

} // namespace

int main()
{
    if (glfwInit() != GLFW_TRUE)
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "VectorGL - Clipping", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (gladLoadGL(glfwGetProcAddress) == 0)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    Canvas canvas;
    canvas.init();
    const bool hasFont = loadFont(canvas);

    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        glClearColor(0.025f, 0.035f, 0.065f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        canvas.beginFrame(kWindowWidth, kWindowHeight);
        if (hasFont)
        {
            canvas.setFillColor(Color::hex(0xF1F5FF));
            canvas.fillText("NESTED CLIPPING", 40.0f, 48.0f);
            canvas.setFillColor(Color::hex(0x8292AF));
            canvas.fillText("Rectangular and rounded masks, restored with Canvas state", 40.0f, 85.0f);
        }

        const float time = static_cast<float>(glfwGetTime());
        drawOverflowGallery(canvas, time, hasFont);
        drawNestedClip(canvas, time, hasFont);
        drawScrollingFeed(canvas, time, hasFont);
        drawClipResetDemo(canvas, hasFont);
        canvas.endFrame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    canvas.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
