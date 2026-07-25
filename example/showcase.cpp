#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include <vectorgl/canvas.hpp>

namespace
{

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 680;
constexpr float kPi = 3.14159265f;

using vectorgl::Canvas;
using vectorgl::Color;

void drawGlow(Canvas& canvas, float centerX, float centerY, float radius, Color color)
{
    for (int layer = 7; layer >= 0; --layer)
    {
        const float spread = static_cast<float>(layer) * 9.0f;
        color.a = 0.025f + static_cast<float>(7 - layer) * 0.018f;
        canvas.setFillColor(color);
        canvas.fillCircle(centerX, centerY, radius + spread);
    }
}

void drawGrid(Canvas& canvas)
{
    canvas.setStrokeColor({0.20f, 0.27f, 0.40f, 0.18f});
    canvas.setLineWidth(1.0f);
    for (int x = 40; x < kWindowWidth; x += 40)
    {
        canvas.beginPath();
        canvas.moveTo(static_cast<float>(x), 0.0f);
        canvas.lineTo(static_cast<float>(x), static_cast<float>(kWindowHeight));
        canvas.stroke();
    }
    for (int y = 40; y < kWindowHeight; y += 40)
    {
        canvas.beginPath();
        canvas.moveTo(0.0f, static_cast<float>(y));
        canvas.lineTo(static_cast<float>(kWindowWidth), static_cast<float>(y));
        canvas.stroke();
    }
}

void drawWordmark(Canvas& canvas, bool hasFont)
{
    canvas.setFillColor({0.10f, 0.82f, 0.96f, 1.0f});
    canvas.fillRoundedRect(54.0f, 44.0f, 42.0f, 42.0f, 12.0f);
    canvas.setStrokeColor({0.02f, 0.08f, 0.15f, 1.0f});
    canvas.setLineWidth(4.0f);
    canvas.beginPath();
    canvas.moveTo(65.0f, 58.0f);
    canvas.lineTo(75.0f, 75.0f);
    canvas.lineTo(86.0f, 56.0f);
    canvas.stroke();

    if (hasFont)
    {
        canvas.setFillColor({0.93f, 0.96f, 1.0f, 1.0f});
        canvas.fillText("VECTORGL", 112.0f, 70.0f);
        canvas.setFillColor({0.48f, 0.57f, 0.72f, 1.0f});
        canvas.fillText("GPU VECTOR ENGINE", 1010.0f, 70.0f);
    }
}

void drawHeroCopy(Canvas& canvas, bool hasFont)
{
    canvas.setFillColor({0.08f, 0.72f, 0.95f, 0.14f});
    canvas.fillRoundedRect(58.0f, 133.0f, 220.0f, 34.0f, 17.0f);
    canvas.setFillColor({0.15f, 0.85f, 0.98f, 1.0f});
    canvas.fillRoundedRect(58.0f, 374.0f, 226.0f, 48.0f, 12.0f);
    canvas.setStrokeColor({0.42f, 0.91f, 1.0f, 0.75f});
    canvas.setLineWidth(1.5f);
    canvas.strokeRoundedRect(58.0f, 374.0f, 226.0f, 48.0f, 12.0f);

    if (hasFont)
    {
        canvas.setFillColor({0.33f, 0.86f, 1.0f, 1.0f});
        canvas.fillText("OPENGL 3.3 / C++20", 76.0f, 139.0f);

        canvas.setFillColor({0.95f, 0.97f, 1.0f, 1.0f});
        canvas.fillText("FAST 2D GRAPHICS.", 58.0f, 220.0f);
        canvas.fillText("BEAUTIFULLY SIMPLE.", 58.0f, 258.0f);

        canvas.setFillColor({0.54f, 0.62f, 0.76f, 1.0f});
        canvas.fillText("SDF primitives, paths, text, SVG and animation", 58.0f, 315.0f);
        canvas.fillText("through one expressive canvas API.", 58.0f, 345.0f);

        canvas.setFillColor({0.03f, 0.09f, 0.15f, 1.0f});
        canvas.fillText("EXPLORE VECTORGL", 83.0f, 387.0f);
    }
}

void drawOrbitalArtwork(Canvas& canvas, float time)
{
    constexpr float centerX = 880.0f;
    constexpr float centerY = 277.0f;
    drawGlow(canvas, centerX, centerY, 100.0f, {0.04f, 0.76f, 1.0f, 1.0f});

    canvas.setFillColor({0.055f, 0.075f, 0.13f, 1.0f});
    canvas.fillCircle(centerX, centerY, 155.0f);
    canvas.setStrokeColor({0.20f, 0.73f, 0.96f, 0.35f});
    canvas.setLineWidth(1.5f);
    canvas.strokeCircle(centerX, centerY, 155.0f);
    canvas.strokeCircle(centerX, centerY, 112.0f);

    for (int index = 0; index < 28; ++index)
    {
        const float phase = static_cast<float>(index) / 28.0f;
        const float angle = phase * kPi * 2.0f + time * 0.12f;
        const float radius = 112.0f + 22.0f * std::sin(phase * kPi * 6.0f + time * 0.3f);
        const float x = centerX + std::cos(angle) * radius;
        const float y = centerY + std::sin(angle) * radius;
        canvas.setFillColor(Color::lerpOklab({0.08f, 0.85f, 1.0f, 0.9f}, {0.60f, 0.24f, 1.0f, 0.9f}, phase));
        canvas.fillCircle(x, y, 3.0f + 3.0f * std::abs(std::sin(angle * 2.0f)));
    }

    canvas.save();
    canvas.translate(centerX, centerY);
    canvas.rotate(time * 0.18f);
    canvas.setFillColor({0.12f, 0.80f, 1.0f, 0.90f});
    canvas.fillRoundedRect(-74.0f, -74.0f, 148.0f, 148.0f, 35.0f);
    canvas.rotate(-time * 0.32f);
    canvas.setFillColor({0.32f, 0.12f, 0.70f, 0.88f});
    canvas.fillRoundedRect(-53.0f, -53.0f, 106.0f, 106.0f, 28.0f);
    canvas.setFillColor({0.92f, 0.97f, 1.0f, 1.0f});
    canvas.fillCircle(0.0f, 0.0f, 24.0f);
    canvas.restore();

    canvas.setStrokeColor({0.32f, 0.92f, 1.0f, 0.80f});
    canvas.setLineWidth(3.0f);
    canvas.beginPath();
    canvas.moveTo(1052.0f, 190.0f);
    canvas.bezierCurveTo(1115.0f, 215.0f, 1100.0f, 340.0f, 1163.0f, 365.0f);
    canvas.stroke();
}

void drawFeatureCard(Canvas& canvas, float x, const std::string& number, const std::string& title,
                     const std::string& detail, Color accent, bool hasFont)
{
    canvas.setFillColor({0.055f, 0.072f, 0.12f, 0.96f});
    canvas.fillRoundedRect(x, 470.0f, 270.0f, 170.0f, 18.0f);
    canvas.setStrokeColor({0.22f, 0.30f, 0.46f, 0.50f});
    canvas.setLineWidth(1.0f);
    canvas.strokeRoundedRect(x, 470.0f, 270.0f, 170.0f, 18.0f);

    accent.a = 0.14f;
    canvas.setFillColor(accent);
    canvas.fillCircle(x + 44.0f, 512.0f, 24.0f);
    accent.a = 1.0f;
    canvas.setStrokeColor(accent);
    canvas.setLineWidth(2.5f);
    canvas.strokeCircle(x + 44.0f, 512.0f, 24.0f);

    if (hasFont)
    {
        canvas.setFillColor(accent);
        canvas.fillText(number, x + 34.0f, 518.0f);
        canvas.setFillColor({0.92f, 0.95f, 1.0f, 1.0f});
        canvas.fillText(title, x + 24.0f, 570.0f);
        canvas.setFillColor({0.45f, 0.54f, 0.68f, 1.0f});
        canvas.fillText(detail, x + 24.0f, 605.0f);
    }
}

bool loadSystemFont(Canvas& canvas)
{
    const char* paths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/SFNS.ttf",
    };
    for (const char* path : paths)
    {
        if (canvas.setFont(path, 18.0f))
            return true;
    }
    return false;
}

} // namespace

