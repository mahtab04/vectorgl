#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <vectorgl/canvas.hpp>
#include <vectorgl/scene.hpp>

using namespace vectorgl;

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

Path2D sample(int row)
{
    Path2D path;
    if (row == 0)
    {
        path.moveTo(55, 210);
        path.lineTo(220, 210);
        path.lineTo(220, 170);
        path.lineTo(485, 170);
    }
    else if (row == 1)
    {
        path.moveTo(55, 355);
        path.bezierCurveTo(180, 280, 330, 420, 485, 330);
    }
    else
    {
        path.moveTo(55, 525);
        path.lineTo(185, 525);
        path.lineTo(185, 600);
        path.lineTo(485, 600);
    }
    return path;
}
} // namespace

int main(int argc, char** argv)
{
    bool smoke = false;
    std::string font, screenshotPath;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--smoke-test")
            smoke = true;
        else if (arg == "--font" && i + 1 < argc)
            font = argv[++i];
        else if (arg == "--screenshot" && i + 1 < argc)
            screenshotPath = argv[++i];
        else
        {
            std::cerr << "Usage: vectorgl_dashes [--font font.ttf] [--smoke-test] [--screenshot preview.bmp]\n";
            return 1;
        }
    }
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 0); // Path stroking needs no stencil buffer.
    if (smoke)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    auto* window = glfwCreateWindow(
        1100, 760, "VectorGL dashed strokes | Space: pause | Left/Right: offset | Up/Down: speed", nullptr, nullptr);
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
        Scene scene(canvas.renderer());
        std::vector<std::shared_ptr<Node>> nodes;
        for (int row = 0; row < 3; ++row)
            nodes.push_back(scene.path(sample(row)));
        Path2D selection;
        selection.rect(610, 240, 400, 260);
        nodes.push_back(scene.path(selection));
        nodes.push_back(scene.path(selection));
        std::vector<StrokeStyle> styles(nodes.size());
        styles[0].dashArray = {16, 10};
        styles[1].dashArray = {0, 20};
        styles[1].cap = LineCap::Round;
        styles[2].dashArray = {24, 8, 4};
        styles[2].cap = LineCap::Square;
        styles[3].dashArray = styles[4].dashArray = {10, 10};
        float phase = 0, manualOffset = 0, speed = 30;
        bool animated = true;
        bool previousSpace = false, previousUp = false, previousDown = false, previousLeft = false,
             previousRight = false;
        double previousTime = glfwGetTime();
        int frames = 0;
        while (!glfwWindowShouldClose(window) && (!smoke || frames < 3))
        {
            glfwPollEvents();
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                break;
            auto pressed = [&](int key, bool& previous)
            {
                const bool down = glfwGetKey(window, key) == GLFW_PRESS;
                const bool result = down && !previous;
                previous = down;
                return result;
            };
            if (pressed(GLFW_KEY_SPACE, previousSpace))
                animated = !animated;
            if (pressed(GLFW_KEY_UP, previousUp))
                speed = std::min(120.0f, speed + 10);
            if (pressed(GLFW_KEY_DOWN, previousDown))
                speed = std::max(0.0f, speed - 10);
            if (pressed(GLFW_KEY_RIGHT, previousRight))
                manualOffset += 2;
            if (pressed(GLFW_KEY_LEFT, previousLeft))
                manualOffset -= 2;
            const double now = glfwGetTime();
            if (animated)
                phase = std::fmod(phase + static_cast<float>(std::min(now - previousTime, 0.1)) * speed, 20.0f);
            previousTime = now;
            int width, height, windowWidth, windowHeight;
            glfwGetFramebufferSize(window, &width, &height);
            glfwGetWindowSize(window, &windowWidth, &windowHeight);
            if (width <= 0 || height <= 0 || windowWidth <= 0 || windowHeight <= 0)
                continue;
            const float sx = width / 1100.0f, sy = height / 760.0f;
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            const float mouseX = static_cast<float>(mx) * width / windowWidth;
            const float mouseY = static_cast<float>(my) * height / windowHeight;
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
            label("Dashed strokes: connectors, dots and animated selections", 28, 18);
            label("Space: pause | Left/Right: dash offset | Up/Down: speed | Esc: close", 28, 48);
            canvas.setFillColor({0.075f, 0.1f, 0.15f, 1});
            canvas.fillRoundedRect(20, 100, 510, 600, 18);
            canvas.fillRoundedRect(550, 100, 530, 600, 18);
            label("Dashed connector / butt caps / {16, 10}", 40, 120);
            label("Dotted curve / round caps / {0, 20}", 40, 280);
            label("Odd pattern / square caps / {24, 8, 4}", 40, 465);
            label("Animated selection outline", 575, 120);
            label(animated ? "Marching ants: running" : "Marching ants: paused", 575, 152);
            canvas.setFillColor({0.12f, 0.19f, 0.3f, 1});
            canvas.fillRoundedRect(620, 250, 380, 240, 12);
            label("Selected object", 735, 335);
            label("Two dash phases, one shared path", 660, 370);
            label("Dash offset: " + std::to_string(manualOffset).substr(0, 6), 575, 555);
            label("Animation speed: " + std::to_string(static_cast<int>(speed)), 575, 585);
            for (size_t i = 0; i < nodes.size(); ++i)
            {
                auto& node = nodes[i];
                node->setScale(sx, sy);
                auto& style = styles[i];
                style.dashOffset = i < 3 ? manualOffset : manualOffset + phase + (i == 4 ? 10.0f : 0.0f);
                node->setStrokeStyle(style);
                node->setStroke(i < 3 ? Color{0.25f, 0.7f, 1, 0.85f}
                                      : (i == 3 ? Color::White : Color{0.02f, 0.03f, 0.05f, 1}),
                                i < 3 ? 8.0f : 4.0f);
            }
            canvas.restore();
            const auto hovered = scene.pick(mouseX, mouseY);
            scene.render();
            canvas.save();
            canvas.scale(sx, sy);
            label(hovered ? "Painted dash hit: picking uses the rendered mesh" : "Hover a dash or gap to check picking",
                  28, 720);
            canvas.restore();
            canvas.endFrame();
            if (frames == 0 && !screenshotPath.empty())
                screenshot(screenshotPath, width, height);
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
