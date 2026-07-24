#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <cmath>
#include <iostream>
#include <vectorgl/animator.hpp>
#include <vectorgl/canvas.hpp>
#include <vectorgl/scene.hpp>

int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "VectorGL - SDF Demo", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (gladLoadGL(glfwGetProcAddress) == 0)
    {
        std::cerr << "Failed to initialize GLAD\n";
        return 1;
    }

    // ============================================================
    // Immediate-mode Canvas (HTML5-style, SDF accelerated)
    // ============================================================
    vectorgl::Canvas canvas;
    canvas.init();

    // ============================================================
    // Retained-mode Scene Graph with Animations
    // ============================================================
    vectorgl::Scene scene(canvas.renderer());

    // Background
    auto bg = scene.rect(640, 360, 1280, 720);
    bg->style().fillColor = vectorgl::Color{0.05f, 0.05f, 0.1f, 1.0f};

    // Animated rounded rectangle
    auto card = scene.roundedRect(200, 300, 300, 200, 16);
    card->style().fillColor = vectorgl::Color{0.15f, 0.4f, 0.9f, 1.0f};
    card->style().strokeColor = vectorgl::Color{0.3f, 0.6f, 1.0f, 1.0f};
    card->style().strokeWidth = 2.0f;

    // Animated circle
    auto circle = scene.circle(640, 360, 60);
    circle->style().fillColor = vectorgl::Color{1.0f, 0.4f, 0.2f, 1.0f};

    // Animated ellipse
    auto ellipse = scene.ellipse(900, 300, 80, 50);
    ellipse->style().fillColor = vectorgl::Color{0.2f, 0.9f, 0.5f, 1.0f};
    ellipse->style().strokeColor = vectorgl::Color{1.0f, 1.0f, 1.0f, 0.5f};
    ellipse->style().strokeWidth = 1.5f;

    // Group of small circles (instancing showcase - 100 shapes in ONE draw call)
    auto group = scene.group();
    for (int i = 0; i < 100; ++i)
    {
        float angle = 2.0f * 3.14159f * i / 100.0f;
        float r = 150.0f + 30.0f * std::sin(angle * 3);
        float x = 640.0f + r * std::cos(angle);
        float y = 360.0f + r * std::sin(angle);
        auto dot = scene.circle(x, y, 4.0f + 3.0f * std::sin(angle * 5));
        float t = static_cast<float>(i) / 100.0f;
        dot->style().fillColor = vectorgl::Color{t, 1.0f - t, 0.5f + 0.5f * t, 0.8f};
        group->addChild(dot);
    }

    // Spring animation on the card
    {
        vectorgl::AnimTarget from, to;
        from.y = 300.0f;
        from.mask = vectorgl::AnimTarget::PosY;
        to.y = 360.0f;
        to.mask = vectorgl::AnimTarget::PosY;
        scene.animator().emplace<vectorgl::SpringAnimation>(card->id(), to, 120.0f, 12.0f);
    }

    // Tween animation on the circle (opacity ping-pong)
    {
        vectorgl::AnimTarget from, to;
        from.opacity = 0.3f;
        from.mask = vectorgl::AnimTarget::Opacity;
        to.opacity = 1.0f;
        to.mask = vectorgl::AnimTarget::Opacity;
        scene.animator().emplace<vectorgl::TweenAnimation>(circle->id(), from, to, 2.0f,
                                                           vectorgl::Ease::InOutCubic, vectorgl::LoopMode::PingPong);
    }

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);

        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Update scene (propagates animations + world transforms)
        scene.update(dt);

        // Render scene graph (all SDF shapes batched via instancing!)
        canvas.beginFrame(fbW, fbH);
        scene.render();

        // Also demonstrate immediate-mode drawing on top
        canvas.save();
        float t = static_cast<float>(now);
        canvas.translate(1000.0f, 500.0f);
        canvas.rotate(t * 0.5f);
        canvas.setFillColor(vectorgl::Color{0.9f, 0.8f, 0.2f, 0.9f});
        canvas.fillRoundedRect(-60, -40, 120, 80, 12);
        canvas.restore();

        canvas.endFrame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    canvas.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
