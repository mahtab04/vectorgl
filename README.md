# VectorGL
A GPU-accelerated 2D vector graphics library for C++20, built on OpenGL 3.3 Core.

VectorGL provides two complementary APIs:

- Canvas — an immediate-mode, HTML5 Canvas-style drawing surface with SDF-accelerated shape primitives.
- Scene — a retained-mode scene graph with animations, effects, and hierarchical node composition.

It also includes an SVG loader, TrueType font rendering, and a post-processing effects pipeline (blur, shadow, glow).
## Features
- SDF-accelerated primitives — rectangles, circles, ellipses, and rounded rectangles rendered entirely on the GPU via signed distance fields. Hundreds of shapes in a single draw call.
- Path rendering — shapes built from lines, cubic/quadratic Bézier curves, arcs, and ellipses, with adaptive curve tessellation and antialiased contour rendering.
- SVG support — load and render SVG files through SvgImage (single file) or SvgCache (handle-based asset manager for batch loading).
- Text rendering — TrueType fonts rasterized to a GPU texture atlas via stb_truetype.
- Animation system — tween (22 easing curves), spring (critically-damped), and keyframe animations with loop/ping-pong modes.
- Post-processing effects — FBO-based Gaussian blur, drop shadows, and outer glow, composable via a fluent EffectChain API.
- Scene graph — parent/child hierarchy with transform inheritance, dirty tracking, and z-index ordering.
- Paint system — solid colors, linear/radial gradients, and texture patterns.
## Requirements
- C++20 compiler (MSVC 2022, GCC 12+, Clang 15+)
- CMake 3.20+
- OpenGL 3.3 capable GPU

All other dependencies are fetched automatically via CMake FetchContent:

## Building
# Configure

cmake -S vectorgl -B build

# Build

cmake --build build --config Debug

# Build examples

cmake --build build --config Debug --target vectorgl_demo

Or use the provided CMake presets:

cmake --preset windows-debug

cmake --build --preset windows-debug
## Quick Start
### Immediate-mode Canvas
#include <vectorgl/canvas.hpp>

// After creating an OpenGL 3.3 context:

vectorgl::Canvas canvas;

canvas.init();

// Each frame:

canvas.beginFrame(fbWidth, fbHeight);

canvas.setFillColor(vectorgl::Color::hex(0x3366FF));

canvas.fillRoundedRect(50, 50, 300, 200, 12);

canvas.setStrokeColor(vectorgl::Color::White);

canvas.setLineWidth(2.0f);

canvas.strokeCircle(400, 150, 60);

canvas.beginPath();

canvas.moveTo(500, 50);

canvas.bezierCurveTo(550, 0, 650, 100, 700, 50);

canvas.stroke();

canvas.endFrame();
### Retained-mode Scene Graph
#include <vectorgl/canvas.hpp>

#include <vectorgl/scene.hpp>

vectorgl::Scene scene(canvas.renderer());

auto rect = scene.roundedRect(100, 100, 200, 150, 8);

rect->setFill(vectorgl::Color::Blue);

rect->setShadow(8.0f, {4, 4}, vectorgl::Color{0, 0, 0, 0.3f});

// Animate

vectorgl::AnimTarget from, to;

from.x = 100; from.mask = vectorgl::AnimTarget::PosX;

to.x = 500;   to.mask = vectorgl::AnimTarget::PosX;

scene.animator().emplace<vectorgl::TweenAnimation>(

    rect->id(), from, to, 2.0f, vectorgl::Ease::OutCubic);

// Each frame:

scene.update(dt);

scene.render();
### SVG Rendering
#include <vectorgl/svg.hpp>

vectorgl::SvgImage svg;

svg.load("icon.svg");

svg.render(canvas, 10.0f, 10.0f, 64.0f, 64.0f);
### Text Input And UI Widgets
VectorGL also includes a small widget layer for common UI building blocks. Use vectorgl::TextBox for single-line text editing, vectorgl::GlfwTextBoxController to forward GLFW input events, and vectorgl::ui::Label / vectorgl::ui::Button for simple text and button elements.

#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <iostream>

#include <string>

#include <vectorgl/canvas.hpp>

#include <vectorgl/glfw_text_box.hpp>

#include <vectorgl/ui.hpp>

struct AppState

{

    vectorgl::Canvas canvas;

    vectorgl::ui::TextBox field;

    vectorgl::ui::Label title;

    vectorgl::ui::Label status;

    vectorgl::ui::Button button;

    vectorgl::GlfwTextBoxController textInput;

    std::string fontPath;

    std::string statusText = "Type into the field or click the button.";

};

static void applySample(AppState& app)

{

    app.field.setText("VectorGL text box with selection and clipboard support.");

    app.field.clearSelection();

    app.field.setFocused(false);

    app.statusText = "Sample text loaded.";

}

static void onChar(GLFWwindow* window, unsigned int codepoint)

{

    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));

    if (app)

        app->textInput.handleChar(codepoint);

}

static void onKey(GLFWwindow* window, int key, int, int action, int mods)

{

    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));

    if (app)

        app->textInput.handleKey(key, action, mods);

}

static void onCursorPos(GLFWwindow* window, double x, double y)

{

    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));

    if (!app)

        return;

    app->textInput.handleCursorPos(x, y);

    app->button.setHovered(app->button.hitTest(static_cast<float>(x), static_cast<float>(y)));

}

static void onMouseButton(GLFWwindow* window, int button, int action, int)

