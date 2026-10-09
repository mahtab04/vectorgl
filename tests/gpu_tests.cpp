#define GLFW_INCLUDE_NONE

#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vectorgl/canvas.hpp>
#include <vectorgl/color.hpp>
#include <vectorgl/font.hpp>
#include <vectorgl/image.hpp>
#include <vectorgl/scene.hpp>
#include <vectorgl/text_box.hpp>

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

        if (result == 0)
        {
            vectorgl::Scene scene;
            auto group = scene.group();
            group->setPosition(36, 34);
            group->setRotation(0.3f);
            group->setScale(1.2f, 0.8f);
            auto rounded = scene.roundedRect(-22, -18, 44, 36, 9);
            rounded->setFill(vectorgl::Color::White);
            rounded->setRotation(0.45f);
            rounded->setCornerRadii(2, 7, 12, 4);
            group->addChild(rounded);
            auto ellipse = scene.ellipse(92, 34, 22, 13);
            ellipse->setStroke(vectorgl::Color::White, 6);
            ellipse->setRotation(-0.4f);
            auto line = scene.line(12, 85, 52, 106);
            line->setStroke(vectorgl::Color::White, 7);
            vectorgl::Path2D triangle;
            triangle.moveTo(70, 80);
            triangle.lineTo(115, 80);
            triangle.lineTo(92, 115);
            triangle.closePath();
            auto path = scene.path(triangle);
            path->setFill(vectorgl::Color::White);
            scene.update(0);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            scene.render(canvas.renderer(), kFramebufferSize, kFramebufferSize);
            glFinish();
            for (int y = 2; y < kFramebufferSize - 2 && result == 0; y += 2)
                for (int x = 2; x < kFramebufferSize - 2; x += 2)
                {
                    const auto pixel = readPixel(x, y);
                    const bool picked = scene.pick(x + 0.5f, y + 0.5f) != nullptr;
                    // Exclude a two-pixel boundary band: path AA fringes can
                    // be almost opaque just outside the geometric stroke.
                    bool nearEdge = false;
                    for (int dy : {-2, 0, 2})
                        for (int dx : {-2, 0, 2})
                            if ((scene.pick(x + dx + 0.5f, y + dy + 0.5f) != nullptr) != picked)
                                nearEdge = true;
                    if (nearEdge)
                        continue;
                    if ((pixel[0] > 252 && !picked) || (pixel[0] < 3 && picked))
                    {
                        std::cerr << "Picking mismatch at " << x << ',' << y << ": red=" << static_cast<int>(pixel[0])
                                  << " picked=" << picked << '\n';
                        result = fail("scene picking disagrees with rendered shape geometry");
                        break;
                    }
                }
            path->setOpacity(0);
            line->setOpacity(0);
            glClear(GL_COLOR_BUFFER_BIT);
            scene.render(canvas.renderer(), kFramebufferSize, kFramebufferSize);
            glFinish();
            if (!pixelIs(readPixel(92, 90), 0, 0, 0) || !pixelIs(readPixel(32, 95), 0, 0, 0))
                result = fail("scene path or line ignored node opacity");
        }

        if (result == 0)
        {
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            canvas.beginFrame(kFramebufferSize, kFramebufferSize);
            GLuint unpackBuffer;
            glGenBuffers(1, &unpackBuffer);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, 4096, nullptr, GL_STATIC_DRAW);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 37);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 2);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 3);
            if (!canvas.setFont(VECTORGL_TEST_FONT, 32))
                result = fail("Unicode test font failed to load");
            canvas.setTextPixelSnap(false);
            canvas.setFillColor(vectorgl::Color::White);
            canvas.fillText("A", 10.3f, 10);
            canvas.fillText("\xCE\xA9", 60, 10);
            GLint restoredBuffer, restoredAlignment, restoredLength, restoredRows, restoredPixels;
            glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &restoredBuffer);
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &restoredAlignment);
            glGetIntegerv(GL_UNPACK_ROW_LENGTH, &restoredLength);
            glGetIntegerv(GL_UNPACK_SKIP_ROWS, &restoredRows);
            glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &restoredPixels);
            if (restoredBuffer != static_cast<GLint>(unpackBuffer) || restoredAlignment != 8 || restoredLength != 37 ||
                restoredRows != 2 || restoredPixels != 3)
                result = fail("lazy glyph upload corrupted pixel-unpack state");
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            glDeleteBuffers(1, &unpackBuffer);
            canvas.endFrame();
            glFinish();
            if (!pixelIs(readPixel(18, 25), 255, 255, 255) || !pixelIs(readPixel(82, 25), 255, 255, 255))
                result = fail("dynamic Unicode glyph did not render its actual outline");
            int transitionPixels = 0;
            for (int x = 5; x < 35; ++x)
            {
                const int value = readPixel(x, 25)[0];
                if (value > 5 && value < 250)
                    ++transitionPixels;
            }
            if (transitionPixels > 2 || transitionPixels == 0)
                result = fail("SDF text edges are blurred or lack antialias coverage");

            for (float radius : {0.0f, 2.0f})
            {
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                canvas.beginFrame(kFramebufferSize, kFramebufferSize);
                canvas.renderer().beginEffectPass(0, 0, kFramebufferSize, kFramebufferSize);
                canvas.setFillColor({1, 1, 1, 0.5f});
                canvas.fillText("A", 10.3f, 10);
                canvas.renderer().endEffectPass(); // Must flush queued text into the capture.
                canvas.renderer().applyBlur(radius);
                canvas.endFrame();
                glFinish();
                const auto center = readPixel(18, 25);
                if (center[0] < 110 || center[0] > 145 || readPixel(18, 100)[0] > 3)
                {
                    std::cerr << "Blur radius " << radius << " center=" << static_cast<int>(center[0])
                              << " bottom=" << static_cast<int>(readPixel(18, 100)[0]) << '\n';
                    result = fail("text effect changed opacity or vertically flipped the capture");
                }
                if (radius > 0 && readPixel(9, 25)[0] == 0)
                    result = fail("text blur did not spread edge coverage");
            }

            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            canvas.beginFrame(kFramebufferSize, kFramebufferSize);
            canvas.setTextPixelSnap(true);
            vectorgl::TextBox field;
            field.setFont(VECTORGL_TEST_FONT, 16);
            field.setBounds(0, 60, 80, 50);
            field.setText("A\xCE\xA9\xF0\x9F\x98\x80"
                          "AVAVAVAVAVAV");
            field.setFocused(true);
            if (!field.render(canvas, 0.01f))
                result = fail("Unicode text box rendering failed");
            field.handleKey(vectorgl::TextBoxKey::Home);
            field.handlePointerDown(36, 85, canvas);
            if (field.selectionStart() != 3)
                result = fail("text-box pointer hit split a UTF-8 codepoint");
            canvas.endFrame();
            if (glGetError() != GL_NO_ERROR)
                result = fail("Unicode text or blur produced an OpenGL error");
        }

        if (result == 0)
        {
            using namespace vectorgl;
            auto font = std::make_shared<Font>();
            auto image = std::make_shared<Image>();
            if (!font->load(VECTORGL_TEST_FONT, 32) || !image->load(VECTORGL_TEST_IMAGE))
                result = fail("scene resources failed to load");
            Scene scene(canvas.renderer());
            TextLayoutOptions options;
            options.maxWidth = 32;
            options.align = TextAlign::Right;
            options.wrap = TextWrap::Character;
            auto label = scene.text("A\xCE\xA9", 64, 0, font, options);
            label->setFill(Color::White);
            label->setOpacity(0.5f);
            auto tile = scene.image(image, 8, 64, 64, 64);
            // render() must refresh transforms even without update().
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            scene.render(canvas.renderer(), kFramebufferSize, kFramebufferSize);
            glFinish();
            const int textRed = readPixel(83, 20)[0];
            if (textRed < 120 || textRed > 135 || readPixel(88, 68)[0] < 120)
                result = fail("scene wrapped/aligned Unicode text or opacity incorrect");
            else if (!pixelIs(readPixel(24, 80), 255, 0, 0) || !pixelIs(readPixel(56, 80), 0, 255, 0) ||
                     !pixelIs(readPixel(24, 112), 0, 0, 255) || !pixelIs(readPixel(56, 112), 0, 0, 0))
                result = fail("scene image colors, orientation or source alpha incorrect");
            else if (scene.pick(56, 112) != tile || scene.pick(88, 68) != label)
                result = fail("scene text/image bounds not picked, including transparent image pixels");
            tile->setOpacity(0.5f);
            label->setBlur(0);
            glClear(GL_COLOR_BUFFER_BIT);
            scene.render(canvas.renderer(), kFramebufferSize, kFramebufferSize);
            glFinish();
            if (readPixel(24, 80)[0] < 120 || readPixel(24, 80)[0] > 135 || readPixel(83, 20)[0] < 120)
                result = fail("scene image opacity or text effect capture incorrect");
            label->clearEffects();
            auto group = scene.group();
            group->setPosition(90, 100);
            group->setRotation(0.4f);
            group->setScale(-1, 0.7f);
            tile->setPosition(0, 0);
            tile->setRotation(0.3f);
            tile->setOpacity(1);
            group->addChild(tile);
            scene.update(0);
            const auto imagePoint = tile->worldTransform().transformPoint({-16, -16});
            glClear(GL_COLOR_BUFFER_BIT);
            scene.render(canvas.renderer(), kFramebufferSize, kFramebufferSize);
            glFinish();
            if (scene.pick(imagePoint.x, imagePoint.y) != tile ||
                !pixelIs(readPixel(static_cast<int>(imagePoint.x), static_cast<int>(imagePoint.y)), 255, 0, 0))
                result = fail("image parent affine transform disagrees with rendering/picking");
            auto natural = scene.image(image, 0, 0);
            if (natural->imageSize().x != 2 || natural->imageSize().y != 2 || natural->position().x != 1 ||
                scene.pick(1, 1) != natural)
                result = fail("image natural-size factory or top-left placement incorrect");
            natural->setSize(-1, 2);
            if (scene.pick(1, 1))
                result = fail("negative image size was pickable");
            scene.removeRoot(natural);
            natural.reset();
            auto cover = scene.rect(75, 10, 20, 30);
            cover->setFill(Color::Blue);
            cover->setZIndex(2);
            glClear(GL_COLOR_BUFFER_BIT);
            scene.render(canvas.renderer(), kFramebufferSize, kFramebufferSize);
            glFinish();
            if (scene.pick(83, 20) != cover || !pixelIs(readPixel(83, 20), 0, 0, 255))
                result = fail("shape/text batch order disagrees with z-index");
            cover->setZIndex(-2);
            cover->setFill(Color::Black);
            glClear(GL_COLOR_BUFFER_BIT);
            scene.render(canvas.renderer(), kFramebufferSize, kFramebufferSize);
            glFinish();
            if (scene.pick(83, 20) != label || readPixel(83, 20)[0] < 120)
                result = fail("text did not render/pick above an earlier shape");
            scene.removeRoot(cover);
            tile->setVisible(false);
            // Draw through the bound-scene overload and remove every font owner
            // before endFrame: the queued glyph batch must retain the font.
            glClear(GL_COLOR_BUFFER_BIT);
            canvas.beginFrame(kFramebufferSize, kFramebufferSize);
            scene.render();
            std::weak_ptr<Font> queuedFont = font;
            font.reset();
            label->setFont(nullptr);
            scene.removeRoot(label);
            label.reset();
            if (queuedFont.expired())
                result = fail("queued scene glyphs did not retain their font");
            canvas.endFrame();
            glFinish();
            if (!queuedFont.expired() || readPixel(83, 20)[0] < 120)
                result = fail("font release happened before draw or was retained after flush");
            tile->setVisible(true);
            if (scene.pick(imagePoint.x, imagePoint.y) != tile)
                result = fail("image visibility toggle did not restore picking");
            std::weak_ptr<Image> sharedImage = image;
            image.reset();
            if (sharedImage.expired())
                result = fail("image node did not retain its shared resource");
            tile->setImage(nullptr);
            if (!sharedImage.expired())
                result = fail("clearing image node leaked its resource");
            if (scene.pick(imagePoint.x, imagePoint.y))
                result = fail("missing image resource remained pickable");
            if (glGetError() != GL_NO_ERROR)
                result = fail("scene text/image rendering produced an OpenGL error");
        }

        if (result == 0)
        {
            using namespace vectorgl;
            auto font = std::make_shared<Font>();
            if (!font->load(VECTORGL_TEST_FONT, 16) || !canvas.setFont(VECTORGL_TEST_FONT, 16))
                result = fail("hinted rendering test font failed to load");
            const auto shaderMode = []
            {
                GLint program = 0, mode = 0;
                glGetIntegerv(GL_CURRENT_PROGRAM, &program);
                glGetUniformiv(static_cast<GLuint>(program), glGetUniformLocation(static_cast<GLuint>(program), "uSDF"),
                               &mode);
                return mode;
            };
            const auto draw = [&](TextRenderingMode mode, const Mat3x3& transform, bool snap = true)
            {
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                canvas.beginFrame(kFramebufferSize, kFramebufferSize);
                canvas.renderer().drawText(*font, "A", 10, 10, {1, 1, 1, 0.5f}, transform, {}, snap, mode);
                canvas.endFrame();
                glFinish();
                return shaderMode();
            };
            const int bitmapMode = font->hasBitmapSupport() ? 2 : 1;
            if (draw(TextRenderingMode::Auto, Mat3x3::identity()) != bitmapMode ||
                draw(TextRenderingMode::Sdf, Mat3x3::identity()) != 1 ||
                draw(TextRenderingMode::Bitmap, Mat3x3::identity()) != bitmapMode)
                result = fail("automatic/explicit text rendering mode incorrect");
            const auto center = readPixel(14, 21);
            if (center[0] < 115 || center[0] > 140 || std::abs(int(center[0]) - int(center[1])) > 2 ||
                std::abs(int(center[0]) - int(center[2])) > 2)
                result = fail("bitmap R8 coverage did not produce white text at requested alpha");
            if (draw(TextRenderingMode::Auto, Mat3x3::scaling(1.5f, 1.5f)) != bitmapMode ||
                draw(TextRenderingMode::Auto, Mat3x3::scaling(2, 2)) != 1 ||
                draw(TextRenderingMode::Bitmap, Mat3x3::scaling(2, 2)) != bitmapMode ||
                draw(TextRenderingMode::Bitmap, Mat3x3::scaling(5, 5)) != 1 ||
                draw(TextRenderingMode::Auto, Mat3x3::identity(), false) != 1 ||
                draw(TextRenderingMode::Bitmap, Mat3x3::rotation(0.2f)) != 1 ||
                draw(TextRenderingMode::Bitmap, Mat3x3::scaling(-1, 1)) != 1 ||
                draw(TextRenderingMode::Bitmap, Mat3x3::scaling(1, 2)) != 1)
                result = fail("bitmap mode did not account for framebuffer size or transform restrictions");
            canvas.beginFrame(kFramebufferSize, kFramebufferSize);
            canvas.setTextRenderingMode(TextRenderingMode::Sdf);
            canvas.save();
            canvas.setTextRenderingMode(TextRenderingMode::Bitmap);
            canvas.restore();
            canvas.setFillColor(Color::White);
            canvas.fillText("A", 10, 10);
            canvas.endFrame();
            if (shaderMode() != 1 || std::abs(canvas.measureText("AV") - 17.6f) > 0.001f)
                result = fail("Canvas save/restore lost rendering mode or changed layout metrics");
            if (font->hasBitmapSupport())
            {
                const auto* sdf = font->getGlyph('A');
                const auto* bitmap = font->getBitmapGlyph('A', 16);
                DrawArraySpy spy;
                canvas.beginFrame(kFramebufferSize, kFramebufferSize);
                for (const auto* glyph : {sdf, bitmap, sdf})
                    canvas.renderer().drawGlyph(10, 10, 20, 20, glyph->u0, glyph->v0, glyph->u1, glyph->v1,
                                                glyph->texture, Color::White, Mat3x3::identity(), glyph->sdf);
                canvas.endFrame();
                if (drawArrayCalls != 3)
                    result = fail("coverage/SDF mode transitions failed to split shared-atlas batches");
            }
            Scene scene(canvas.renderer());
            auto label = scene.text("A", 10, 10, font);
            label->setFill(Color::White);
            label->setTextRenderingMode(TextRenderingMode::Bitmap);
            label->setBlur(0);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            scene.render(canvas.renderer(), kFramebufferSize, kFramebufferSize);
            glFinish();
            if (readPixel(14, 21)[1] < 230 || readPixel(14, 100)[1] > 3)
                result = fail("scene bitmap text effect flipped or lost coverage");
            if (glGetError() != GL_NO_ERROR)
                result = fail("bitmap text rendering produced an OpenGL error");
            canvas.setTextRenderingMode(TextRenderingMode::Auto);
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
