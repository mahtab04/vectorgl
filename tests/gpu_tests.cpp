#define GLFW_INCLUDE_NONE

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>

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

template <typename Exception, typename Function> bool throws(Function&& function)
{
    try
    {
        function();
    }
    catch (const Exception&)
    {
        return true;
    }
    return false;
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

        if (canvas.renderer().isInitialized() || canvas.renderer().isFrameActive())
            result = fail("new renderer reported an active lifecycle state");
        else if (!throws<std::logic_error>([&] { canvas.beginFrame(kFramebufferSize, kFramebufferSize); }))
            result = fail("beginFrame before init did not fail");

        canvas.init();
        if (!canvas.renderer().isInitialized())
            result = fail("renderer did not report initialized state");
        else if (!throws<std::logic_error>([&] { canvas.init(); }))
            result = fail("repeated init did not fail");
        else if (!throws<std::invalid_argument>([&] { canvas.beginFrame(0, kFramebufferSize); }))
            result = fail("invalid framebuffer dimensions did not fail");

        glViewport(0, 0, kFramebufferSize, kFramebufferSize);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        canvas.beginFrame(kFramebufferSize, kFramebufferSize);
        if (!canvas.renderer().isFrameActive())
            result = fail("renderer did not report active frame state");
        else if (!throws<std::logic_error>([&] { canvas.beginFrame(kFramebufferSize, kFramebufferSize); }))
            result = fail("nested beginFrame did not fail");

        canvas.renderer().beginEffectPass(0.0f, 0.0f, 32.0f, 32.0f);
        if (!throws<std::logic_error>([&] { canvas.renderer().beginEffectPass(0.0f, 0.0f, 32.0f, 32.0f); }))
            result = fail("nested effect pass did not fail");
        canvas.renderer().endEffectPass();
        if (!throws<std::logic_error>([&] { canvas.renderer().endEffectPass(); }))
            result = fail("endEffectPass without an active pass did not fail");

        canvas.setFillColor({1.0f, 0.0f, 0.0f, 1.0f});
        canvas.fillRect(24.0f, 24.0f, 80.0f, 80.0f);
        canvas.endFrame();
        if (canvas.renderer().isFrameActive())
            result = fail("renderer kept frame active after endFrame");
        else if (!throws<std::logic_error>([&] { canvas.endFrame(); }))
            result = fail("endFrame without an active frame did not fail");

        glFinish();

        std::array<unsigned char, 4> pixel{};
        glReadPixels(kFramebufferSize / 2, kFramebufferSize / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());

        if (glGetError() != GL_NO_ERROR)
            result = fail("OpenGL reported an error after rendering");
        else if (pixel[0] < 200 || pixel[1] > 40 || pixel[2] > 40 || pixel[3] < 200)
            result = fail("rendered center pixel was not opaque red");

        canvas.destroy();
        if (canvas.renderer().isInitialized())
            result = fail("renderer stayed initialized after destroy");
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
