#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vectorgl/canvas.hpp>
#include <vectorgl/font.hpp>
#include <vectorgl/scene.hpp>

namespace
{
struct View
{
    float x = 30, y = 160, zoom = 1;
};

std::string findFont()
{
    for (const char* path : {"C:/Windows/Fonts/segoeui.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                             "/System/Library/Fonts/Supplemental/Arial.ttf"})
        if (std::filesystem::exists(path))
            return path;
    return {};
}
} // namespace

int main(int argc, char** argv)
{
    bool smoke = false;
    std::string fontPath = findFont();
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument == "--smoke-test")
            smoke = true;
        else if (argument == "--font" && i + 1 < argc)
            fontPath = argv[++i];
        else
        {
            std::cerr << "Usage: vectorgl_large_scene [--font font.ttf] [--smoke-test]\n";
            return 1;
        }
    }
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    if (smoke)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    auto* window = glfwCreateWindow(1120, 780, "VectorGL - Large scene profiler", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(smoke ? 0 : 1);
    if (!gladLoadGL(glfwGetProcAddress))
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    int result = 0;
    try
    {
        vectorgl::Canvas canvas;
        canvas.init();
        vectorgl::Scene scene(canvas.renderer());
        const auto world = scene.group();
        auto font = std::make_shared<vectorgl::Font>();
        if (fontPath.empty() || !font->load(fontPath, 16))
        {
            if (smoke)
                throw std::runtime_error("Smoke test requires a loadable font");
            font.reset();
        }
        if (font && !canvas.setFont(fontPath, 16))
            throw std::runtime_error("HUD font failed to load");
        constexpr int columns = 100, rows = 100;
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < columns; ++x)
            {
                auto tile = scene.roundedRect(x * 80.0f, y * 60.0f, 64, 44, 8);
                tile->setFill({0.15f + x * 0.006f, 0.25f + y * 0.005f, 0.7f, 1});
                world->addChild(tile);
            }
        if (font)
            for (int i = 0; i < 16; ++i)
            {
                auto label = scene.text("Cached label " + std::to_string(i), 5, i * 60.0f + 14, font);
                label->setFill(vectorgl::Color::White);
                label->setZIndex(1);
                world->addChild(label);
            }

        View view;
        glfwSetWindowUserPointer(window, &view);
        glfwSetScrollCallback(window,
                              [](GLFWwindow* target, double, double delta)
                              {
                                  auto& v = *static_cast<View*>(glfwGetWindowUserPointer(target));
                                  int w, h, fw, fh;
                                  glfwGetWindowSize(target, &w, &h);
                                  glfwGetFramebufferSize(target, &fw, &fh);
                                  if (w <= 0 || h <= 0)
                                      return;
                                  double mx, my;
                                  glfwGetCursorPos(target, &mx, &my);
                                  const float x = static_cast<float>(mx) * fw / w, y = static_cast<float>(my) * fh / h;
                                  const float next =
                                      std::clamp(v.zoom * static_cast<float>(std::pow(1.15, delta)), 0.05f, 4.0f);
                                  v.x = x - (x - v.x) * next / v.zoom;
                                  v.y = y - (y - v.y) * next / v.zoom;
                                  v.zoom = next;
                              });
        bool cWasDown = false, lWasDown = false, dragging = false, cacheEnabled = true;
        double previousX = 0, previousY = 0;
        int frames = 0;
        double cpuMs = 0;
        vectorgl::Renderer::FrameStats previousStats;
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                break;
            int fw, fh, w, h;
            glfwGetFramebufferSize(window, &fw, &fh);
            glfwGetWindowSize(window, &w, &h);
            if (fw <= 0 || fh <= 0 || w <= 0 || h <= 0)
            {
                glfwWaitEventsTimeout(0.05);
                continue;
            }
            const bool cDown = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
            if (cDown && !cWasDown)
                canvas.renderer().setViewportCulling(!canvas.renderer().viewportCulling());
            cWasDown = cDown;
            const bool lDown = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
            if (lDown && !lWasDown)
                cacheEnabled = !cacheEnabled;
            lWasDown = lDown;
            if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
                view = {};
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            const bool down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if (down && dragging)
            {
                view.x += static_cast<float>(mx - previousX) * fw / w;
                view.y += static_cast<float>(my - previousY) * fh / h;
            }
            dragging = down;
            previousX = mx;
            previousY = my;
            if (smoke)
            {
                canvas.renderer().setViewportCulling(frames != 2);
                view.zoom = frames == 3 ? 0.5f : 1.0f;
            }
            world->setPosition(view.x, view.y);
            world->setScale(view.zoom, view.zoom);
            if (font && (!cacheEnabled || frames == 0))
                font->clearLayoutCache();
            glClearColor(0.035f, 0.045f, 0.065f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            canvas.beginFrame(fw, fh);
            const auto start = std::chrono::steady_clock::now();
            scene.render();
            canvas.renderer().flush();
            const auto stats = canvas.renderer().frameStats();
            cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            if (smoke)
            {
                if ((frames == 2 && stats.nodesCulled != 0) || (frames != 2 && stats.nodesCulled < 9000) ||
                    (frames == 1 && stats.textLayoutHits != 16) || stats.drawCalls == 0)
                    throw std::runtime_error("Large scene counters/culling/cache smoke check failed");
                std::cout << "frame=" << frames << " submitted=" << stats.nodesSubmitted
                          << " culled=" << stats.nodesCulled << " calls=" << stats.drawCalls
                          << " stream_bytes=" << stats.bufferUploadBytes << " layout_hits=" << stats.textLayoutHits
                          << " layout_misses=" << stats.textLayoutMisses << " cpu_submit_ms=" << cpuMs << '\n';
            }
            std::ostringstream summary;
            summary << "10,000 tiles | submitted " << stats.nodesSubmitted << " | retained " << stats.nodesRendered
                    << " | culled " << stats.nodesCulled << " | draw calls " << stats.drawCalls;
            glfwSetWindowTitle(window, summary.str().c_str());
            if (font)
            {
                // HUD is excluded from the scene measurements above. Values
                // show the previous frame. A separate Canvas font keeps changing
                // HUD strings from evicting the scene's cached layouts.
                canvas.setFillColor({0.04f, 0.055f, 0.085f, 0.98f});
                canvas.fillRect(0, 0, static_cast<float>(fw), 140);
                canvas.setFillColor(vectorgl::Color::White);
                canvas.fillText("Large scene: 10,000 tiles + 16 text labels", 18, 15);
                canvas.fillText("Drag: pan | Wheel: zoom | C: culling | L: cold layouts | R: reset | Esc: close", 18,
                                42);
                std::ostringstream details;
                details << "Culling " << (canvas.renderer().viewportCulling() ? "ON" : "OFF") << " | layout cache "
                        << (cacheEnabled ? "warm" : "cleared each frame") << " | CPU scene submit " << cpuMs << " ms";
                canvas.fillText(details.str(), 18, 70);
                std::ostringstream counters;
                counters << "Previous scene: drawn " << previousStats.nodesRendered << " | culled "
                         << previousStats.nodesCulled << " | calls " << previousStats.drawCalls << " | stream bytes "
                         << previousStats.bufferUploadBytes << " | layout hits/misses " << previousStats.textLayoutHits
                         << '/' << previousStats.textLayoutMisses;
                canvas.fillText(counters.str(), 18, 98);
            }
            canvas.endFrame();
            previousStats = stats;
            glfwSwapBuffers(window);
            if (smoke && ++frames == 4)
                break;
            if (!smoke)
                ++frames;
        }
        // Resource destructors run while the GL context remains current.
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
