#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "app.hpp"

namespace
{
void screenshot(const std::filesystem::path& path, int width, int height)
{
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    for (std::size_t i = 0; i < pixels.size(); i += 4)
        std::swap(pixels[i], pixels[i + 2]);
    unsigned char header[54]{};
    const auto put = [&](int offset, uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
            header[offset + i] = static_cast<unsigned char>(value >> (8 * i));
    };
    header[0] = 'B';
    header[1] = 'M';
    put(2, static_cast<uint32_t>(54 + pixels.size()));
    put(10, 54);
    put(14, 40);
    put(18, width);
    put(22, height);
    header[26] = 1;
    header[28] = 32;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(header), sizeof(header));
    file.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    if (!file)
        throw std::runtime_error("Could not save screenshot");
}
std::string findFont()
{
    for (const char* path : {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
                             "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                             "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
                             "/System/Library/Fonts/Supplemental/Arial.ttf"})
        if (std::filesystem::exists(path))
            return path;
    return {};
}
struct SmokeDirectory
{
    std::filesystem::path path;
    explicit SmokeDirectory(bool enabled)
    {
        if (!enabled)
            return;
        path = std::filesystem::temp_directory_path() /
               ("flow-workspace-smoke-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(path))
            throw std::runtime_error("Smoke folder already exists");
    }
    ~SmokeDirectory()
    {
        if (!path.empty())
        {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    }
};
flow::App& app(GLFWwindow* window)
{
    return *static_cast<flow::App*>(glfwGetWindowUserPointer(window));
}
template <typename Function> void callback(GLFWwindow* window, Function function)
{
    try
    {
        function();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Input error: " << error.what() << '\n';
    }
}
} // namespace

int main(int argc, char** argv)
{
    std::string font = findFont();
    std::filesystem::path document, capture, stateDirectory;
    bool smoke = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument == "--smoke-test")
            smoke = true;
        else if (argument == "--font" && i + 1 < argc)
            font = argv[++i];
        else if (argument == "--state-dir" && i + 1 < argc)
            stateDirectory = argv[++i];
        else if (argument == "--screenshot" && i + 1 < argc)
            capture = argv[++i];
        else if (argument == "--help")
        {
            std::cout << "Flow Studio [board.json] [--font font.ttf] [--state-dir folder] [--smoke-test] [--screenshot "
                         "preview.bmp]\n";
            return 0;
        }
        else if (!argument.empty() && argument.front() != '-' && document.empty())
            document = argument;
        else
        {
            std::cerr << "Unknown or incomplete argument: " << argument << '\n';
            return 1;
        }
    }
    if (font.empty())
    {
        std::cerr << "No UI font found. Pass --font path/to/font.ttf\n";
        return 1;
    }
    glfwSetErrorCallback([](int, const char* description) { std::cerr << "GLFW: " << description << '\n'; });
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    if (smoke)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    auto* window = glfwCreateWindow(1440, 900, "Flow Studio", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwSetWindowSizeLimits(window, 1180, 780, GLFW_DONT_CARE, GLFW_DONT_CARE);
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
        SmokeDirectory smokeDirectory(smoke);
        flow::App editor(window, font, smoke ? smokeDirectory.path / "workspace" : stateDirectory, document);
        glfwSetWindowUserPointer(window, &editor);
        glfwSetCursorPosCallback(window,
                                 [](GLFWwindow* w, double x, double y) {
                                     callback(w, [&] { app(w).pointer(static_cast<float>(x), static_cast<float>(y)); });
                                 });
        glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods)
                                   { callback(w, [&] { app(w).mouse(button, action, mods); }); });
        glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int, int action, int mods)
                           { callback(w, [&] { app(w).key(key, action, mods); }); });
        glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int cp) { callback(w, [&] { app(w).character(cp); }); });
        glfwSetScrollCallback(window, [](GLFWwindow* w, double, double y)
                              { callback(w, [&] { app(w).scroll(static_cast<float>(y)); }); });
        glfwSetWindowCloseCallback(window, [](GLFWwindow* w) { callback(w, [&] { app(w).requestClose(); }); });
        glfwSetWindowFocusCallback(window,
                                   [](GLFWwindow* w, int focused)
                                   {
                                       if (!focused)
                                           callback(w,
                                                    [&]
                                                    {
                                                        app(w).mouse(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
                                                        app(w).mouse(GLFW_MOUSE_BUTTON_MIDDLE, GLFW_RELEASE, 0);
                                                    });
                                   });
        double previous = glfwGetTime();
        int frames = 0;
        while (!glfwWindowShouldClose(window))
        {
            editor.tickWorkspace();
            int width, height, fw, fh;
            glfwGetWindowSize(window, &width, &height);
            glfwGetFramebufferSize(window, &fw, &fh);
            if (width <= 0 || height <= 0 || fw <= 0 || fh <= 0)
            {
                glfwWaitEventsTimeout(0.05);
                continue;
            }
            const double now = glfwGetTime();
            const float dt = static_cast<float>(std::min(now - previous, 0.1));
            previous = now;
            editor.render(static_cast<float>(width), static_cast<float>(height), static_cast<float>(fw) / width, dt);
            if (glGetError() != GL_NO_ERROR)
                throw std::runtime_error("OpenGL rendering error");
            if (frames == 0 && !capture.empty())
                screenshot(capture, fw, fh);
            if (smoke && frames == 0)
            {
                const auto directory =
                    std::filesystem::temp_directory_path() /
                    ("flow-studio-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
                if (!std::filesystem::create_directory(directory))
                    throw std::runtime_error("Smoke test directory already exists");
                try
                {
                    editor.smokeTest(directory);
                }
                catch (...)
                {
                    std::filesystem::remove_all(directory);
                    throw;
                }
                std::filesystem::remove_all(directory);
                editor.pointer(60, 190);
                editor.mouse(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
                editor.mouse(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
                editor.key(GLFW_KEY_Z, GLFW_PRESS, GLFW_MOD_CONTROL);
                editor.key(GLFW_KEY_F, GLFW_PRESS, 0);
            }
            if (smoke && frames == 1)
            {
                editor.pointer(1340, 40);
                editor.mouse(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
                editor.mouse(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
            }
            if (smoke && ++frames == 3)
                break;
            if (!smoke)
                ++frames;
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        glfwSetWindowUserPointer(window, nullptr);
    }
    catch (const std::exception& error)
    {
        std::cerr << "Flow Studio: " << error.what() << '\n';
        result = 1;
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
