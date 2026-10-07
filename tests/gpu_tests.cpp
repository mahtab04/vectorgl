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

PFNGLDRAWARRAYSPROC originalDrawArrays = nullptr;
int drawArrayCalls = 0;
void GLAD_API_PTR countDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    ++drawArrayCalls;
    originalDrawArrays(mode, first, count);
}

struct DrawArraySpy
{
    DrawArraySpy()
    {
        originalDrawArrays = glad_glDrawArrays;
        glad_glDrawArrays = countDrawArrays;
        drawArrayCalls = 0;
    }
    ~DrawArraySpy()
    {
        glad_glDrawArrays = originalDrawArrays;
    }
};

int fail(const char* message)
{
    std::cerr << "[vectorgl_gpu_tests] " << message << '\n';
    return 1;
}

bool pixelIs(const std::array<unsigned char, 4>& pixel, int red, int green, int blue)
{
    constexpr int tolerance = 45;
    return std::abs(static_cast<int>(pixel[0]) - red) <= tolerance &&
           std::abs(static_cast<int>(pixel[1]) - green) <= tolerance &&
           std::abs(static_cast<int>(pixel[2]) - blue) <= tolerance && pixel[3] >= 200;
}

void glfwErrorCallback(int error, const char* description)
{
    std::cerr << "[vectorgl_gpu_tests/GLFW] error " << error << ": " << (description ? description : "no description")
              << '\n';
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
    glfwSetErrorCallback(glfwErrorCallback);

    if (glfwInit() != GLFW_TRUE)
        return fail("GLFW initialization failed");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window =
        glfwCreateWindow(kFramebufferSize, kFramebufferSize, "VectorGL GPU integration test", nullptr, nullptr);
    if (!window)
    {
        const char* description = nullptr;
        const int error = glfwGetError(&description);
        std::cerr << "[vectorgl_gpu_tests] GLFW error " << error << ": "
                  << (description ? description : "no description") << '\n';
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

        if (!throws<std::invalid_argument>([&] { canvas.clipRect(0.0f, 0.0f, -1.0f, 10.0f); }))
            result = fail("negative clip dimensions did not fail");

        // The first draw must flush before clipping changes, otherwise it
        // would incorrectly inherit the later scissor rectangle.
        canvas.setFillColor({1.0f, 0.0f, 0.0f, 1.0f});
        canvas.fillRect(0.0f, 0.0f, 128.0f, 128.0f);

        canvas.save();
        canvas.clipRect(32.0f, 32.0f, 64.0f, 64.0f);
        canvas.setFillColor({0.0f, 1.0f, 0.0f, 1.0f});
        canvas.fillRect(0.0f, 0.0f, 128.0f, 128.0f);

        canvas.save();
        canvas.clipRect(48.0f, 48.0f, 16.0f, 16.0f);
        canvas.setFillColor({0.0f, 0.0f, 1.0f, 1.0f});
        canvas.fillRect(0.0f, 0.0f, 128.0f, 128.0f);
        canvas.restore();

        canvas.setFillColor({1.0f, 1.0f, 0.0f, 1.0f});
        canvas.fillRect(80.0f, 80.0f, 40.0f, 20.0f);
        canvas.restore();
        canvas.endFrame();
        if (canvas.renderer().isFrameActive())
            result = fail("renderer kept frame active after endFrame");
        else if (!throws<std::logic_error>([&] { canvas.endFrame(); }))
            result = fail("endFrame without an active frame did not fail");

        glFinish();

        const auto readPixel = [](int x, int y)
        {
            std::array<unsigned char, 4> pixel{};
            glReadPixels(x, kFramebufferSize - 1 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            return pixel;
        };

        if (glGetError() != GL_NO_ERROR)
            result = fail("OpenGL reported an error after rendering");
        else if (!pixelIs(readPixel(16, 16), 255, 0, 0))
            result = fail("pre-clip batch did not remain red outside the clip");
        else if (!pixelIs(readPixel(40, 40), 0, 255, 0))
            result = fail("outer clip did not render green");
        else if (!pixelIs(readPixel(56, 56), 0, 0, 255))
            result = fail("nested clip did not render blue");
        else if (!pixelIs(readPixel(90, 90), 255, 255, 0))
            result = fail("restored outer clip did not render yellow");
        else if (!pixelIs(readPixel(110, 90), 255, 0, 0))
            result = fail("restored outer clip leaked beyond its boundary");

        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.beginFrame(kFramebufferSize, kFramebufferSize);

        if (!throws<std::invalid_argument>([&] { canvas.clipRoundedRect(0.0f, 0.0f, 10.0f, 10.0f, -1.0f); }))
            result = fail("negative rounded clip radius did not fail");

        canvas.save();
        canvas.clipRoundedRect(16.0f, 16.0f, 96.0f, 96.0f, 24.0f);
        canvas.setFillColor({0.0f, 1.0f, 0.0f, 1.0f});
        canvas.fillRect(0.0f, 0.0f, 128.0f, 128.0f);

        canvas.save();
        canvas.clipRoundedRect(48.0f, 48.0f, 48.0f, 48.0f, 14.0f);
        canvas.setFillColor({0.0f, 0.0f, 1.0f, 1.0f});
        canvas.fillRect(0.0f, 0.0f, 128.0f, 128.0f);
        canvas.restore();

        canvas.setFillColor({1.0f, 1.0f, 0.0f, 1.0f});
        canvas.fillRect(80.0f, 24.0f, 24.0f, 16.0f);
        canvas.restore();
        canvas.endFrame();
        glFinish();

        if (glGetError() != GL_NO_ERROR)
            result = fail("OpenGL reported an error after rounded clipping");
        else if (!pixelIs(readPixel(18, 18), 255, 0, 0))
            result = fail("rounded clip included a pixel outside its corner");
        else if (!pixelIs(readPixel(24, 64), 0, 255, 0))
            result = fail("rounded clip did not render its interior");
        else if (!pixelIs(readPixel(64, 64), 0, 0, 255))
            result = fail("nested rounded clip did not render its intersection");
        else if (!pixelIs(readPixel(88, 30), 255, 255, 0))
            result = fail("restored rounded clip did not render");
        else if (!pixelIs(readPixel(118, 64), 255, 0, 0))
            result = fail("rounded clip leaked outside its bounds");

        {
            DrawArraySpy spy;
            GLuint textures[2]{};
            glGenTextures(2, textures);
            const unsigned char white[] = {255, 255, 255, 255};
            for (GLuint texture : textures)
            {
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            canvas.beginFrame(kFramebufferSize, kFramebufferSize);
            const auto glyph = [&](GLuint texture, vectorgl::Color color, float x, float w)
            { canvas.renderer().drawGlyph(x, 0, w, 128, 0, 0, 1, 1, texture, color, vectorgl::Mat3x3::identity()); };
            for (int i = 0; i < 20; ++i)
                glyph(textures[0], vectorgl::Color::Green, 0, 128);
            if (drawArrayCalls != 0)
                result = fail("consecutive glyphs were not deferred into one batch");
            canvas.setFillColor(vectorgl::Color::Red);
            canvas.fillRect(0, 0, 64, 128);
            glyph(textures[0], vectorgl::Color::Blue, 64, 64);
            canvas.clipRect(96, 0, 32, 128);
            glyph(textures[0], {1, 1, 0, 1}, 0, 128);
            canvas.endFrame();
            glFinish();
            if (drawArrayCalls != 3)
                result = fail("glyphs did not batch across shape and clip boundaries correctly");
            else if (!pixelIs(readPixel(32, 64), 255, 0, 0) || !pixelIs(readPixel(80, 64), 0, 0, 255) ||
                     !pixelIs(readPixel(112, 64), 255, 255, 0))
                result = fail("glyph batching changed drawing order or clipping");

            drawArrayCalls = 0;
            canvas.beginFrame(kFramebufferSize, kFramebufferSize);
            glyph(textures[0], vectorgl::Color::White, 0, 16);
            glyph(textures[1], vectorgl::Color::White, 16, 16);
            canvas.endFrame();
            if (drawArrayCalls != 2)
                result = fail("atlas changes did not split glyph batches");

            drawArrayCalls = 0;
            canvas.beginFrame(kFramebufferSize, kFramebufferSize);
            for (int i = 0; i < 1025; ++i)
                glyph(textures[0], vectorgl::Color::White, 0, 16);
            canvas.endFrame();
            if (drawArrayCalls != 2)
                result = fail("glyph batch size is not bounded at 1024 glyphs");

            canvas.setFontCacheLimit(1);
            canvas.beginFrame(kFramebufferSize, kFramebufferSize);
            if (!canvas.setFont(VECTORGL_TEST_FONT, 16))
                result = fail("test font failed to load into GPU cache");
            canvas.fillText("?", 0, 0);
            if (!canvas.setFont(VECTORGL_TEST_FONT, 18))
                result = fail("second test font failed to load into GPU cache");
            canvas.fillText("?", 32, 0);
            canvas.clearFontCache();
            canvas.endFrame();
            if (glGetError() != GL_NO_ERROR)
                result = fail("batched glyphs or cache eviction caused an OpenGL error");
            glDeleteTextures(2, textures);
        }

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