int main()
{
    if (glfwInit() != GLFW_TRUE)
    {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "VectorGL Showcase", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create the showcase window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (gladLoadGL(glfwGetProcAddress) == 0)
    {
        std::cerr << "Failed to load OpenGL\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    Canvas canvas;
    canvas.init();
    const bool hasFont = loadSystemFont(canvas);

    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        const float time = static_cast<float>(glfwGetTime());

        glViewport(0, 0, width, height);
        glClearColor(0.018f, 0.026f, 0.050f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        canvas.beginFrame(width, height);
        drawGrid(canvas);
        drawGlow(canvas, 1120.0f, 60.0f, 70.0f, {0.52f, 0.18f, 1.0f, 1.0f});
        drawWordmark(canvas, hasFont);
        drawHeroCopy(canvas, hasFont);
        drawOrbitalArtwork(canvas, time);
        drawFeatureCard(canvas, 58.0f, "01", "SDF PRIMITIVES", "Sharp at every scale", {0.08f, 0.84f, 1.0f, 1.0f}, hasFont);
        drawFeatureCard(canvas, 350.0f, "02", "EXPRESSIVE PATHS", "Canvas-style drawing", {0.46f, 0.36f, 1.0f, 1.0f}, hasFont);
        drawFeatureCard(canvas, 642.0f, "03", "SVG + TEXT", "Production-ready assets", {0.98f, 0.32f, 0.62f, 1.0f}, hasFont);
        drawFeatureCard(canvas, 934.0f, "04", "GPU BATCHING", "Built for real-time UI", {0.22f, 0.92f, 0.58f, 1.0f}, hasFont);
        canvas.endFrame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    canvas.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
