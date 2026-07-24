#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <cmath>
#include <iostream>
#include <vectorgl/animator.hpp>
#include <vectorgl/canvas.hpp>
#include <vectorgl/scene.hpp>

// Demonstrates the animation system: springs, tweens, keyframes, staggered motion
int main()
{
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "VectorGL - Animation Showcase", nullptr, nullptr);
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

    vectorgl::Scene scene(canvas.renderer());
    auto& animator = scene.animator();

    // --- Row 1: Spring-animated cards sliding in from the left ---
    std::vector<std::shared_ptr<vectorgl::Node>> cards;
    for (int i = 0; i < 5; ++i)
    {
        auto card = scene.roundedRect(-200, 80 + i * 0, 180, 100, 10);
        card->style().fillColor = vectorgl::Color{0.2f + 0.15f * i, 0.3f, 0.8f - 0.1f * i, 0.95f};
        card->style().strokeColor = vectorgl::Color{1, 1, 1, 0.3f};
        card->style().strokeWidth = 1.0f;
        cards.push_back(card);

        // Spring to final position with staggered timing
        vectorgl::AnimTarget target;
        target.x = 150.0f + i * 220.0f;
        target.mask = vectorgl::AnimTarget::PosX;
        animator.emplace<vectorgl::SpringAnimation>(card->id(), target, 80.0f + i * 10.0f, 10.0f + i * 1.0f);
    }

    // --- Row 2: Bouncing circles with easing variety ---
    struct EaseDemo
    {
        vectorgl::Ease ease;
        const char* name;
        vectorgl::Color color;
    };
    EaseDemo easings[] = {
        {vectorgl::Ease::InOutCubic, "InOutCubic", {1.0f, 0.4f, 0.4f, 1.0f}},
        {vectorgl::Ease::OutBounce, "OutBounce", {0.4f, 1.0f, 0.4f, 1.0f}},
        {vectorgl::Ease::OutElastic, "OutElastic", {0.4f, 0.4f, 1.0f, 1.0f}},
        {vectorgl::Ease::InOutBack, "InOutBack", {1.0f, 0.8f, 0.2f, 1.0f}},
        {vectorgl::Ease::OutExpo, "OutExpo", {0.8f, 0.2f, 1.0f, 1.0f}},
    };

    for (int i = 0; i < 5; ++i)
    {
        float cx = 150.0f + i * 250.0f;
        auto ball = scene.circle(cx, 250, 25);
        ball->style().fillColor = easings[i].color;

        vectorgl::AnimTarget from, to;
        from.y = 250.0f;
        from.mask = vectorgl::AnimTarget::PosY;
        to.y = 450.0f;
        to.mask = vectorgl::AnimTarget::PosY;
        animator.emplace<vectorgl::TweenAnimation>(ball->id(), from, to, 2.0f, easings[i].ease,
                               vectorgl::LoopMode::PingPong);
    }

    // --- Row 3: Rotating group of shapes ---
    auto orbitGroup = scene.group();
    float orbitRadius = 100.0f;
    int orbitCount = 8;
    std::vector<std::shared_ptr<vectorgl::Node>> orbiters;
    for (int i = 0; i < orbitCount; ++i)
    {
        float angle = 2.0f * 3.14159f * i / orbitCount;
        float x = 640.0f + orbitRadius * std::cos(angle);
        float y = 580.0f + orbitRadius * std::sin(angle);
        auto shape = (i % 2 == 0) ? scene.roundedRect(x - 15, y - 15, 30, 30, 6) : scene.circle(x, y, 15);
        float t = static_cast<float>(i) / orbitCount;
        shape->style().fillColor =
            vectorgl::Color{0.5f + 0.5f * std::sin(t * 6.28f), 0.5f + 0.5f * std::sin(t * 6.28f + 2.09f),
                            0.5f + 0.5f * std::sin(t * 6.28f + 4.18f), 0.9f};
        orbitGroup->addChild(shape);
        orbiters.push_back(shape);
    }

    // --- Row 4: Opacity pulsing grid ---
    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 10; ++col)
        {
            float x = 100.0f + col * 60.0f;
            float y = 520.0f + row * 60.0f;
            auto dot = scene.roundedRect(x, y, 40, 40, 8);
            float hue = (row * 10 + col) / 30.0f;
            dot->style().fillColor =
                vectorgl::Color{0.5f + 0.5f * std::cos(hue * 6.28f), 0.5f + 0.5f * std::cos(hue * 6.28f + 2.09f),
                                0.5f + 0.5f * std::cos(hue * 6.28f + 4.18f), 1.0f};

            vectorgl::AnimTarget from, to;
            from.opacity = 0.2f;
            from.mask = vectorgl::AnimTarget::Opacity;
            to.opacity = 1.0f;
            to.mask = vectorgl::AnimTarget::Opacity;
            float duration = 1.0f + 0.5f * std::sin((row * 10 + col) * 0.5f);
            animator.emplace<vectorgl::TweenAnimation>(dot->id(), from, to, duration, vectorgl::Ease::InOutQuad,
                                                       vectorgl::LoopMode::PingPong);
        }
    }

    double lastTime = glfwGetTime();
    float totalTime = 0;

    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;
        totalTime += dt;

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);

        glClearColor(0.03f, 0.03f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Manually rotate orbit shapes each frame
        for (int i = 0; i < orbitCount; ++i)
        {
            float angle = 2.0f * 3.14159f * i / orbitCount + totalTime * 1.2f;
            float x = 640.0f + orbitRadius * std::cos(angle);
            float y = 580.0f + orbitRadius * std::sin(angle);
            orbiters[i]->setPosition(x, y);
        }

        scene.update(dt);

        canvas.beginFrame(fbW, fbH);
        scene.render();
        canvas.endFrame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    canvas.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
