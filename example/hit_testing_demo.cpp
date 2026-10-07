#define GLFW_INCLUDE_NONE
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <vectorgl/canvas.hpp>
#include <vectorgl/scene.hpp>

using namespace vectorgl;

namespace
{
struct Editor
{
    Scene scene;
    std::shared_ptr<Node> selected;
    NodeStyle previousStyle;
    Vec2 grabOffset;
    std::vector<std::pair<std::shared_ptr<Node>, Vec2>> startingPositions;

    Editor()
    {
        auto card = scene.roundedRect(100, 100, 240, 160, 28);
        card->setFill({0.15f, 0.45f, 0.9f, 1});
        card->setRotation(-0.15f);
        auto circle = scene.circle(340, 210, 80);
        circle->setFill({0.95f, 0.35f, 0.2f, 1});
        auto ellipse = scene.ellipse(620, 170, 100, 55);
        ellipse->setFill({0.2f, 0.75f, 0.5f, 1});
        ellipse->setRotation(0.4f);
        auto outline = scene.roundedRect(540, 320, 210, 150, 24);
        outline->setStroke({0.7f, 0.5f, 1, 1}, 12);
        auto line = scene.line(80, 530, 390, 450);
        line->setStroke({0.3f, 0.8f, 0.9f, 1}, 14);
        auto group = scene.group();
        group->setPosition(270, 380);
        group->setRotation(0.45f);
        group->setScale(1.2f, 1.2f);
        auto child = scene.rect(-70, -30, 140, 60);
        child->setFill({1, 0.75f, 0.2f, 1});
        group->addChild(child);
        Path2D triangle;
        triangle.moveTo(-70, 60);
        triangle.lineTo(0, -60);
        triangle.lineTo(70, 60);
        triangle.closePath();
        auto path = scene.path(triangle);
        path->setPosition(850, 430);
        path->setFill({0.9f, 0.35f, 0.65f, 1});
        for (const auto& node : {card, circle, ellipse, outline, line, child, path})
            startingPositions.emplace_back(node, node->position());
    }

    bool parentPoint(Vec2 screen, Vec2& local) const
    {
        local = screen;
        if (!selected || !selected->parent())
            return true;
        const auto& m = selected->parent()->worldTransform().m;
        const float determinant = m[0] * m[4] - m[1] * m[3];
        if (determinant == 0 || !std::isfinite(determinant))
            return false;
        const float x = screen.x - m[6], y = screen.y - m[7];
        local = {(m[4] * x - m[3] * y) / determinant, (m[0] * y - m[1] * x) / determinant};
        return true;
    }

    void selectAt(Vec2 point)
    {
        if (selected)
            selected->style() = previousStyle;
        selected = scene.pick(point.x, point.y);
        if (!selected)
            return;
        previousStyle = selected->style();
        selected->setStroke(Color::White, std::max(4.0f, previousStyle.strokeWidth));
        Vec2 local;
        if (parentPoint(point, local))
            grabOffset = local - selected->position();
    }

    void dragTo(Vec2 point)
    {
        Vec2 local;
        if (selected && parentPoint(point, local))
        {
            local = local - grabOffset;
            selected->setPosition(local.x, local.y);
        }
    }
};

Vec2 cursorPoint(GLFWwindow* window)
{
    double x, y;
    int width, height, fbWidth, fbHeight;
    glfwGetCursorPos(window, &x, &y);
    glfwGetWindowSize(window, &width, &height);
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    // GLFW cursors use window units; the renderer uses framebuffer pixels.
    return {width > 0 ? static_cast<float>(x * fbWidth / width) : 0,
            height > 0 ? static_cast<float>(y * fbHeight / height) : 0};
}
} // namespace

int main(int argc, char** argv)
{
    const bool smoke = argc > 1 && std::string(argv[1]) == "--smoke-test";
    if (!glfwInit())
        return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (smoke)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1024, 640, "VectorGL - Click and drag shapes", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGL(glfwGetProcAddress))
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glfwSwapInterval(smoke ? 0 : 1);
    int result = 0;
    {
        Canvas canvas;
        canvas.init();
        Editor editor;
        glfwSetWindowUserPointer(window, &editor);
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow* w, int button, int action, int)
                                   {
                                       if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
                                           static_cast<Editor*>(glfwGetWindowUserPointer(w))->selectAt(cursorPoint(w));
                                   });
        glfwSetCursorPosCallback(window,
                                 [](GLFWwindow* w, double, double)
                                 {
                                     if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
                                         static_cast<Editor*>(glfwGetWindowUserPointer(w))->dragTo(cursorPoint(w));
                                 });
        glfwSetKeyCallback(window,
                           [](GLFWwindow* w, int key, int, int action, int)
                           {
                               if (action != GLFW_PRESS)
                                   return;
                               auto& editor = *static_cast<Editor*>(glfwGetWindowUserPointer(w));
                               if (key == GLFW_KEY_ESCAPE)
                                   glfwSetWindowShouldClose(w, GLFW_TRUE);
                               if (key == GLFW_KEY_R)
                                   for (const auto& [node, position] : editor.startingPositions)
                                   {
                                       node->setPosition(position.x, position.y);
                                       node->setZIndex(0);
                                   }
                               if (editor.selected && (key == GLFW_KEY_UP || key == GLFW_KEY_DOWN))
                                   editor.selected->setZIndex(editor.selected->zIndex() +
                                                              (key == GLFW_KEY_UP ? 1 : -1));
                           });
        std::cout << "Click painted areas to select; drag to move. Outline centers are empty.\n"
                     "Up/Down: change z-index. R: reset positions/order. Esc: exit.\n";
        if (smoke)
        {
            editor.selectAt({340, 210});
            auto circle = editor.startingPositions[1].first;
            if (editor.selected != circle)
                result = 1;
            editor.dragTo({380, 250});
            if (circle->position().x != 380 || circle->position().y != 250)
                result = 1;
            editor.selectAt({270, 380});
            auto child = editor.startingPositions[5].first;
            if (editor.selected != child)
                result = 1;
            editor.dragTo({290, 400});
            if (editor.scene.pick(290, 400) != child)
                result = 1;
        }
        int frames = 0;
        while (!glfwWindowShouldClose(window) && (!smoke || frames < 3))
        {
            glfwPollEvents();
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            if (width <= 0 || height <= 0)
                continue;
            editor.scene.update(0);
            glClearColor(0.06f, 0.07f, 0.1f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            editor.scene.render(canvas.renderer(), width, height);
            std::string title = "VectorGL - Click/drag | Up/Down: order | R: reset | Esc: exit";
            if (editor.selected)
                title += " | Selected " + std::to_string(editor.selected->id()) +
                         " z=" + std::to_string(editor.selected->zIndex());
            glfwSetWindowTitle(window, title.c_str());
            glfwSwapBuffers(window);
            ++frames;
        }
        if (smoke && glGetError() != GL_NO_ERROR)
            result = 1;
        canvas.destroy();
        glfwSetWindowUserPointer(window, nullptr);
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
