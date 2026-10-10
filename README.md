# VectorGL

[![Build and Test](https://github.com/mahtab04/vectorgl/actions/workflows/build.yml/badge.svg)](https://github.com/mahtab04/vectorgl/actions/workflows/build.yml)
[![Latest Release](https://img.shields.io/github/v/release/mahtab04/vectorgl?display_name=tag&sort=semver)](https://github.com/mahtab04/vectorgl/releases/latest)
[![License](https://img.shields.io/github/license/mahtab04/vectorgl)](./LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/20)
[![OpenGL 3.3](https://img.shields.io/badge/OpenGL-3.3-5586A4?logo=opengl)](https://www.khronos.org/opengl/)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/mahtab04/vectorgl)

[Documentation](https://mahtab04.github.io/vectorgl/) ·
[Architecture](ARCHITECTURE.md) ·
[Examples](https://mahtab04.github.io/vectorgl/examples.html) ·
[API Reference](https://mahtab04.github.io/vectorgl/api.html) ·
[Guides](https://mahtab04.github.io/vectorgl/guides.html) ·
[Releases](https://github.com/mahtab04/vectorgl/releases)

> A GPU-accelerated 2D vector graphics library for C++20, built on OpenGL 3.3 Core.

[![VectorGL showcase rendered by the library](docs/images/vectorgl-showcase.png)](example/showcase.cpp)

_The scene above is rendered in real time by the standalone
[`vectorgl_showcase`](example/showcase.cpp) example._

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
| **Nested Clipping** | Transformed rectangular and rounded clipping with intersection, reset, and automatic `save()` / `restore()` state. |

## Large scenes and rendering statistics

Build and run [`vectorgl_large_scene`](example/large_scene_demo.cpp) to explore
10,000 tiles and 16 text labels. Drag to pan, use the wheel to zoom, press **C**
to toggle viewport culling, **L** to clear scene layouts each frame, and **R**
to reset. Pass `--font path/to/font.ttf` to choose a font. The HUD reports CPU
scene submission time, draw calls, streamed buffer bytes, culled nodes and
layout cache hits/misses. CPU submission time excludes presentation and does
not measure GPU execution time. The HUD's own drawing is excluded from these
scene measurements.

```cpp
scene.render();                    // Within an active Canvas frame
canvas.renderer().flush();         // Include pending batches in counters
const auto stats = canvas.renderer().frameStats();
canvas.renderer().setViewportCulling(false); // Compare against the baseline
```

Counters reset at `beginFrame()` and remain available after `endFrame()`.
Draw calls include clipping masks and effects; upload bytes count streamed
vertex/instance buffers, excluding glyph/image texture uploads. Rendered nodes
means submissions retained after culling, including missing-resource nodes.

Scene SDF shapes and images are conservatively culled against the framebuffer,
including transformed quad bounds and SDF stroke/antialias padding. Text,
paths, effect-bearing nodes, and nonfinite/unsupported bounds are retained.
Culling does not affect picking or direct Canvas primitive draws. Scene render
lists reuse their allocation, but rebuild and sort each frame to observe edits.

Canvas and scene text drawing share an immutable layout cache owned by each
font: at most 32 entries and 2 MiB of accounted text/layout storage. Keys include
text, wrapping width/mode, alignment and line spacing. Layout metrics stay
independent of transforms and bitmap/SDF mode; reload/destroy invalidates the
cache. Oversized inputs/layouts are computed without retention. Call
`font.layoutCacheStats()` or `font.clearLayoutCache()` to inspect/reset it.
`cachedLayoutText()` returns shared immutable layouts; externally retained
layouts survive eviction and are outside the cache budget. Allocator bookkeeping
is outside the accounted byte limit. Font caches require external synchronization
if accessed from multiple threads.

---

## 📋 Requirements

| Requirement | Minimum Version |
|---|---|
| C++ Standard | C++20 |
| Compiler | MSVC 2022, GCC 12+, or Clang 15+ |
| CMake | 3.20+ |
| GPU | OpenGL 3.3 capable; 8-bit stencil buffer for rounded clipping |

> All other dependencies are fetched automatically via **CMake FetchContent**.

---

## 🔧 Dependencies

| Dependency | Version | Purpose |
|---|---|---|
| [GLFW](https://www.glfw.org/) | 3.3.8 | Window/context management *(examples only)* |
| [glad](https://github.com/Dav1dde/glad) | 2.0.8 | Vendored OpenGL 3.3 loader |
| [stb](https://github.com/nothings/stb) | latest | Image loading (`stb_image`) and font rasterization (`stb_truetype`) |

---

## 🏗️ Building

For prerequisites, troubleshooting, and contribution steps,
see [Building and Contributing](CONTRIBUTING.md).

### Windows helper

```bat
scripts\build.bat -Configuration Debug
```

The helper works from PowerShell or Command Prompt. It detects the available
toolchain, builds examples, deploys MinGW runtime DLLs when needed, and runs
tests. See the contribution guide for direct PowerShell usage and options.

Linux and macOS users can run:

```bash
./scripts/build.sh
```

Visual Studio is optional on Windows: the helper also detects MinGW-w64,
Ninja with GCC/Clang, and NMake developer environments.

GitHub Actions tests both Debug and Release with Windows MSVC and Windows
MinGW GCC/Ninja, in addition to Linux GCC.

### Standard CMake

```bash
# Configure
cmake -S vectorgl -B build

# Build
cmake --build build --config Debug

# Build the flagship showcase
cmake --build build --config Debug --target vectorgl_showcase
```

### Using CMake Presets

```bash
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

The Windows presets let CMake select the installed Visual Studio version,
including VS 2022 and VS 2026. VS 2026 requires CMake 4.2 or newer;
VS 2022 and the presets require CMake 3.21 or newer.

For MinGW-w64 with `gcc`, `g++`, and `mingw32-make` in PATH, use
`windows-mingw-debug` or `windows-mingw-release` instead. The Windows helper
also detects MinGW automatically and uses separate output directories for
each generator/compiler combination.

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

### Scene Hit Testing

Use `Scene::pick(x, y)` to select the topmost painted shape in framebuffer
coordinates. It refreshes transforms without advancing animations and needs no
OpenGL context:

```cpp
if (auto node = scene.pick(mouseX, mouseY)) {
    node->setFill(vectorgl::Color::Red);
}
```

Picking supports rectangles, rounded rectangles, circles, ellipses, lines and
simple paths, with parent transforms, visibility, opacity, fills and strokes.
Higher z-index wins; ties follow draw order. Empty outline interiors do not hit.
Path subpaths are filled independently, matching the current renderer. Effects,
antialias fringes and renderer clipping are excluded. Text uses logical layout
bounds, including spaces and gaps between lines; images use their full rectangle,
including transparent pixels. Both support the full parent affine transform.

The [interactive example](example/hit_testing_demo.cpp) demonstrates selection,
dragging, a rotated parent group, outlines, and overlapping shapes. Build the
`vectorgl_hit_testing` target and run it from your build's `example` directory
(the `Debug` subdirectory for Visual Studio). Click/drag to move, Up/Down to
change z-index, R to reset, and Esc to close. Convert GLFW window cursor units to
framebuffer pixels on high-DPI displays, as shown in the example.

### Scene Text and Images

```cpp
#include <vectorgl/font.hpp>
#include <vectorgl/image.hpp>

auto font = std::make_shared<vectorgl::Font>();
auto image = std::make_shared<vectorgl::Image>();
if (!font->load("font.ttf", 22) || !image->load("photo.png")) {
    // Handle missing assets before adding content.
    return;
}
vectorgl::TextLayoutOptions options;
options.maxWidth = 320;
options.align = vectorgl::TextAlign::Center;
auto label = scene.text("Unicode text", 40, 60, font, options);
label->setFill(vectorgl::Color::White);
auto picture = scene.image(image, 40, 180, 320, 180);
picture->setOpacity(0.8f);
```

Text positions address the top-left layout origin. The loaded font sets the em
size; `setTextLayout()` controls wrapping, alignment and line spacing. Update
content with `setText()` or replace the shared font with `setFont()`.
`textLayout()` measures without GL calls. `setSize()` does not resize text;
load a different font size or use `setScale()`. Text uses fill color and ignores
stroke. Pixel snapping defaults on and can be disabled with `setTextPixelSnap(false)`.

The image factory takes top-left coordinates, then stores a centered rectangle,
like `rect()`. Later `setPosition()` moves its center. Zero width/height use the
image's natural dimensions; positive dimensions stretch it. `setImage()` changes
the resource. Fill/stroke colors do not tint images; node opacity multiplies
source alpha. Null or unloaded resources produce no rendering or hit.

Nodes share ownership of fonts/images. Keep their GL context current when loading
or releasing resources. Queued scene text retains its font until the glyph batch
is flushed; explicitly flush the renderer before reloading or destroying a font
in place. Parent transforms and visibility apply, while opacity remains a property
of each individual node. Both `render()` overloads refresh transforms without
advancing animations; `update(dt)` advances animations.

Build and run `vectorgl_scene_content` for the
[scene text/image example](example/scene_content_demo.cpp). Click/drag text or images,
Up/Down changes order, O toggles opacity, R resets, and Esc closes. The example
shares one image between two nodes, includes a rotated parent group, and handles
high-DPI cursor coordinates. Pass `--font path/to/font.ttf` to choose a font.

### SVG Rendering

```cpp
#include <vectorgl/svg.hpp>

vectorgl::SvgImage svg;
svg.load("icon.svg");
svg.render(canvas, 10.0f, 10.0f, 64.0f, 64.0f);
```

### Text Input & UI Widgets

Font atlases use a least recently used cache, limited to 16 entries by default.
Applications can tune or clear it:

```cpp
canvas.setFontCacheLimit(8);
canvas.clearFontCache(); // flushes pending text before releasing its atlases
```

Font loading returns `false` for unsupported files or sizes outside `(0, 256]`.
Unicode glyphs are rasterized on demand into up to four 1024x1024 R8 pages per
font, with a maximum of 4096 distinct cached glyphs. Font-file bytes remain in
memory for dynamic rasterization and kerning. Atlas pages keep stable UVs;
missing glyphs or a full cache draw the replacement glyph while retaining the
original font advance. Consecutive glyphs sharing a page are batched, with drawing order
preserved across shapes, images, clipping, and effects. Low-level callers must
keep an atlas alive until `renderer.flush()` or the end of the frame.

Font sizes specify the em square in pixels; line height follows the font's
ascent/descent/gap metrics and may be larger than the requested size.
`fillText()` and `measureText()` share UTF-8 decoding and font kerning. The `y`
coordinate is the top of the line; the baseline is `y + font ascent`. Explicit
line breaks are supported, and measurement returns the widest line. Add layout
options for wrapping, alignment, and line spacing:

```cpp
vectorgl::TextLayoutOptions options;
options.maxWidth = 320;
options.wrap = vectorgl::TextWrap::Word; // None or Character are also available
options.align = vectorgl::TextAlign::Center; // Left or Right
options.lineSpacing = 1.25f;
canvas.fillText(utf8Text, 40, 100, options);
auto layout = canvas.layoutText(utf8Text, options); // no texture uploads or GL calls
// layout.width/height, lines, glyph positions, and UTF-8 caret byte boundaries
```

Word wrapping breaks at whitespace and falls back to codepoint breaks for long
words. Tabs advance by four spaces. `setTextPixelSnap(false)` enables fractional
line origins; by default whole lines snap in framebuffer space, preserving
fractional glyph advances. SDF coverage uses a one-pixel antialias band.

Small, snapped, axis-aligned text uses FreeType light-hinted grayscale coverage
by default at 1-24 framebuffer pixels per em. Larger text, rotation, reflection,
shear, nonuniform scaling, and disabled pixel snapping use SDF. Uniform DPI
scaling is included when choosing the raster size. Layout, kerning, wrapping
and caret positions stay the same in both modes.

```cpp
// Set after beginFrame(); save()/restore() preserves this setting.
canvas.setTextRenderingMode(vectorgl::TextRenderingMode::Auto); // default
canvas.setTextRenderingMode(vectorgl::TextRenderingMode::Sdf);  // always SDF
canvas.setTextRenderingMode(vectorgl::TextRenderingMode::Bitmap);
label->setTextRenderingMode(vectorgl::TextRenderingMode::Bitmap); // scene text
```

`Bitmap` permits hinted coverage up to 64 framebuffer pixels per em for supported
transforms; other transforms and unavailable coverage fall back to SDF. It rounds
the raster em size and baseline to device pixels, with quarter-pixel horizontal
phases to preserve fractional spacing. This is grayscale coverage, not LCD or
Windows ClearType. All sizes/phases share the font's existing four-page,
4096-entry cache limit; pages and UVs stay stable until font reload/destruction.

FreeType 2.14.3 is fetched at configure time, built statically, and installed with
VectorGL and its license notices. Use `-DVECTORGL_ENABLE_HINTED_TEXT=OFF` for the
original SDF-only build; bitmap requests then fall back to SDF. Existing offline
builds need one online configure to populate the new dependency.

The [small-text comparison example](example/text_raster_demo.cpp),
`vectorgl_text_raster`, compares SDF, hinted bitmap and Auto at 12-32 px and
1x/1.5x/2x framebuffer scales. F2 toggles pixel snapping; F3 switches the editable
16 px Unicode field's mode. It supports `--font path/to/font.ttf` and
`--smoke-test --screenshot preview.bmp`.

This software uses the FreeType library under the FreeType License (FTL).
Portions of this software are copyright (C) 2026 The FreeType Project
(https://freetype.org). All rights reserved.

`TextBox` accepts Unicode typing and clipboard input. Arrow keys, selection,
Backspace/Delete, scrolling, and pointer placement preserve UTF-8 codepoints.
Selection indices are UTF-8 byte offsets; malformed input becomes U+FFFD and
single-line controls are removed. Layout is left-to-right and does not include
complex-script shaping, bidi, font fallback stacks, color emoji, IME composition,
or grapheme-cluster editing (combining marks and emoji sequences remain separate
codepoints).

The [text layout example](example/text_layout_demo.cpp) shows Unicode input,
kerning, wrapped left/center/right columns, small and transformed text, and a
sharp-versus-blur comparison. Run `vectorgl_text_layout` from your build's
`example/Debug` directory on Visual Studio, or `example` with a single-config
generator. It discovers a system font, or accepts `--font path/to/font.ttf`.
F2 toggles pixel snapping. `--smoke-test --screenshot preview.bmp` saves a hidden
preview and exits. The blur pass now flushes queued glyphs, uses the correct
vertex layout and UV orientation, and preserves opacity through filtering.

Animation time steps must be finite and non-negative. Looping animations wrap
time without repeatedly subtracting their duration, and tween/keyframe masks
support position, size, rotation, scale, opacity, fill, and stroke color.

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
    GIT_REPOSITORY https://github.com/mahtab04/vectorgl.git
    GIT_TAG        v1.0.0
)

FetchContent_MakeAvailable(vectorgl)
target_link_libraries(myapp PRIVATE vectorgl::vectorgl)
```

---

## 📖 Documentation

The complete documentation is published as a browsable site:

- [Documentation home](https://mahtab04.github.io/vectorgl/)
- [Overview and architecture](https://mahtab04.github.io/vectorgl/overview.html)
- [Usage guides](https://mahtab04.github.io/vectorgl/guides.html)
- [API reference](https://mahtab04.github.io/vectorgl/api.html)
- [Example gallery](https://mahtab04.github.io/vectorgl/examples.html)

The source for the static site lives in [`docs/`](./docs). Every push to
`main` deploys that directory through the GitHub Pages workflow.

---

## ✅ Build and test status

The [Build and Test workflow](https://github.com/mahtab04/vectorgl/actions/workflows/build.yml)
compiles and tests all four supported CI configurations:

- Linux GCC Debug
- Linux GCC Release
- Windows MSVC Debug
- Windows MSVC Release

Version tags matching `v*` run the release pipeline and publish installable
Windows and Linux archives. See [Releases](https://github.com/mahtab04/vectorgl/releases).

---

## 📄 License

See [LICENSE](./LICENSE) for details.
