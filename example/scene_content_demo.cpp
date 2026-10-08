#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <vectorgl/canvas.hpp>
#include <vectorgl/font.hpp>
#include <vectorgl/image.hpp>
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

struct App
{
    Canvas canvas;
    Scene scene{canvas.renderer()};
    std::shared_ptr<Node> root = scene.group();
    std::shared_ptr<Font> font = std::make_shared<Font>();
    std::shared_ptr<Image> image = std::make_shared<Image>();
    std::vector<std::shared_ptr<Node>> items;
    std::vector<Vec2> positions;
    std::shared_ptr<Node> selected;
    Vec2 grab;
    std::string fontPath;

    bool init(const std::string& requestedFont)
    {
        canvas.init();
        if (!requestedFont.empty())
        {
            fontPath = requestedFont;
            if (!font->load(fontPath, 22))
                return false;
        }
        else
        {
            for (const char* path : {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
                                     "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                     "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
                                     "/System/Library/Fonts/Supplemental/Arial.ttf"})
                if (font->load(path, 22))
                {
                    fontPath = path;
                    break;
                }
        }
        if (fontPath.empty() || !image->load(VECTORGL_DEMO_IMAGE))
            return false;
        auto panel = scene.roundedRect(40, 150, 410, 430, 20);
        panel->setFill({0.1f, 0.14f, 0.21f, 1});
        panel->setZIndex(-2);
        root->addChild(panel);
        auto panel2 = scene.roundedRect(490, 150, 430, 430, 20);
        panel2->setFill({0.1f, 0.14f, 0.21f, 1});
        panel2->setZIndex(-2);
        root->addChild(panel2);

        TextLayoutOptions options;
        options.maxWidth = 340;
        options.align = TextAlign::Center;
        options.lineSpacing = 1.25f;
        auto paragraph = scene.text("Scene text: Caf\xC3\xA9, \xCE\xA9 and Unicode.\n"
                                    "Wrapped, centered and selectable. Drag this paragraph to move it.",
                                    75, 182, font, options);
        paragraph->setFill({0.86f, 0.92f, 1, 1});
        root->addChild(paragraph);
        items.push_back(paragraph);
        auto tile = scene.image(image, 95, 365, 290, 160);
        tile->setOpacity(0.85f);
        root->addChild(tile);
        items.push_back(tile);

        auto group = scene.group();
        group->setPosition(700, 350);
        group->setRotation(-0.15f);
        group->setScale(1, 0.9f);
        root->addChild(group);
        auto rotatedImage = scene.image(image, -145, -90, 290, 180);
        group->addChild(rotatedImage);
        items.push_back(rotatedImage);
        options.maxWidth = 320;
        auto caption = scene.text("Shared image + parent transform", -160, 110, font, options);
        caption->setFill({0.5f, 0.84f, 1, 1});
        group->addChild(caption);
        items.push_back(caption);
        for (const auto& item : items)
            positions.push_back(item->position());
        return true;
    }

    Vec2 parentPoint(Vec2 point) const
    {
        if (!selected || !selected->parent())
            return point;
        const auto& m = selected->parent()->worldTransform().m;
        const float determinant = m[0] * m[4] - m[1] * m[3];
        if (determinant == 0)
            return selected->position();
        const float x = point.x - m[6], y = point.y - m[7];
        return {(m[4] * x - m[3] * y) / determinant, (m[0] * y - m[1] * x) / determinant};
    }

    void select(Vec2 point)
    {
        selected = scene.pick(point.x, point.y);
        if (std::find(items.begin(), items.end(), selected) == items.end())
            selected.reset();
        if (selected)
            grab = parentPoint(point) - selected->position();
    }

    void drag(Vec2 point)
    {
        if (selected)
        {
            point = parentPoint(point) - grab;
            selected->setPosition(point.x, point.y);
        }
    }

    void draw(int width, int height, int fbWidth, int fbHeight)
    {
        root->setScale(float(fbWidth) / width, float(fbHeight) / height);
        canvas.beginFrame(fbWidth, fbHeight);
        scene.render();
        if (selected)
        {
            float x, y, w, h;
            if (selected->type() == ShapeType::Image)
            {
                const auto size = selected->imageSize();
                w = size.x;
                h = size.y;
                x = -w * 0.5f;
                y = -h * 0.5f;
            }
            else
            {
                const auto layout = selected->textLayout();
                x = std::numeric_limits<float>::infinity();
                float right = 0;
                for (const auto& line : layout.lines)
                {
                    if (line.width <= 0)
                        continue;
                    x = std::min(x, line.x);
                    right = std::max(right, line.x + line.width);
                }
                if (!std::isfinite(x))
                    x = 0;
                w = right - x;
                y = 0;
                h = layout.height;
            }
            canvas.renderer().strokePath({{{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, {x, y}}},
                                         {0.3f, 0.85f, 1, 1}, 2, selected->worldTransform());
        }
        canvas.scale(float(fbWidth) / width, float(fbHeight) / height);
        (void)canvas.setFont(fontPath, 30);
        canvas.setFillColor(Color::White);
        canvas.fillText("Text and images in the scene", 40, 28);
        (void)canvas.setFont(fontPath, 16);
        canvas.setFillColor({0.65f, 0.75f, 0.87f, 1});
        canvas.fillText("Click / drag to select and move | Up / Down: order | O: opacity | R: reset | Esc: close", 40,
                        78);
        canvas.fillText("Text uses logical bounds. Images include transparent pixels when picking.", 40, 108);
        canvas.endFrame();
    }
};

Vec2 cursor(GLFWwindow* window)
{
    double x, y;
    int w, h, fw, fh;
    glfwGetCursorPos(window, &x, &y);
    glfwGetWindowSize(window, &w, &h);
    glfwGetFramebufferSize(window, &fw, &fh);
    return {w > 0 ? float(x * fw / w) : 0, h > 0 ? float(y * fh / h) : 0};
}
} // namespace

