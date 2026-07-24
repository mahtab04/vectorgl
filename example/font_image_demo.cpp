#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vectorgl/canvas.hpp>

// Demonstrates font rendering and image drawing.
// Usage: vectorgl_font_image [font.ttf] [image.png]
// If no arguments, renders procedural content showing text capabilities.
int main(int argc, char* argv[])
{
    if (!glfwInit())
    {
        std::cerr << "[vectorgl_font_image] Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "VectorGL - Font & Image", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "[vectorgl_font_image] Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (gladLoadGL(glfwGetProcAddress) == 0)
    {
        std::cerr << "[vectorgl_font_image] Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    vectorgl::Canvas canvas;
    try
    {
        canvas.init();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[vectorgl_font_image] Failed to initialize renderer: " << e.what() << "\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Try loading font from argument or common system font paths
    std::string fontPath;
    bool hasFont = false;
    if (argc >= 2)
    {
        fontPath = argv[1];
    }
    else
    {
        // Try common Windows font
        const char* candidates[] = {
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/consola.ttf",
            "C:/Windows/Fonts/calibri.ttf",
        };
        for (auto* path : candidates)
        {
            if (canvas.setFont(path, 32.0f))
            {
                fontPath = path;
                hasFont = true;
                break;
            }
        }
    }

    if (!hasFont && !fontPath.empty())
    {
        hasFont = canvas.setFont(fontPath, 32.0f);
    }

    if (!hasFont)
    {
        std::cerr << "No font loaded. Text rendering will be skipped.\n"
                  << "Usage: " << argv[0] << " [font.ttf] [image.png]\n";
    }

    // Try loading image
    vectorgl::Image image;
    bool hasImage = false;
    if (argc >= 3)
    {
        hasImage = image.load(argv[2]);
        if (!hasImage)
        {
            std::cerr << "Failed to load image: " << argv[2] << "\n";
        }
    }

    double startTime = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        float t = static_cast<float>(glfwGetTime() - startTime);

        glClearColor(0.06f, 0.06f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        canvas.beginFrame(fbW, fbH);

        // ============================================================
        // TEXT RENDERING SHOWCASE
        // ============================================================
        if (hasFont)
        {
            // Title
            canvas.setFont(fontPath, 48.0f);
            canvas.setFillColor(vectorgl::Color{1.0f, 1.0f, 1.0f, 1.0f});
            canvas.fillText("VectorGL - SDF Text Rendering", 40, 30);

            // Subtitle with different size
            canvas.setFont(fontPath, 24.0f);
            canvas.setFillColor(vectorgl::Color{0.6f, 0.7f, 0.9f, 1.0f});
            canvas.fillText("Signed Distance Field font atlas * Resolution independent * Smooth at any scale", 40, 90);

            // Multiple font sizes in a column
            float y = 140;
            int sizes[] = {12, 16, 20, 24, 32, 40, 56};
            for (int sz : sizes)
            {
                canvas.setFont(fontPath, static_cast<float>(sz));
                float brightness = 0.5f + 0.5f * (static_cast<float>(sz) / 56.0f);
                canvas.setFillColor(vectorgl::Color{brightness, brightness, brightness, 1.0f});
                canvas.fillText("The quick brown fox jumps - " + std::to_string(sz) + "px", 40, y);
                y += static_cast<float>(sz) + 10;
            }

            // Colored text labels on cards
            struct LabelCard
            {
                float x, y;
                vectorgl::Color bg, fg;
                const char* text;
            };
            LabelCard labelCards[] = {
                {40, 500, {0.15f, 0.4f, 0.9f, 1.0f}, {1, 1, 1, 1}, "Primary"},
                {230, 500, {0.1f, 0.7f, 0.4f, 1.0f}, {1, 1, 1, 1}, "Success"},
                {420, 500, {0.9f, 0.3f, 0.2f, 1.0f}, {1, 1, 1, 1}, "Danger"},
                {610, 500, {0.95f, 0.7f, 0.1f, 1.0f}, {0.1f, 0.1f, 0.1f, 1}, "Warning"},
            };

            canvas.setFont(fontPath, 20.0f);
            for (auto& card : labelCards)
            {
                float pulse = 0.97f + 0.03f * std::sin(t * 2 + card.x * 0.01f);
                canvas.save();
                canvas.translate(card.x + 80, card.y + 30);
                canvas.scale(pulse, pulse);
                canvas.setFillColor(card.bg);
                canvas.fillRoundedRect(-80, -30, 160, 60, 10);
                canvas.setFillColor(card.fg);
                canvas.fillText(card.text, -35, -12);
                canvas.restore();
            }

            // Animated typewriter text
            canvas.setFont(fontPath, 28.0f);
            std::string typewriter = "GPU-accelerated vector graphics...";
            int visibleChars = static_cast<int>(t * 4) % (static_cast<int>(typewriter.size()) + 20);
            visibleChars = std::min(visibleChars, static_cast<int>(typewriter.size()));
            canvas.setFillColor(vectorgl::Color{0.3f, 1.0f, 0.6f, 1.0f});
            canvas.fillText(typewriter.substr(0, visibleChars), 40, 600);

            // Blinking cursor
            if (static_cast<int>(t * 2) % 2 == 0 && visibleChars < static_cast<int>(typewriter.size()))
            {
                float cursorX = 40.0f + visibleChars * 16.0f; // Approximate
                canvas.setFillColor(vectorgl::Color{0.3f, 1.0f, 0.6f, 1.0f});
                canvas.fillRect(cursorX, 600, 2, 30);
            }

            // Rotating text
            canvas.save();
            canvas.translate(1100, 350);
            canvas.rotate(t * 0.3f);
            canvas.setFont(fontPath, 20.0f);
            canvas.setFillColor(vectorgl::Color{1.0f, 0.8f, 0.3f, 0.9f});
            canvas.fillText("Rotating!", -40, -10);
            canvas.restore();
        }
        else
        {
            // No font available — show placeholder shapes
            canvas.setFillColor(vectorgl::Color{1.0f, 0.5f, 0.5f, 1.0f});
            canvas.fillRoundedRect(40, 30, 500, 60, 8);
            canvas.setFillColor(vectorgl::Color{0.5f, 0.5f, 0.5f, 1.0f});
            canvas.fillRect(60, 50, 460, 20);
        }

        // ============================================================
        // IMAGE RENDERING SHOWCASE
        // ============================================================
        if (hasImage)
        {
            // Original size
            canvas.drawImage(image, 800, 100);

            // Scaled versions
            canvas.drawImage(image, 800, 350, 200, 150);
            canvas.drawImage(image, 1020, 350, 100, 100);

            // With rotation
            canvas.save();
            canvas.translate(900, 550);
            canvas.rotate(std::sin(t) * 0.2f);
            canvas.drawImage(image, -75, -50, 150, 100);
            canvas.restore();
        }
        else
        {
            // Placeholder for image area
            canvas.setStrokeColor(vectorgl::Color{0.4f, 0.4f, 0.5f, 1.0f});
            canvas.setLineWidth(2.0f);
            canvas.strokeRect(800, 100, 200, 150);

            // Draw an X to indicate missing image
            canvas.beginPath();
            canvas.moveTo(800, 100);
            canvas.lineTo(1000, 250);
            canvas.moveTo(1000, 100);
            canvas.lineTo(800, 250);
            canvas.stroke();

            if (hasFont)
            {
                canvas.setFont(fontPath, 16.0f);
                canvas.setFillColor(vectorgl::Color{0.5f, 0.5f, 0.6f, 1.0f});
                canvas.fillText("Pass image path as 2nd arg", 810, 270);
            }
        }

        // ============================================================
        // Decorative shapes to fill the scene
        // ============================================================
        for (int i = 0; i < 6; ++i)
        {
            float cx = 850.0f + 70.0f * std::cos(t * 0.5f + i * 1.05f);
            float cy = 500.0f + 70.0f * std::sin(t * 0.5f + i * 1.05f);
            float alpha = 0.3f + 0.3f * std::sin(t + i);
            canvas.setFillColor(vectorgl::Color{0.4f, 0.6f, 1.0f, alpha});
            canvas.fillCircle(cx, cy, 8);
        }

        canvas.endFrame();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    image.destroy();
    canvas.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
