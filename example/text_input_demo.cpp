#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#include <vectorgl/canvas.hpp>
#include <vectorgl/glfw_text_box.hpp>
#include <vectorgl/ui.hpp>

namespace
{

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr float kFieldX = 120.0f;
constexpr float kFieldY = 260.0f;
constexpr float kFieldWidth = 620.0f;
constexpr float kFieldHeight = 56.0f;
constexpr float kButtonX = 760.0f;
constexpr float kButtonY = 260.0f;
constexpr float kButtonWidth = 180.0f;
constexpr float kButtonHeight = 56.0f;

struct AppState
{
    vectorgl::Canvas canvas;
    vectorgl::ui::TextBox field;
    vectorgl::ui::Label titleLabel;
    vectorgl::ui::Label hintLabel;
    vectorgl::ui::Label inputLabel;
    vectorgl::ui::Label currentValueLabel;
    vectorgl::ui::Label statusLabel;
    vectorgl::ui::Button sampleButton;
    vectorgl::GlfwTextBoxController fieldInput;
    std::string fontPath;
    std::string statusText = "Edit text, select a range, and try the sample button.";
    bool hasFont = false;
};

void applySampleText(AppState& app)
{
    app.field.setText("VectorGL now supports selection, clipboard, and a tiny ui layer.");
    app.field.clearSelection();
    app.field.setFocused(false);
    app.statusText = "Sample text loaded.";
}

void onChar(GLFWwindow* window, unsigned int codepoint)
{
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!app)
        return;

    app->fieldInput.handleChar(codepoint);
}

void onKey(GLFWwindow* window, int key, int, int action, int mods)
{
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!app)
        return;

    app->fieldInput.handleKey(key, action, mods);
}

void onCursorPos(GLFWwindow* window, double x, double y)
{
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!app)
        return;

    app->fieldInput.handleCursorPos(x, y);
    app->sampleButton.setHovered(app->sampleButton.hitTest(static_cast<float>(x), static_cast<float>(y)));
}

void onMouseButton(GLFWwindow* window, int button, int action, int)
{
    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!app)
        return;

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(window, &cursorX, &cursorY);

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        app->sampleButton.handlePointerDown(static_cast<float>(cursorX), static_cast<float>(cursorY));

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE &&
        app->sampleButton.handlePointerUp(static_cast<float>(cursorX), static_cast<float>(cursorY)))
    {
        applySampleText(*app);
    }

    // Route the same pointer event to the text controller so clicks outside the field clear focus.
    app->fieldInput.handleMouseButton(button, action);
}

bool loadDefaultFont(AppState& app)
{
    const char* candidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/consola.ttf",
    };

    for (const char* candidate : candidates)
    {
        if (app.canvas.setFont(candidate, 24.0f))
        {
            app.fontPath = candidate;
            app.field.setFont(candidate, 24.0f);
            app.titleLabel.setFont(candidate, 34.0f);
            app.hintLabel.setFont(candidate, 18.0f);
            app.inputLabel.setFont(candidate, 16.0f);
            app.currentValueLabel.setFont(candidate, 18.0f);
            app.statusLabel.setFont(candidate, 18.0f);
            app.sampleButton.setFont(candidate, 18.0f);
            return true;
        }
    }

    return false;
}

} // namespace

int main()
{
    if (!glfwInit())
    {
        std::cerr << "[vectorgl_text_input] Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "VectorGL - Text Input", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "[vectorgl_text_input] Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (gladLoadGL(glfwGetProcAddress) == 0)
    {
        std::cerr << "[vectorgl_text_input] Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    AppState app;
    glfwSetWindowUserPointer(window, &app);

    app.canvas.init();
    app.titleLabel.setPosition(120.0f, 110.0f);
    app.titleLabel.setText("Interactive Text Input");
    app.titleLabel.style().textColor = vectorgl::Color{0.96f, 0.98f, 1.0f, 1.0f};

    app.hintLabel.setPosition(120.0f, 156.0f);
    app.hintLabel.setText("Click and drag to select. Use Ctrl+A/C/X/V plus arrows, Home, End, Backspace, Delete.");
    app.hintLabel.style().textColor = vectorgl::Color{0.66f, 0.74f, 0.86f, 1.0f};

    app.inputLabel.setPosition(kFieldX, kFieldY - 26.0f);
    app.inputLabel.setText("Input");
    app.inputLabel.style().textColor = vectorgl::Color{0.46f, 0.84f, 1.0f, 1.0f};

    app.currentValueLabel.setPosition(120.0f, 360.0f);
    app.currentValueLabel.setText("Current value:");
    app.currentValueLabel.style().textColor = vectorgl::Color{0.86f, 0.9f, 0.96f, 1.0f};

    app.statusLabel.setPosition(120.0f, 434.0f);
    app.statusLabel.style().textColor = vectorgl::Color{0.94f, 0.81f, 0.55f, 1.0f};

    app.field.setBounds(kFieldX, kFieldY, kFieldWidth, kFieldHeight);
    app.field.setText("VectorGL text input");
    app.field.setPlaceholder("Type here");

    app.sampleButton.setBounds(kButtonX, kButtonY, kButtonWidth, kButtonHeight);
    app.sampleButton.setText("Load Sample");
    app.sampleButton.style().fillColor = vectorgl::Color{0.14f, 0.24f, 0.34f, 1.0f};
    app.sampleButton.style().hoverFillColor = vectorgl::Color{0.18f, 0.31f, 0.44f, 1.0f};
    app.sampleButton.style().pressedFillColor = vectorgl::Color{0.10f, 0.19f, 0.28f, 1.0f};
    app.sampleButton.style().borderColor = vectorgl::Color{0.32f, 0.68f, 0.94f, 1.0f};

    app.fieldInput.bind(app.field, app.canvas, window);
    app.hasFont = loadDefaultFont(app);
    if (!app.hasFont)
    {
        std::cerr << "[vectorgl_text_input] Failed to load a system font\n";
        app.canvas.destroy();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    glfwSetCharCallback(window, onChar);
    glfwSetKeyCallback(window, onKey);
    glfwSetCursorPosCallback(window, onCursorPos);
    glfwSetMouseButtonCallback(window, onMouseButton);

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        const double now = glfwGetTime();
        const float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        glClearColor(0.04f, 0.06f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        app.canvas.beginFrame(fbWidth, fbHeight);
        app.canvas.setFillColor(vectorgl::Color{0.18f, 0.24f, 0.34f, 0.9f});
        app.canvas.fillRoundedRect(96.0f, 84.0f, 880.0f, 270.0f, 24.0f);
        app.canvas.setStrokeColor(vectorgl::Color{0.26f, 0.36f, 0.52f, 1.0f});
        app.canvas.setLineWidth(1.5f);
        app.canvas.strokeRoundedRect(96.0f, 84.0f, 880.0f, 270.0f, 24.0f);

        app.titleLabel.render(app.canvas);
        app.hintLabel.render(app.canvas);
        app.inputLabel.render(app.canvas);

        app.field.render(app.canvas, dt);
        app.sampleButton.render(app.canvas);

        if (app.field.consumeSubmit())
            app.statusText = "Submitted value: " + app.field.text();

        app.currentValueLabel.render(app.canvas);
        app.canvas.setFillColor(vectorgl::Color{0.62f, 0.9f, 0.74f, 1.0f});
        app.canvas.fillText(app.field.text(), 120.0f, 392.0f);

        app.statusLabel.setText(app.statusText);
        app.statusLabel.render(app.canvas);

        app.canvas.endFrame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    app.canvas.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