{

    auto* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));

    if (!app)

        return;

    double cursorX = 0.0;

    double cursorY = 0.0;

    glfwGetCursorPos(window, &cursorX, &cursorY);

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)

        app->button.handlePointerDown(static_cast<float>(cursorX), static_cast<float>(cursorY));

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE &&

        app->button.handlePointerUp(static_cast<float>(cursorX), static_cast<float>(cursorY)))

    {

        applySample(*app);

    }

    app->textInput.handleMouseButton(button, action);

}

int main()

{

    if (!glfwInit())

        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(960, 540, "VectorGL UI", nullptr, nullptr);

    if (!window)

    {

        glfwTerminate();

        return 1;

    }

    glfwMakeContextCurrent(window);

    glfwSwapInterval(1);

    if (gladLoadGL(glfwGetProcAddress) == 0)

    {

        glfwDestroyWindow(window);

        glfwTerminate();

        return 1;

    }

    AppState app;

    glfwSetWindowUserPointer(window, &app);

    app.canvas.init();

    app.fontPath = "C:/Windows/Fonts/segoeui.ttf";

    if (!app.canvas.setFont(app.fontPath, 24.0f))

    {

        std::cerr << "Failed to load font\n";

        app.canvas.destroy();

        glfwDestroyWindow(window);

        glfwTerminate();

        return 1;

    }

    app.field.setBounds(40.0f, 140.0f, 460.0f, 52.0f);

    app.field.setFont(app.fontPath, 24.0f);

    app.field.setPlaceholder("Type here");

    app.title.setPosition(40.0f, 72.0f);

    app.title.setText("Text Input And UI Widgets");

    app.title.setFont(app.fontPath, 30.0f);

    app.status.setPosition(40.0f, 250.0f);

    app.status.setFont(app.fontPath, 18.0f);

    app.button.setBounds(530.0f, 140.0f, 160.0f, 52.0f);

    app.button.setText("Load Sample");

    app.button.setFont(app.fontPath, 18.0f);

    app.textInput.bind(app.field, app.canvas, window);

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

        glClearColor(0.05f, 0.07f, 0.11f, 1.0f);

        glClear(GL_COLOR_BUFFER_BIT);

        app.canvas.beginFrame(fbWidth, fbHeight);

        app.title.render(app.canvas);

        app.field.render(app.canvas, dt);

        app.button.render(app.canvas);

        if (app.field.consumeSubmit())

            app.statusText = "Submitted: " + app.field.text();

        app.status.setText(app.statusText);

        app.status.render(app.canvas);

        app.canvas.endFrame();

        glfwSwapBuffers(window);

        glfwPollEvents();

    }

    app.canvas.destroy();

    glfwDestroyWindow(window);

    glfwTerminate();

    return 0;

}

The full working sample lives in example/text_input_demo.cpp. Use it when you want the complete version with styling, hover state, selection, clipboard shortcuts, and status text.
## Project Structure
vectorgl/

├── CMakeLists.txt

├── cmake/                  # CMake modules (dependencies, tooling)

├── include/

│   └── PublicHeaders/

│       └── vectorgl/       # Public API headers

│           ├── canvas.hpp

│           ├── color.hpp

│           ├── path.hpp

│           ├── image.hpp

│           ├── font.hpp

│           ├── renderer.hpp

│           ├── node.hpp

│           ├── scene.hpp

│           ├── animator.hpp

│           ├── effects.hpp

│           ├── paint.hpp

│           ├── svg.hpp

│           └── svg_cache.hpp

├── src/                    # Implementation files

│   └── svg/                # SVG parser modules

├── shaders/                # GLSL shaders (SDF, path, textured, blur)

└── example/                # Demo applications

    ├── main.cpp            # SDF + scene graph demo

    ├── svg_demo.cpp        # SVG rendering demo

    ├── animation_demo.cpp  # Animation showcase

    ├── paths_demo.cpp      # Complex path rendering

    └── ...
## Integration
### CMake find_package
After installing (cmake --install build):

find_package(vectorgl REQUIRED)

target_link_libraries(myapp PRIVATE vectorgl::vectorgl)
### CMake add_subdirectory
add_subdirectory(vectorgl)

target_link_libraries(myapp PRIVATE vectorgl::vectorgl)
### CMake FetchContent
include(FetchContent)

FetchContent_Declare(vectorgl

    GIT_REPOSITORY <url>

    GIT_TAG        v1.0.0

)

FetchContent_MakeAvailable(vectorgl)

target_link_libraries(myapp PRIVATE vectorgl::vectorgl)
## API Documentation
All public headers include Doxygen-style /*! @brief ... */ documentation. Generate HTML docs with:

doxygen Doxyfile

For a ready-to-browse static documentation site, open docs/index.html in a browser. The docs are split into separate pages for overview, guides, API reference, and examples, which also makes the docs/ folder suitable for GitHub Pages style hosting.
### Publishing the docs site
If you want to publish the static docs with GitHub Pages:

- Push the repository to GitHub.
- In the repository settings, enable GitHub Pages.
- Set the source to deploy from the main branch and the /docs folder.
- Use the generated Pages URL as the public docs link for the project.

The docs/ folder is self-contained and includes a .nojekyll marker so GitHub Pages serves the files as a plain static site.
## License
See LICENSE for details.


| Dependency | Version | Purpose |
| --- | --- | --- |
| GLFW | 3.3.8 | Window/context management (examples only) |
| glad | 2.0.8 | OpenGL loader |
| stb | latest | Image loading (stb_image) and font rasterization (stb_truetype) |
