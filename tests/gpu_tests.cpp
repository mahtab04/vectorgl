#define GLFW_INCLUDE_NONE

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <array>
#include <exception>
#include <iostream>

#include <vectorgl/canvas.hpp>
#include <vectorgl/color.hpp>

namespace
{

constexpr int kFramebufferSize = 128;

int fail(const char* message)
{
    std::cerr << "[vectorgl_gpu_tests] " << message << '\n';
    return 1;
}

} // namespace

int main()
{
    if (glfwInit() != GLFW_TRUE)
        return fail("GLFW initialization failed");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window =
        glfwCreateWindow(kFramebufferSize, kFramebufferSize, "VectorGL GPU integration test", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return fail("hidden OpenGL 3.3 window creation failed");
    }

    glfwMakeContextCurrent(window);
    if (gladLoadGL(glfwGetProcAddress) == 0)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return fail("GLAD could not load OpenGL 3.3");
    }

    int result = 0;
    try
    {
        vectorgl::Canvas canvas;
        canvas.init();

        glViewport(0, 0, kFramebufferSize, kFramebufferSize);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        canvas.beginFrame(kFramebufferSize, kFramebufferSize);
        canvas.setFillColor({1.0f, 0.0f, 0.0f, 1.0f});
        canvas.fillRect(24.0f, 24.0f, 80.0f, 80.0f);
        canvas.endFrame();
        glFinish();

        std::array<unsigned char, 4> pixel{};
        glReadPixels(kFramebufferSize / 2, kFramebufferSize / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());

        if (glGetError() != GL_NO_ERROR)
            result = fail("OpenGL reported an error after rendering");
        else if (pixel[0] < 200 || pixel[1] > 40 || pixel[2] > 40 || pixel[3] < 200)
            result = fail("rendered center pixel was not opaque red");

        canvas.destroy();
    }
    catch (const std::exception& error)
    {
        std::cerr << "[vectorgl_gpu_tests] Exception: " << error.what() << '\n';
        result = 1;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
