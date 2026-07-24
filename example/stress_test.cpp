#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <vectorgl/canvas.hpp>
#include <vectorgl/scene.hpp>

// Stress test: renders thousands of SDF shapes in a single instanced draw call
// This demonstrates the key architectural advantage over NanoVG/Cairo/Skia
int main()
{
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 1024, "VectorGL - Stress Test (10K shapes)", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Uncapped FPS for benchmarking

    if (gladLoadGL(glfwGetProcAddress) == 0)
        return 1;

    vectorgl::Canvas canvas;
    canvas.init();

    // Pre-generate shape data
    const int NUM_SHAPES = 20000;
    struct ShapeData
    {
        float x, y;
        float vx, vy;
        float size;
        float hue;
        int shapeType; // 0=rect, 1=circle, 2=roundedRect
    };

    std::vector<ShapeData> shapes(NUM_SHAPES);
    std::srand(42); // Deterministic
    for (auto& s : shapes)
    {
        s.x = static_cast<float>(std::rand() % 1280);
        s.y = static_cast<float>(std::rand() % 720);
        s.vx = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * 200.0f;
        s.vy = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * 200.0f;
        s.size = 3.0f + static_cast<float>(std::rand() % 15);
        s.hue = static_cast<float>(std::rand()) / RAND_MAX;
        s.shapeType = std::rand() % 3;
    }

    double lastTime = glfwGetTime();
    int frameCount = 0;
    double fpsTimer = 0;
    float displayFps = 0;

    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        // FPS counter
        frameCount++;
        fpsTimer += dt;
        if (fpsTimer >= 1.0)
        {
            displayFps = static_cast<float>(frameCount) / static_cast<float>(fpsTimer);
            std::cout << "FPS: " << displayFps << " | Shapes: " << NUM_SHAPES
                      << " | ms/frame: " << (1000.0f / displayFps) << "\n";
            frameCount = 0;
            fpsTimer = 0;
        }

        // Update positions (bounce off walls)
        for (auto& s : shapes)
        {
            s.x += s.vx * dt;
            s.y += s.vy * dt;
            if (s.x < 0 || s.x > 1280)
            {
                s.vx = -s.vx;
                s.x = std::max(0.0f, std::min(1280.0f, s.x));
            }
            if (s.y < 0 || s.y > 720)
            {
                s.vy = -s.vy;
                s.y = std::max(0.0f, std::min(720.0f, s.y));
            }
        }

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);

        glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        canvas.beginFrame(fbW, fbH);

        // Draw all shapes using SDF (batched into very few draw calls via instancing)
        for (const auto& s : shapes)
        {
            float r = 0.5f + 0.5f * std::cos(s.hue * 6.28f);
            float g = 0.5f + 0.5f * std::cos(s.hue * 6.28f + 2.09f);
            float b = 0.5f + 0.5f * std::cos(s.hue * 6.28f + 4.18f);
            canvas.setFillColor(vectorgl::Color{r, g, b, 0.8f});

            switch (s.shapeType)
            {
            case 0:
                canvas.fillRect(s.x - s.size, s.y - s.size, s.size * 2, s.size * 2);
                break;
            case 1:
                canvas.fillCircle(s.x, s.y, s.size);
                break;
            case 2:
                canvas.fillRoundedRect(s.x - s.size, s.y - s.size, s.size * 2, s.size * 2, s.size * 0.3f);
                break;
            }
        }

        canvas.endFrame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    canvas.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