int main(int argc, char** argv)
{
    bool smoke = false;
    std::string fontPath, screenshotPath;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--smoke-test")
            smoke = true;
        else if (std::string(argv[i]) == "--font" && i + 1 < argc)
            fontPath = argv[++i];
        else if (std::string(argv[i]) == "--screenshot" && i + 1 < argc)
            screenshotPath = argv[++i];
        else
            return 1;
    }
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (smoke)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    auto* window = glfwCreateWindow(960, 640, "VectorGL - Scene text and images", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwSetWindowSizeLimits(window, 960, 640, GLFW_DONT_CARE, GLFW_DONT_CARE);
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
        if (!app.init(fontPath))
            throw std::runtime_error("Could not load demo resources; use --font path/to/font.ttf");
        glfwSetWindowUserPointer(window, &app);
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow* w, int button, int action, int)
                                   {
                                       if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
                                           static_cast<App*>(glfwGetWindowUserPointer(w))->select(cursor(w));
                                   });
        glfwSetCursorPosCallback(window,
                                 [](GLFWwindow* w, double, double)
                                 {
                                     if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
                                         static_cast<App*>(glfwGetWindowUserPointer(w))->drag(cursor(w));
                                 });
        glfwSetKeyCallback(window,
                           [](GLFWwindow* w, int key, int, int action, int)
                           {
                               if (action != GLFW_PRESS)
                                   return;
                               auto& app = *static_cast<App*>(glfwGetWindowUserPointer(w));
                               if (key == GLFW_KEY_ESCAPE)
                                   glfwSetWindowShouldClose(w, GLFW_TRUE);
                               if (key == GLFW_KEY_R)
                                   for (size_t i = 0; i < app.items.size(); ++i)
                                   {
                                       app.items[i]->setPosition(app.positions[i].x, app.positions[i].y);
                                       app.items[i]->setZIndex(0);
                                       app.items[i]->setOpacity(1);
                                   }
                               if (app.selected)
                               {
                                   if (key == GLFW_KEY_O)
                                       app.selected->setOpacity(app.selected->style().opacity < 1 ? 1 : 0.4f);
                                   if (key == GLFW_KEY_UP || key == GLFW_KEY_DOWN)
                                       app.selected->setZIndex(app.selected->zIndex() + (key == GLFW_KEY_UP ? 1 : -1));
                               }
                           });
        int frames = 0;
        while (!glfwWindowShouldClose(window) && (!smoke || frames < 3))
        {
            glfwPollEvents();
            int width, height, fw, fh;
            glfwGetWindowSize(window, &width, &height);
            glfwGetFramebufferSize(window, &fw, &fh);
            if (width <= 0 || height <= 0 || fw <= 0 || fh <= 0)
                continue;
            glClearColor(0.035f, 0.05f, 0.085f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            app.draw(width, height, fw, fh);
            if (frames == 0 && !screenshotPath.empty())
                screenshot(screenshotPath, fw, fh);
            if (smoke && frames == 0)
            {
                const auto layout = app.items[0]->textLayout();
                const auto textPoint = app.items[0]->worldTransform().transformPoint(
                    {layout.lines[0].x + layout.lines[0].width * 0.5f, 10});
                app.select(textPoint);
                if (app.selected != app.items[0])
                    result = 1;
                const auto point = app.items[2]->worldTransform().transformPoint({-70, -40});
                app.select(point);
                if (app.selected != app.items[2])
                    result = 1;
                app.drag({point.x + 25, point.y + 15});
                if (app.scene.pick(point.x + 25, point.y + 15) != app.items[2])
                    result = 1;
            }
            glfwSwapBuffers(window);
            ++frames;
        }
        if (glGetError() != GL_NO_ERROR)
            result = 1;
        glfwSetWindowUserPointer(window, nullptr);
    } // Scene and shared GL resources are destroyed before the context.
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
