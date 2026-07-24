#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <cmath>
#include <iostream>
#include <vectorgl/canvas.hpp>

// Demonstrates complex path drawing: bezier curves, arcs, stars, gears, waves
int main()
{
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "VectorGL - Path Drawing", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (gladLoadGL(glfwGetProcAddress) == 0)
        return 1;

    vectorgl::Canvas canvas;
    canvas.init();

    double startTime = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        float t = static_cast<float>(glfwGetTime() - startTime);

        glClearColor(0.02f, 0.02f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        canvas.beginFrame(fbW, fbH);

        // --- 1. Animated Bezier Flower ---
        canvas.save();
        canvas.translate(200, 200);
        canvas.rotate(t * 0.3f);
        int petals = 6;
        for (int i = 0; i < petals; ++i)
        {
            float angle = 2.0f * 3.14159f * i / petals;
            float hue = static_cast<float>(i) / petals;
            canvas.setFillColor(vectorgl::Color{0.5f + 0.5f * std::cos(hue * 6.28f),
                                                0.5f + 0.5f * std::cos(hue * 6.28f + 2.09f),
                                                0.5f + 0.5f * std::cos(hue * 6.28f + 4.18f), 0.7f});

            canvas.beginPath();
            canvas.moveTo(0, 0);
            float r = 80.0f + 10.0f * std::sin(t * 2 + i);
            float cx1 = r * std::cos(angle - 0.3f);
            float cy1 = r * std::sin(angle - 0.3f);
            float cx2 = r * std::cos(angle + 0.3f);
            float cy2 = r * std::sin(angle + 0.3f);
            float ex = r * 0.7f * std::cos(angle);
            float ey = r * 0.7f * std::sin(angle);
            canvas.bezierCurveTo(cx1, cy1, cx2, cy2, ex, ey);
            canvas.closePath();
            canvas.fill();
        }
        // Center circle
        canvas.setFillColor(vectorgl::Color{1.0f, 0.9f, 0.3f, 1.0f});
        canvas.fillCircle(0, 0, 15);
        canvas.restore();

        // --- 2. Star polygon ---
        canvas.save();
        canvas.translate(500, 200);
        canvas.rotate(-t * 0.5f);
        int points = 5;
        float outerR = 80, innerR = 35;
        canvas.beginPath();
        for (int i = 0; i <= points * 2; ++i)
        {
            float angle = 3.14159f * i / points - 3.14159f / 2;
            float r = (i % 2 == 0) ? outerR : innerR;
            float px = r * std::cos(angle);
            float py = r * std::sin(angle);
            if (i == 0)
                canvas.moveTo(px, py);
            else
                canvas.lineTo(px, py);
        }
        canvas.closePath();
        canvas.setFillColor(vectorgl::Color{1.0f, 0.6f, 0.1f, 0.9f});
        canvas.fill();
        canvas.setStrokeColor(vectorgl::Color{1.0f, 1.0f, 1.0f, 0.8f});
        canvas.setLineWidth(2.0f);
        canvas.stroke();
        canvas.restore();

        // --- 3. Gear shape ---
        canvas.save();
        canvas.translate(800, 200);
        canvas.rotate(t * 0.8f);
        int teeth = 12;
        float gearOuter = 70, gearInner = 55, toothDepth = 15;
        canvas.beginPath();
        for (int i = 0; i < teeth * 2; ++i)
        {
            float angle = 3.14159f * i / teeth;
            float r = (i % 2 == 0) ? gearOuter + toothDepth : gearInner;
            float px = r * std::cos(angle);
            float py = r * std::sin(angle);
            if (i == 0)
                canvas.moveTo(px, py);
            else
                canvas.lineTo(px, py);
        }
        canvas.closePath();
        canvas.setFillColor(vectorgl::Color{0.4f, 0.4f, 0.5f, 1.0f});
        canvas.fill();
        canvas.setStrokeColor(vectorgl::Color{0.7f, 0.7f, 0.8f, 1.0f});
        canvas.setLineWidth(1.5f);
        canvas.stroke();
        // Inner hole
        canvas.setFillColor(vectorgl::Color{0.02f, 0.02f, 0.06f, 1.0f});
        canvas.fillCircle(0, 0, 20);
        canvas.restore();

        // --- 4. Animated sine wave ---
        canvas.save();
        canvas.translate(0, 450);
        canvas.setStrokeColor(vectorgl::Color{0.2f, 0.8f, 1.0f, 0.9f});
        canvas.setLineWidth(3.0f);
        canvas.beginPath();
        for (int i = 0; i <= 200; ++i)
        {
            float x = static_cast<float>(i) * 6.4f;
            float y = 50.0f * std::sin(x * 0.02f + t * 3) * std::cos(x * 0.005f + t * 0.5f);
            if (i == 0)
                canvas.moveTo(x, y);
            else
                canvas.lineTo(x, y);
        }
        canvas.stroke();

        // Second wave (offset)
        canvas.setStrokeColor(vectorgl::Color{1.0f, 0.3f, 0.6f, 0.7f});
        canvas.setLineWidth(2.0f);
        canvas.beginPath();
        for (int i = 0; i <= 200; ++i)
        {
            float x = static_cast<float>(i) * 6.4f;
            float y = 40.0f * std::sin(x * 0.025f + t * 2 + 1) * std::sin(x * 0.01f - t);
            if (i == 0)
                canvas.moveTo(x, y);
            else
                canvas.lineTo(x, y);
        }
        canvas.stroke();
        canvas.restore();

        // --- 5. Spirograph ---
        canvas.save();
        canvas.translate(1050, 200);
        canvas.setStrokeColor(vectorgl::Color{0.5f, 1.0f, 0.5f, 0.8f});
        canvas.setLineWidth(1.5f);
        canvas.beginPath();
        float R = 60, rr = 25, d = 50;
        int steps = 300;
        for (int i = 0; i <= steps; ++i)
        {
            float theta = static_cast<float>(i) * 0.1f + t * 0.5f;
            float x = (R - rr) * std::cos(theta) + d * std::cos((R - rr) / rr * theta);
            float y = (R - rr) * std::sin(theta) - d * std::sin((R - rr) / rr * theta);
            if (i == 0)
                canvas.moveTo(x, y);
            else
                canvas.lineTo(x, y);
        }
        canvas.stroke();
        canvas.restore();

        // --- 6. Rounded rect dashboard cards ---
        for (int i = 0; i < 4; ++i)
        {
            float cx = 200.0f + i * 280.0f;
            float cy = 620.0f;
            float pulse = 0.95f + 0.05f * std::sin(t * 2 + i * 1.5f);

            canvas.save();
            canvas.translate(cx, cy);
            canvas.scale(pulse, pulse);

            // Card background
            canvas.setFillColor(vectorgl::Color{0.1f + 0.05f * i, 0.12f + 0.03f * i, 0.2f + 0.05f * i, 0.9f});
            canvas.fillRoundedRect(-120, -40, 240, 80, 12);

            // Accent bar
            float hue = static_cast<float>(i) / 4.0f;
            canvas.setFillColor(vectorgl::Color{0.5f + 0.5f * std::cos(hue * 6.28f),
                                                0.5f + 0.5f * std::cos(hue * 6.28f + 2.09f),
                                                0.5f + 0.5f * std::cos(hue * 6.28f + 4.18f), 1.0f});
            canvas.fillRoundedRect(-120, -40, 6, 80, 3);

            canvas.restore();
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
