#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <vectorgl/canvas.hpp>
using namespace vectorgl;
namespace
{
struct Named
{
    const char* name;
    Color value;
};
constexpr Named palette[] = {
    {"Black", Color::Black},
    {"White", Color::White},
    {"Red", Color::Red},
    {"Green", Color::Green},
    {"Blue", Color::Blue},
    {"Yellow", Color::Yellow},
    {"Cyan", Color::Cyan},
    {"Magenta", Color::Magenta},
    {"Transparent", Color::Transparent},
    {"Gray", Color::Gray},
    {"DarkGray", Color::DarkGray},
    {"LightGray", Color::LightGray},
    {"Orange", Color::Orange},
    {"Aqua", Color::Aqua},
    {"Fuchsia", Color::Fuchsia},
    {"Maroon", Color::Maroon},
    {"Grey", Color::Grey},
    {"DarkGrey", Color::DarkGrey},
    {"LightGrey", Color::LightGrey},
    {"DarkRed", Color::DarkRed},
    {"Crimson", Color::Crimson},
    {"Firebrick", Color::Firebrick},
    {"IndianRed", Color::IndianRed},
    {"Salmon", Color::Salmon},
    {"LightSalmon", Color::LightSalmon},
    {"Coral", Color::Coral},
    {"Tomato", Color::Tomato},
    {"HotPink", Color::HotPink},
    {"DeepPink", Color::DeepPink},
    {"LightPink", Color::LightPink},
    {"Pink", Color::Pink},
    {"DarkOrange", Color::DarkOrange},
    {"OrangeRed", Color::OrangeRed},
    {"Gold", Color::Gold},
    {"Goldenrod", Color::Goldenrod},
    {"Khaki", Color::Khaki},
    {"LightYellow", Color::LightYellow},
    {"LemonChiffon", Color::LemonChiffon},
    {"Amber", Color::Amber},
    {"DarkGreen", Color::DarkGreen},
    {"ForestGreen", Color::ForestGreen},
    {"Lime", Color::Lime},
    {"LimeGreen", Color::LimeGreen},
    {"LightGreen", Color::LightGreen},
    {"SeaGreen", Color::SeaGreen},
    {"MediumSeaGreen", Color::MediumSeaGreen},
    {"SpringGreen", Color::SpringGreen},
    {"Olive", Color::Olive},
    {"OliveDrab", Color::OliveDrab},
    {"YellowGreen", Color::YellowGreen},
    {"Chartreuse", Color::Chartreuse},
    {"Teal", Color::Teal},
    {"MintGreen", Color::MintGreen},
    {"Emerald", Color::Emerald},
    {"Navy", Color::Navy},
    {"DarkBlue", Color::DarkBlue},
    {"MediumBlue", Color::MediumBlue},
    {"RoyalBlue", Color::RoyalBlue},
    {"DodgerBlue", Color::DodgerBlue},
    {"DeepSkyBlue", Color::DeepSkyBlue},
    {"SkyBlue", Color::SkyBlue},
    {"LightSkyBlue", Color::LightSkyBlue},
    {"LightBlue", Color::LightBlue},
    {"SteelBlue", Color::SteelBlue},
    {"LightSteelBlue", Color::LightSteelBlue},
    {"CornflowerBlue", Color::CornflowerBlue},
    {"MidnightBlue", Color::MidnightBlue},
    {"Turquoise", Color::Turquoise},
    {"DarkTurquoise", Color::DarkTurquoise},
    {"Aquamarine", Color::Aquamarine},
    {"Indigo", Color::Indigo},
    {"Purple", Color::Purple},
    {"DarkViolet", Color::DarkViolet},
    {"BlueViolet", Color::BlueViolet},
    {"Violet", Color::Violet},
    {"Orchid", Color::Orchid},
    {"MediumOrchid", Color::MediumOrchid},
    {"Plum", Color::Plum},
    {"Lavender", Color::Lavender},
    {"RebeccaPurple", Color::RebeccaPurple},
    {"SlateBlue", Color::SlateBlue},
    {"MediumSlateBlue", Color::MediumSlateBlue},
    {"Brown", Color::Brown},
    {"SaddleBrown", Color::SaddleBrown},
    {"Sienna", Color::Sienna},
    {"Chocolate", Color::Chocolate},
    {"Peru", Color::Peru},
    {"Tan", Color::Tan},
    {"SandyBrown", Color::SandyBrown},
    {"BurlyWood", Color::BurlyWood},
    {"Wheat", Color::Wheat},
    {"Beige", Color::Beige},
    {"Ivory", Color::Ivory},
    {"DimGray", Color::DimGray},
    {"SlateGray", Color::SlateGray},
    {"LightSlateGray", Color::LightSlateGray},
    {"Gainsboro", Color::Gainsboro},
    {"Silver", Color::Silver},
    {"Snow", Color::Snow},
    {"WhiteSmoke", Color::WhiteSmoke},
    {"GhostWhite", Color::GhostWhite},
    {"AliceBlue", Color::AliceBlue},
    {"AntiqueWhite", Color::AntiqueWhite},
    {"Linen", Color::Linen},
    {"Primary", Color::Primary},
    {"Secondary", Color::Secondary},
    {"Success", Color::Success},
    {"Warning", Color::Warning},
    {"Error", Color::Error},
    {"Info", Color::Info},
    {"Background", Color::Background},
    {"Surface", Color::Surface},
    {"Border", Color::Border},
    {"TextPrimary", Color::TextPrimary},
    {"TextSecondary", Color::TextSecondary},
};
static_assert(
    []
    {
        for (auto entry : palette)
        {
            auto c = entry.value;
            if (!(c.r >= 0 && c.r <= 1 && c.g >= 0 && c.g <= 1 && c.b >= 0 && c.b <= 1 && c.a >= 0 && c.a <= 1))
                return false;
        }
        return true;
    }());
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
    Canvas& canvas;
    std::string font;
    std::size_t page = 0, pages = 1;
    bool dirty = true;
    void text(const std::string& value, float x, float y, float size = 16, Color color = Color::TextPrimary)
    {
        if (!canvas.setFont(font, size))
            throw std::runtime_error("Could not load palette font");
        canvas.setFillColor(color);
        canvas.fillText(value, x, y);
    }
    void rect(float x, float y, float width, float height)
    {
        canvas.beginPath();
        canvas.moveTo(x, y);
        canvas.lineTo(x + width, y);
        canvas.lineTo(x + width, y + height);
        canvas.lineTo(x, y + height);
        canvas.closePath();
        canvas.fill();
    }
    void draw(float width, float height)
    {
        text("VectorGL / color palette", 24, 20, 28);
        const int columns = std::clamp(int((width - 48) / 180), 1, 8);
        const int rows = std::clamp(int((height - 320) / 88), 1, 5);
        const std::size_t count = std::size_t(columns * rows);
        pages = (std::size(palette) + count - 1) / count;
        page %= pages;
        text(std::to_string(std::size(palette)) + " names | Page " + std::to_string(page + 1) + " / " +
                 std::to_string(pages) + " | Left / Right: browse | Esc: close",
             24, 64, 16, Color::TextSecondary);
        const float cell = std::max(1.0f, (width - 48 - float(columns - 1) * 14) / float(columns));
        for (std::size_t n = 0; n < count && page * count + n < std::size(palette); ++n)
        {
            const auto& item = palette[page * count + n];
            const float x = 24 + float(n % columns) * (cell + 14), y = 110 + float(n / columns) * 88;
            canvas.save();
            canvas.clipRect(x, y, cell, 80);
            // Checkerboard makes Color::Transparent and straight alpha visible.
            if (item.value.a < 1)
                for (int iy = 0; iy < 4; ++iy)
                    for (int ix = 0; float(ix * 10) < cell; ++ix)
                    {
                        canvas.setFillColor((ix + iy) % 2 ? Color::SlateGray : Color::LightSlateGray);
                        rect(x + float(ix * 10), y + float(iy * 10), std::min(10.0f, cell - float(ix * 10)), 10);
                    }
            canvas.setFillColor(item.value);
            canvas.fillRect(x, y, cell, 40);
            text(item.name, x, y + 45);
            const auto c = item.value.clamped();
            char code[32];
            std::snprintf(code, sizeof(code), "#%02X%02X%02X / a %.1f", int(std::round(c.r * 255)),
                          int(std::round(c.g * 255)), int(std::round(c.b * 255)), double(c.a));
            text(code, x, y + 66, 12, Color::TextSecondary);
            canvas.restore();
        }
        for (int row = 0; row < 2; ++row)
        {
            const float y = height - 174 + float(row) * 56;
            text(row ? "Oklab interpolation" : "RGB component interpolation", 24, y, 16);
            const float segment = std::max(1.0f, (width - 48) / 64);
            for (int i = 0; i < 64; ++i)
            {
                const float t = float(i) / 63;
                canvas.setFillColor(row ? Color::lerpOklab(Color::Coral, Color::DodgerBlue, t)
                                        : Color::lerp(Color::Coral, Color::DodgerBlue, t));
                rect(24 + float(i) * segment, y + 25, segment + .25f, 20);
            }
        }
        text("UI palette names are suggestions; existing Color and SVG keyword values keep their original meanings.",
             24, height - 38, 14, Color::TextSecondary);
    }
};
} // namespace
int main(int argc, char** argv)
{
    bool smoke = false;
    std::string font, capture;
    std::size_t page = 0;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--smoke-test")
            smoke = true;
        else if (arg == "--font" && i + 1 < argc)
            font = argv[++i];
        else if (arg == "--screenshot" && i + 1 < argc)
            capture = argv[++i];
        else if (arg == "--page" && i + 1 < argc)
            page = std::stoull(argv[++i]);
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
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, smoke ? GLFW_FALSE : GLFW_TRUE);
    auto* window = glfwCreateWindow(1180, 860, "VectorGL - color palette", nullptr, nullptr);
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
        Canvas canvas;
        canvas.init();
        App app{canvas, font, page};
        glfwSetWindowUserPointer(window, &app);
        struct Callbacks
        {
            GLFWwindow* window;
            ~Callbacks()
            {
                glfwSetKeyCallback(window, nullptr);
                glfwSetFramebufferSizeCallback(window, nullptr);
                glfwSetWindowContentScaleCallback(window, nullptr);
                glfwSetWindowRefreshCallback(window, nullptr);
                glfwSetWindowUserPointer(window, nullptr);
            }
        } callbacks{window};
        glfwSetKeyCallback(window,
                           [](GLFWwindow* w, int key, int, int action, int)
                           {
                               if (action != GLFW_PRESS && action != GLFW_REPEAT)
                                   return;
                               auto& app = *static_cast<App*>(glfwGetWindowUserPointer(w));
                               if (key == GLFW_KEY_ESCAPE)
                                   glfwSetWindowShouldClose(w, true);
                               else if (key == GLFW_KEY_RIGHT || key == GLFW_KEY_PAGE_DOWN)
                               {
                                   ++app.page;
                                   app.dirty = true;
                               }
                               else if (key == GLFW_KEY_LEFT || key == GLFW_KEY_PAGE_UP)
                               {
                                   app.page = app.page ? app.page - 1 : app.pages - 1;
                                   app.dirty = true;
                               }
                           });
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int, int)
                                       { static_cast<App*>(glfwGetWindowUserPointer(w))->dirty = true; });
        glfwSetWindowContentScaleCallback(window, [](GLFWwindow* w, float, float)
                                          { static_cast<App*>(glfwGetWindowUserPointer(w))->dirty = true; });
        glfwSetWindowRefreshCallback(window, [](GLFWwindow* w)
                                     { static_cast<App*>(glfwGetWindowUserPointer(w))->dirty = true; });
        int frames = 0;
        while (!glfwWindowShouldClose(window))
        {
            int width = 0, height = 0;
            float sx = 1, sy = 1;
            glfwGetFramebufferSize(window, &width, &height);
            glfwGetWindowContentScale(window, &sx, &sy);
            if (width > 0 && height > 0 && (app.dirty || smoke))
            {
                app.dirty = false;
                const auto bg = Color::Background;
                glClearColor(bg.r, bg.g, bg.b, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                canvas.beginFrame(width, height);
                canvas.setTransform(Mat3x3::scaling(sx, sy));
                app.draw(float(width) / sx, float(height) / sy);
                canvas.endFrame();
                if (frames == 0 && !capture.empty())
                    screenshot(capture, width, height);
                if (glGetError() != GL_NO_ERROR)
                    throw std::runtime_error("Palette OpenGL error");
                glfwSwapBuffers(window);
                ++frames;
            }
            if (smoke)
            {
                if (frames >= 3)
                    break;
                glfwPollEvents();
            }
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
