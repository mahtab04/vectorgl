#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <cmath>
#include <exception>
#include <iostream>
#include <numbers>
#include <string>
#include <vectorgl/canvas.hpp>
#include <vectorgl/paint.hpp>
#include <vectorgl/scene.hpp>

using namespace vectorgl;

namespace
{
Path2D sample(int row, bool reverse)
{
    Path2D path;
    if (row == 0)
    {
        path.arc(0, 0, 64, 0, 2 * std::numbers::pi_v<float>);
        path.closePath();
        path.moveTo(30, 0);
        path.arc(0, 0, 30, 0, (reverse ? -2 : 2) * std::numbers::pi_v<float>, reverse);
        path.closePath();
    }
    else if (row == 1)
    {
        path.rect(-75, -50, 100, 100);
        path.rect(-25, -35, 100, 100);
    }
    else
    {
        for (int i = 0; i < 5; ++i)
        {
            const float angle = -std::numbers::pi_v<float> / 2 + i * 4 * std::numbers::pi_v<float> / 5;
            const float x = std::cos(angle) * 68, y = std::sin(angle) * 68;
            if (i == 0)
                path.moveTo(x, y);
            else
                path.lineTo(x, y);
        }
        path.closePath();
    }
    return path;
}

void emit(Canvas& canvas, const Path2D& path)
{
    canvas.beginPath();
    for (const auto& point : path.points())
        switch (point.cmd)
        {
        case PathCmd::MoveTo:
            canvas.moveTo(point.pos.x, point.pos.y);
            break;
        case PathCmd::LineTo:
            canvas.lineTo(point.pos.x, point.pos.y);
            break;
        case PathCmd::Close:
            canvas.closePath();
            break;
        }
}
} // namespace

int main(int argc, char** argv)
{
    bool smoke = false;
    std::string font;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--smoke-test")
            smoke = true;
        else if (arg == "--font" && i + 1 < argc)
            font = argv[++i];
        else
        {
            std::cerr << "Usage: vectorgl_fill_rules [--font font.ttf] [--smoke-test]\n";
            return 1;
        }
    }
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 0); // Path filling needs no stencil buffer.
    if (smoke)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    auto* window =
        glfwCreateWindow(1000, 800, "VectorGL fill rules | Space: reverse inner ring | Esc: close", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)))
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glfwSwapInterval(1);
    int result = 0;
    try
    {
        Canvas canvas;
        canvas.init();
        bool hasFont = !font.empty() && canvas.setFont(font, 18);
        if (font.empty())
            for (const char* candidate :
                 {"C:/Windows/Fonts/segoeui.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                  "/System/Library/Fonts/Supplemental/Arial.ttf"})
                if ((hasFont = canvas.setFont(candidate, 18)))
                    break;
        if (!hasFont)
            std::cerr << "No font found; pass --font font.ttf to show labels.\n";
        bool reverse = false, wasSpace = false;
        int frames = 0;
        while (!glfwWindowShouldClose(window) && (!smoke || frames < 3))
        {
            glfwPollEvents();
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                break;
            const bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            if (space && !wasSpace)
                reverse = !reverse;
            wasSpace = space;
            int width, height, windowWidth, windowHeight;
            glfwGetFramebufferSize(window, &width, &height);
            glfwGetWindowSize(window, &windowWidth, &windowHeight);
            if (width <= 0 || height <= 0 || windowWidth <= 0 || windowHeight <= 0)
                continue;
            const float sx = width / 1000.0f, sy = height / 800.0f;
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            const float mouseX = static_cast<float>(mx) * 1000 / windowWidth;
            const float mouseY = static_cast<float>(my) * 800 / windowHeight;
            glClearColor(0.035f, 0.045f, 0.07f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            canvas.beginFrame(width, height);
            canvas.save();
            canvas.scale(sx, sy);
            auto label = [&](const std::string& text, float x, float y)
            {
                if (hasFont)
                {
                    canvas.setFillColor({0.85f, 0.9f, 1, 1});
                    canvas.fillText(text, x, y);
                }
            };
            label("Compound paths: holes, overlaps and self-intersections", 32, 24);
            label("Space: reverse inner ring | Hover shapes to test filled areas | Esc: close", 32, 54);
            label("NonZero (default)", 175, 100);
            label("EvenOdd", 675, 100);
            const char* names[] = {reverse ? "Ring: opposite directions" : "Ring: same direction",
                                   "Overlapping contours at 65% opacity", "Self-intersecting star"};
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 2; ++column)
                {
                    const float x = column * 500.0f, y = 140 + row * 210.0f;
                    canvas.setFillColor({0.075f, 0.1f, 0.15f, 1});
                    canvas.fillRoundedRect(x + 24, y, 452, 194, 18);
                    label(names[row], x + 42, y + 12);
                    auto path = sample(row, reverse);
                    const auto rule = column == 0 ? FillRule::NonZero : FillRule::EvenOdd;
                    const bool hovered = path.contains(mouseX - x - 250, mouseY - y - 115, rule);
                    canvas.save();
                    canvas.translate(x + 250, y + 115);
                    emit(canvas, path);
                    canvas.fillWithPaint(
                        Paint::linearGradient({-75, 0}, {75, 0}, {{0, {0.15f, 0.65f, 1, 1}}, {1, {0.7f, 0.4f, 1, 1}}}),
                        row == 1 ? 0.65f : 1.0f, rule);
                    if (hovered)
                    {
                        canvas.setStrokeColor(Color::White);
                        canvas.setLineWidth(1.5f);
                        canvas.stroke();
                    }
                    canvas.restore();
                    if (hovered)
                        label("Inside filled area", x + 42, y + 164);
                }
            canvas.restore();
            canvas.endFrame();
            if (glGetError() != GL_NO_ERROR)
                result = 1;
            glfwSwapBuffers(window);
            ++frames;
        }
        canvas.destroy();
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
