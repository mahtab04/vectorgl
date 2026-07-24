# VectorGL

> A GPU-accelerated 2D vector graphics library for C++20, built on OpenGL 3.3 Core.

VectorGL provides two complementary APIs:

- **Canvas** — an immediate-mode, HTML5 Canvas-style drawing surface with SDF-accelerated shape primitives.
- **Scene** — a retained-mode scene graph with animations, effects, and hierarchical node composition.

It also includes an SVG loader, TrueType font rendering, and a post-processing effects pipeline (blur, shadow, glow).

---

## ✨ Features

| Feature | Description |
|---|---|
| **SDF Primitives** | Rectangles, circles, ellipses, and rounded rectangles rendered on the GPU via signed distance fields — hundreds of shapes in a single draw call. |
| **Path Rendering** | Lines, cubic/quadratic Bézier curves, arcs, and ellipses with adaptive tessellation and antialiased contours. |
| **SVG Support** | Load and render SVG files via `SvgImage` (single file) or `SvgCache` (handle-based asset manager). |
| **Text Rendering** | TrueType fonts rasterized to a GPU texture atlas via `stb_truetype`. |
| **Animation System** | Tween (22 easing curves), spring (critically-damped), and keyframe animations with loop/ping-pong modes. |
| **Post-Processing** | FBO-based Gaussian blur, drop shadows, and outer glow — composable via a fluent `EffectChain` API. |
| **Scene Graph** | Parent/child hierarchy with transform inheritance, dirty tracking, and z-index ordering. |
| **Paint System** | Solid colors, linear/radial gradients, and texture patterns. |

---

## 📋 Requirements

| Requirement | Minimum Version |
|---|---|
| C++ Standard | C++20 |
| Compiler | MSVC 2022, GCC 12+, or Clang 15+ |
| CMake | 3.20+ |
| GPU | OpenGL 3.3 capable |

> All other dependencies are fetched automatically via **CMake FetchContent**.

---

## 🔧 Dependencies

| Dependency | Version | Purpose |
|---|---|---|
| [GLFW](https://www.glfw.org/) | 3.3.8 | Window/context management *(examples only)* |
| [glad](https://github.com/Dav1dde/glad) | 2.0.8 | OpenGL loader |
| [stb](https://github.com/nothings/stb) | latest | Image loading (`stb_image`) and font rasterization (`stb_truetype`) |

---

## 🏗️ Building

### Standard CMake

```bash
# Configure
cmake -S vectorgl -B build

# Build
cmake --build build --config Debug

# Build examples
cmake --build build --config Debug --target vectorgl_demo
```

### Using CMake Presets

```bash
cmake --preset windows-debug
cmake --build --preset windows-debug
```

---

## 🚀 Quick Start

### Immediate-Mode Canvas

```cpp
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
```

### Retained-Mode Scene Graph

```cpp
#include <vectorgl/canvas.hpp>
#include <vectorgl/scene.hpp>

vectorgl::Scene scene(canvas.renderer());

auto rect = scene.roundedRect(100, 100, 200, 150, 8);
rect->setFill(vectorgl::Color::Blue);
rect->setShadow(8.0f, {4, 4}, vectorgl::Color{0, 0, 0, 0.3f});

// Animate
vectorgl::AnimTarget from, to;
from.x = 100; from.mask = vectorgl::AnimTarget::PosX;
to.x   = 500; to.mask   = vectorgl::AnimTarget::PosX;

scene.animator().emplace<vectorgl::TweenAnimation>(
    rect->id(), from, to, 2.0f, vectorgl::Ease::OutCubic);

// Each frame:
scene.update(dt);
scene.render();
```

### SVG Rendering

```cpp
#include <vectorgl/svg.hpp>

vectorgl::SvgImage svg;
svg.load("icon.svg");
svg.render(canvas, 10.0f, 10.0f, 64.0f, 64.0f);
```

### Text Input & UI Widgets

VectorGL includes a small widget layer for common UI building blocks:

- `vectorgl::TextBox` — single-line text editing
- `vectorgl::GlfwTextBoxController` — forwards GLFW input events
- `vectorgl::ui::Label` / `vectorgl::ui::Button` — simple text and button elements

```cpp
#include <vectorgl/canvas.hpp>
#include <vectorgl/glfw_text_box.hpp>
#include <vectorgl/ui.hpp>

AppState app;
app.canvas.init();
app.field.setBounds(40.0f, 140.0f, 460.0f, 52.0f);
app.field.setFont(fontPath, 24.0f);
app.field.setPlaceholder("Type here");

app.button.setBounds(530.0f, 140.0f, 160.0f, 52.0f);
app.button.setText("Load Sample");

app.textInput.bind(app.field, app.canvas, window);
```

> 💡 The full working sample lives in `example/text_input_demo.cpp` — use it for the complete version with styling, hover state, selection, clipboard shortcuts, and status text.

---

## 📁 Project Structure

```
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
```

---

## 🔌 Integration

### Via `find_package` *(after installing)*

```cmake
find_package(vectorgl REQUIRED)
target_link_libraries(myapp PRIVATE vectorgl::vectorgl)
```

### Via `add_subdirectory`

```cmake
add_subdirectory(vectorgl)
target_link_libraries(myapp PRIVATE vectorgl::vectorgl)
```

### Via `FetchContent`

```cmake
include(FetchContent)

FetchContent_Declare(vectorgl
    GIT_REPOSITORY <url>
    GIT_TAG        v1.0.0
)

FetchContent_MakeAvailable(vectorgl)
target_link_libraries(myapp PRIVATE vectorgl::vectorgl)
```

---

## 📖 API Documentation

All public headers include Doxygen-style `/*! @brief ... */` documentation.

```bash
# Generate HTML docs
doxygen Doxyfile
```

Then open `docs/index.html` in your browser. The docs are split into separate pages for:

- Overview
- Guides
- API Reference
- Examples

> The `docs/` folder is self-contained and suitable for **GitHub Pages** hosting.

### Publishing to GitHub Pages

1. Push the repository to GitHub.
2. Go to **Settings → Pages**.
3. Set the source to the `main` branch, `/docs` folder.
4. Use the generated Pages URL as your public docs link.

> A `.nojekyll` marker is included so GitHub Pages serves the files as a plain static site.

---

## 📄 License

See [LICENSE](./LICENSE) for details.
