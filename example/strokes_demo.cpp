#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
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

Path2D joinPath()
{
    Path2D path;
    path.moveTo(70, 300);
    path.lineTo(165, 195);
    path.lineTo(260, 300);
    return path;
}
Path2D capPath()
{
    Path2D path;
    path.moveTo(70, 465);
    path.lineTo(260, 465);
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
            std::cerr << "Usage: vectorgl_strokes [--font font.ttf] [--smoke-test] [--screenshot preview.bmp]\n";
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
        1000, 680, "VectorGL strokes | Up/Down: width | Left/Right: miter limit | Esc: close", nullptr, nullptr);
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
        const LineJoin joins[] = {LineJoin::Miter, LineJoin::Round, LineJoin::Bevel};
        const LineCap caps[] = {LineCap::Butt, LineCap::Round, LineCap::Square};
        std::vector<std::shared_ptr<Node>> nodes;
        for (int column = 0; column < 3; ++column)
        {
            nodes.push_back(scene.path(joinPath()));
            nodes.push_back(scene.path(capPath()));
        }
        float lineWidth = 32, limit = 4;
        bool previousUp = false, previousDown = false, previousLeft = false, previousRight = false;
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
            if (pressed(GLFW_KEY_UP, previousUp))
                lineWidth = std::min(60.0f, lineWidth + 4);
            if (pressed(GLFW_KEY_DOWN, previousDown))
                lineWidth = std::max(4.0f, lineWidth - 4);
            if (pressed(GLFW_KEY_RIGHT, previousRight))
                limit = std::min(10.0f, limit + 0.5f);
            if (pressed(GLFW_KEY_LEFT, previousLeft))
                limit = std::max(1.0f, limit - 0.5f);
            int width, height, windowWidth, windowHeight;
            glfwGetFramebufferSize(window, &width, &height);
            glfwGetWindowSize(window, &windowWidth, &windowHeight);
            if (width <= 0 || height <= 0 || windowWidth <= 0 || windowHeight <= 0)
                continue;
            const float sx = width / 1000.0f, sy = height / 680.0f;
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
            label("Path strokes: joins and caps", 28, 18);
            label("Up/Down: width " + std::to_string(static_cast<int>(lineWidth)) + " | Left/Right: miter limit " +
                      std::to_string(limit).substr(0, 3),
                  28, 48);
            const char* joinNames[] = {"Miter", "Round", "Bevel"};
            const char* capNames[] = {"Butt", "Round", "Square"};
            for (int column = 0; column < 3; ++column)
            {
                const float x = column * 333.0f;
                canvas.setFillColor({0.075f, 0.1f, 0.15f, 1});
                canvas.fillRoundedRect(x + 16, 84, 300, 264, 16);
                canvas.fillRoundedRect(x + 16, 365, 300, 180, 16);
                label(std::string(joinNames[column]) + " join", x + 34, 96);
                label(std::string(capNames[column]) + " cap", x + 34, 383);
                for (int row = 0; row < 2; ++row)
                {
                    auto& node = nodes[column * 2 + row];
                    node->setFill(Color::Transparent);
                    node->setPosition(x * sx, 0);
                    node->setScale(sx, sy);
                    node->setStroke({0.2f, 0.65f, 1, 0.7f}, lineWidth);
                    node->setStrokeStyle(
                        {row == 0 ? joins[column] : LineJoin::Miter, row == 0 ? LineCap::Butt : caps[column], limit});
                }
            }
            canvas.restore();
            const auto hovered = scene.pick(mouseX, mouseY);
            if (hovered)
                hovered->setStroke({0.6f, 0.8f, 1, 1}, lineWidth);
            scene.render();
            canvas.save();
            canvas.scale(sx, sy);
            label(hovered ? "Inside the stroke: picking matches the rendered mesh"
                          : "Hover a join, cap or empty region to test picking",
                  28, 578);
            label("Translucent segments and joins are merged before drawing. Esc: close", 28, 610);
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
