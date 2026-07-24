#include "editor_app.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include <vectorgl/canvas.hpp>

static constexpr int WIN_W = 1280;
static constexpr int WIN_H = 950;

namespace
{

vectorgl::node_editor::EditorApp* gApp = nullptr;

void onScroll(GLFWwindow* window, double xoffset, double yoffset)
{
    (void)window;
    if (gApp != nullptr)
    {
        gApp->input().handleScroll(gApp->state(), static_cast<float>(xoffset), static_cast<float>(yoffset));
    }
}

} // namespace

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

    GLFWwindow* window = glfwCreateWindow(WIN_W, WIN_H, "VectorGL - Node Editor Demo", nullptr, nullptr);
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

    vectorgl::Canvas canvas;
    canvas.init();

    const char* fontPaths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf",
    };
    for (auto* fontPath : fontPaths)
    {
        if (canvas.setFont(fontPath, 18.0f))
            break;
    }

    vectorgl::node_editor::EditorApp app;
    gApp = &app;
    app.init();
    glfwSetScrollCallback(window, onScroll);

    double lastTime = glfwGetTime();
    bool wasLeftDown = false;
    bool wasMiddleDown = false;
    bool wasDeleteDown = false;
    while (!glfwWindowShouldClose(window))
    {
        int fbW = 0;
        int fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);

        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(window, &cursorX, &cursorY);

        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        app.input().beginFrame(app.state());
        app.input().handlePointerMove(app.state(), app.model(), static_cast<float>(cursorX), static_cast<float>(cursorY));

        const bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (leftDown && !wasLeftDown)
        {
            app.input().handlePointerDown(app.state(), app.model(), static_cast<float>(cursorX), static_cast<float>(cursorY),
                                          GLFW_MOUSE_BUTTON_LEFT);
        }
        else if (!leftDown && wasLeftDown)
        {
            app.input().handlePointerUp(app.state(), app.model(), static_cast<float>(cursorX), static_cast<float>(cursorY),
                                        GLFW_MOUSE_BUTTON_LEFT);
        }
        wasLeftDown = leftDown;

        const bool middleDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
        if (middleDown && !wasMiddleDown)
        {
            app.input().handlePointerDown(app.state(), app.model(), static_cast<float>(cursorX), static_cast<float>(cursorY),
                                          GLFW_MOUSE_BUTTON_MIDDLE);
        }
        else if (!middleDown && wasMiddleDown)
        {
            app.input().handlePointerUp(app.state(), app.model(), static_cast<float>(cursorX), static_cast<float>(cursorY),
                                        GLFW_MOUSE_BUTTON_MIDDLE);
        }
        wasMiddleDown = middleDown;

                const bool deleteDown = glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS ||
                    glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
                if (deleteDown && !wasDeleteDown)
                {
                        app.input().deleteSelected(app.model(), app.state());
                }
                wasDeleteDown = deleteDown;

        app.update(deltaTime);

        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        canvas.beginFrame(fbW, fbH);
        app.render(canvas);
        canvas.endFrame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    app.shutdown();
    gApp = nullptr;
    canvas.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
