# VectorGL — Implementation Plan
## Vision
VectorGL is a GPU-first, retained-mode 2D vector graphics engine for C++20 on OpenGL 3.3+ Core. Unlike NanoVG, Skia, or Cairo which tessellate paths on the CPU and treat the GPU as a dumb rasterizer, VectorGL pushes rendering intelligence into the GPU via SDF-based shape evaluation, analytic antialiasing in fragment shaders, and a scene graph with automatic dirty-tracking that keeps static geometry resident on the GPU.
### What Makes VectorGL Different


## Architecture
Scene (declarative retained-mode graph)

  ├── Node tree (transform hierarchy, dirty flags)

  ├── Animator (spring physics, tweens, keyframes)

  └── RenderGraph (topologically sorted passes)

        ├── SDF Pass (circles, rects, rounded rects, ellipses)

        ├── Path Pass (complex paths via stencil-then-cover)

        ├── Text Pass (MSDF atlas rendering)

        ├── Image Pass (textured quads)

        ├── Instanced Pass (batched similar shapes)

        └── Effect Pass (blur, glow, shadow, mask)

Canvas (immediate-mode convenience layer wrapping Scene)

  ├── Path2D (curve flattening for complex paths)

  ├── Font (stb_truetype MSDF atlas)

  └── Image (stb_image texture)
## Project Structure
vectorgl/

├── CMakeLists.txt

├── include/vectorgl/

│   ├── canvas.hpp           # Immediate-mode convenience API

│   ├── scene.hpp            # Retained-mode scene graph (THE differentiator)

│   ├── node.hpp             # Scene node with transform, style, dirty flags

│   ├── animator.hpp         # Tween/spring animation engine

│   ├── effects.hpp          # Composable effect descriptors (blur, shadow, glow)

│   ├── color.hpp            # Color with color-space-aware interpolation

│   ├── path.hpp             # Path2D with boolean ops (union/diff/intersect)

│   ├── paint.hpp            # Paint: solid, linear grad, radial grad, pattern

│   ├── font.hpp             # MSDF font atlas

│   ├── image.hpp            # Texture loading

│   └── renderer.hpp         # GL backend + render graph

├── src/

│   ├── canvas.cpp

│   ├── scene.cpp            # Scene graph diffing + GPU sync

│   ├── node.cpp             # Dirty propagation, world transform cache

│   ├── animator.cpp         # Spring solver, easing functions

│   ├── effects.cpp          # Blur/glow/shadow shader management

│   ├── path.cpp             # Flatten + boolean ops (Greiner-Hormann)

│   ├── font.cpp             # MSDF atlas generation

│   ├── image.cpp

│   ├── renderer.cpp         # Draw submission, GL state, paths, FBOs

│   └── shader_utils.cpp     # Runtime shader loading + compile/link diagnostics

├── shaders/                 # GLSL 330 source copied to build/shaders at configure time

│   ├── sdf.vert / sdf.frag

│   ├── path.vert / path.frag

│   ├── textured.vert / textured.frag

│   └── blur.vert / blur.frag

├── example/

│   ├── CMakeLists.txt

│   └── main.cpp

└── implementationplan.md
## Dependencies


## Core Components
### 1. SDF Primitive Renderer (THE KEY DIFFERENTIATOR)
Instead of tessellating circles, rectangles, rounded rectangles, and ellipses into triangles (like every other library), VectorGL evaluates signed distance functions in the fragment shader.

How it works:

- Each primitive is drawn as a single screen-aligned quad (2 triangles)
- The fragment shader computes the exact signed distance to the shape boundary
- AA is computed analytically: alpha = 1.0 - smoothstep(-0.5/scale, 0.5/scale, dist)
- Result: pixel-perfect edges at ANY zoom level, zero tessellation cost, zero geometry complexity

SDF functions (in GLSL):

// Rounded rectangle

float sdRoundedBox(vec2 p, vec2 b, vec4 r) {

    r.xy = (p.x > 0.0) ? r.xy : r.zw;

    r.x  = (p.y > 0.0) ? r.x  : r.y;

    vec2 q = abs(p) - b + r.x;

    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;

}

// Circle: length(p) - radius

// Ellipse: length(p/radii) - 1.0 (approximation)

// Line segment (for strokes): distance to segment - half_width

Advantages over tessellation:

- No polygon count scaling with size or zoom
- Perfect circles even at 1000x zoom (no polygon edges visible)
- Strokes via SDF offset: abs(dist) - strokeWidth/2
- Outlines + fills in a single shader invocation
- Trivial to add new primitives (just write the SDF function)
### 2. Scene Graph with GPU Residency (scene.hpp / node.hpp)
auto scene = vectorgl::Scene::create();

// Nodes persist on GPU — only re-upload when dirty

auto bg = scene.rect({0, 0, 800, 600}).fill(Color::hex(0x1a1a2e));

auto btn = scene.roundedRect({100, 100, 200, 50}, 12)

               .fill(Color::hex(0x16213e))

               .stroke(Color::hex(0x0f3460), 2.0f)

               .shadow(4.0f, {0, 2}, Color::rgba(0,0,0,0.3f));

// Mutating a property marks only that node dirty

btn.setPosition(150, 120);  // Only this node gets re-uploaded next frame

Dirty tracking levels:

- Clean — GPU buffer is current, skip entirely
- TransformDirty — only update uniform, no geometry re-upload
- StyleDirty — update color/stroke uniforms only
- GeometryDirty — re-upload vertex data (e.g., path changed)

Benefits:

- Static UIs cost nearly zero GPU time after first frame
- O(dirty_nodes) work per frame instead of O(all_nodes)
- Natural z-ordering via tree traversal
### 3. Instanced Rendering
Draw thousands of similar shapes in a single draw call:

auto particles = scene.instancedCircle(radius);

particles.setCount(10000);

particles.setPositions(positionBuffer);  // vec2[10000]

particles.setColors(colorBuffer);        // vec4[10000]

// Renders 10,000 circles in 1 draw call via GL_INSTANCED

Uses glDrawArraysInstanced with per-instance attributes for position, scale, rotation, and color. Ideal for particle systems, data visualization scatter plots, or repeated UI elements.
### 4. Built-in Animation Engine (animator.hpp)
No other lightweight GL drawing library has animation built in.

// Tween with easing

btn.animate()

   .to({.x = 300, .y = 200})

   .duration(0.4f)

   .easing(Ease::OutCubic)

   .start();

// Spring physics (critically damped by default)

btn.spring()

   .target({.x = 300, .y = 200})

   .stiffness(200.0f)

   .damping(20.0f)

   .start();

// Keyframe sequences

btn.keyframes()

   .at(0.0f, {.opacity = 0.0f, .scale = 0.8f})

   .at(0.3f, {.opacity = 1.0f, .scale = 1.05f})

   .at(0.5f, {.scale = 1.0f})

   .loop(Loop::PingPong)

   .start();

Easing functions: Linear, EaseIn/Out/InOut (Quad, Cubic, Quart, Expo, Back, Elastic, Bounce), Spring

The animator runs during scene.update(dt) and only marks animated nodes as dirty.
### Shader Asset Layout
Shader sources live in the top-level shaders/ directory and are copied into build/shaders during CMake configure. The renderer loads them from that build-side directory at runtime in all builds, which keeps GLSL editable without re-embedding shader strings into C++.

In Debug builds, the renderer also polls those copied shader files while the app is running and swaps programs only after a full successful recompile/link, so broken live edits do not tear down the currently active shaders. In Release builds, shader hot reload is disabled entirely and the shaders are loaded only once during startup.
### 5. Composable Effects Pipeline (effects.hpp)
Post-processing effects rendered via offscreen FBOs and shader passes:

auto card = scene.group();

card.addChild(bg);

card.addChild(text);

// Effects compose naturally

card.effects()

    .shadow(8.0f, {0, 4}, Color::rgba(0,0,0,0.25f))  // Drop shadow

    .blur(2.0f)       // Gaussian blur (separable, 2-pass)

    .glow(4.0f, Color::hex(0x00ff88, 0.6f));          // Outer glow

// Effects are cached — only re-render when subtree is dirty

Implementation:

- Render subtree to offscreen FBO at node resolution
- Apply separable Gaussian blur (horizontal + vertical pass)
- Shadow: render blurred silhouette offset behind original
- Glow: additive blurred copy
- Mask/clip: stencil or alpha from another node
### 6. Gradient & Pattern Paint (paint.hpp)
auto gradient = Paint::linearGradient({0,0}, {200,0}, {

    {0.0f, Color::hex(0xFF6B6B)},

    {0.5f, Color::hex(0xFECA57)},

    {1.0f, Color::hex(0x48DBFB)}

});

auto radial = Paint::radialGradient({100,100}, 50, 150, {

    {0.0f, Color::White},

    {1.0f, Color::Transparent}

});

auto pattern = Paint::pattern(myImage, PatternRepeat::Both);

rect.fill(gradient);

circle.fill(radial);

Gradients are evaluated in the fragment shader — no texture allocation for simple gradients.
### 7. Path Boolean Operations (path.hpp)
Path2D a = Path2D::circle(100, 100, 50);

Path2D b = Path2D::circle(130, 100, 50);

Path2D merged   = Path2D::unite(a, b);      // Union

Path2D cut      = Path2D::subtract(a, b);   // Difference

Path2D overlap  = Path2D::intersect(a, b);  // Intersection

Path2D outline  = Path2D::xorOp(a, b);      // Symmetric difference

Uses the Greiner-Hormann algorithm for polygon clipping. Works on flattened paths (after bezier subdivision).
### 8. MSDF Text Rendering (font.hpp)
Multi-channel Signed Distance Fields instead of single-channel SDF:

- Sharper corners and better small-size rendering than traditional SDF
- Single atlas texture for all glyphs
- Fragment shader: float d = median(r, g, b); alpha = smoothstep(...)
- Supports: kerning, line height, letter spacing, text alignment
- measureText() for layout calculations
### 9. Color with Perceptual Interpolation (color.hpp)
Color a = Color::hex(0xFF0000);

Color b = Color::hex(0x0000FF);

Color mid_rgb  = Color::lerp(a, b, 0.5f);        // RGB lerp (muddy)

Color mid_oklab = Color::lerpOklab(a, b, 0.5f);  // Oklab lerp (vibrant!)

All gradient interpolation uses Oklab by default — produces perceptually uniform, vibrant transitions instead of the muddy grays of RGB interpolation.
### 10. Canvas Immediate-Mode Layer (canvas.hpp)
For quick prototyping, the familiar immediate-mode API still exists — but internally it creates transient scene nodes:

canvas.beginFrame(w, h);

canvas.setFillColor(Color::hex(0x4A90D9));

canvas.fillRect(30, 30, 180, 100);          // Creates a transient SDF rect node

canvas.fillText("Hello", 30, 200);          // Creates a transient MSDF text node

canvas.endFrame();                           // Renders scene, discards transient nodes


## Rendering Pipeline
Scene::update(dt)

  → Animator ticks all active animations

  → Dirty flags propagate up (subtree) and down (world transforms)

Scene::render()

  → RenderGraph sorts nodes by: pass type → z-order → material

  → SDF Pass:

      Collect all SDF primitives (rect, circle, rounded rect, ellipse)

      Upload instance buffer (pos, size, corner radii, color, stroke)

      Single glDrawArraysInstanced per primitive type

      Fragment shader evaluates SDF + analytic AA

  → Path Pass (complex shapes only):

      Stencil-then-cover: render path to stencil buffer

      Cover pass fills stencil region with color

      (Only for paths that can't be expressed as SDF primitives)

  → Text Pass:

      Batch all text quads by atlas page

      MSDF fragment shader for crisp edges

  → Image Pass:

      Textured quads with tint

  → Effect Pass:

      For nodes with effects, render subtree to FBO

      Apply blur/shadow/glow shader passes

      Composite back to main framebuffer

  → Instanced Pass:

      For instance groups, single draw call per group
### Antialiasing Strategy (Analytic, not Geometric)
Unlike geometry-fringe AA (NanoVG, old VectorGL plan), analytic AA computes coverage in the fragment shader:

// For SDF primitives:

float dist = sdRoundedBox(localPos, halfSize, cornerRadii);

float aa = fwidth(dist);  // Screen-space derivative for AA width

float alpha = 1.0 - smoothstep(-aa, aa, dist);

// For strokes:

float strokeDist = abs(dist) - strokeWidth * 0.5;

float strokeAlpha = 1.0 - smoothstep(-aa, aa, strokeDist);

Advantages:

- Resolution-independent: perfect at any zoom level
- No extra geometry (no fringe vertices, no triangle count increase)
- Sub-pixel accurate
- Works with arbitrary transforms (rotation, skew)


## API Design Philosophy
### Retained Mode (Primary — for applications)
// Create once, update incrementally

auto scene = vectorgl::Scene::create();

auto sidebar = scene.roundedRect({0, 0, 250, 800}, 0).fill(Color::hex(0x2d2d2d));

auto button = scene.roundedRect({20, 20, 210, 40}, 8)

    .fill(Color::hex(0x3d3d3d))

    .stroke(Color::hex(0x5d5d5d), 1);

// On hover — only the button re-renders

button.fill(Color::hex(0x4d4d4d));

button.animate().to({.fill = Color::hex(0x4d4d4d)}).duration(0.15f).start();

// Per frame

scene.update(dt);

scene.render();
### Immediate Mode (Secondary — for quick sketches)
// Familiar Canvas-like API, zero persistence

canvas.beginFrame(w, h);

canvas.setFillColor(Color::hex(0x4A90D9));

canvas.fillRoundedRect(100, 100, 200, 50, 12);

canvas.endFrame();


## Build Instructions
cd vectorgl

cmake -B build -DCMAKE_BUILD_TYPE=Release

cmake --build build

./build/example/vectorgl_example    # or build\example\Release\vectorgl_example.exe on Windows
## Example App Features
The demo showcases capabilities impossible in NanoVG/Cairo:

- GPU SDF shapes — zoom into a circle at 100x, still perfectly smooth
- Animated dashboard — cards slide in with spring physics
- 10,000 particle circles — rendered in 1 draw call via instancing
- Gradient-filled paths — Oklab-interpolated linear/radial gradients
- Drop shadows & glow — real-time composable effects
- Interactive buttons — hover/click with animated color transitions
- Boolean path operations — union/subtract/intersect visualized
- MSDF text — crisp at any size, with kerning
- Scene graph demo — static elements cost 0 GPU time after first frame
- Stress test — 1000 animated rounded rects at 60fps


## Implementation Phases
### Phase 1: Core SDF Renderer
- SDF fragment shaders for rect, rounded rect, circle, ellipse, line
- Analytic AA via fwidth()
- Basic VAO/VBO with per-shape uniforms
- Transform stack (Mat3x3)
- Color struct with hex/rgba factories
### Phase 2: Scene Graph + Dirty Tracking
- Node tree with parent/child relationships
- Dirty flag system (Transform / Style / Geometry)
- GPU buffer management with partial updates (glBufferSubData)
- Z-order sorting
### Phase 3: Instanced Rendering + Batching
- Instance buffer for SDF primitives
- Single draw call per primitive type per frame
- Automatic batching of similar nodes
### Phase 4: Animation Engine
- Tween system with easing functions
- Spring physics (critically damped oscillator)
- Keyframe sequences with loop modes
- Integration with dirty-tracking (animated = dirty)
### Phase 5: Text + Images
- MSDF atlas generation from TTF
- Text layout (measure, wrap, align)
- Image loading + textured quads
- Atlas-based text batching
### Phase 6: Complex Paths
- Path2D with bezier/arc flattening
- Stencil-then-cover fill for arbitrary paths
- Stroke expansion for non-SDF paths
- Boolean operations (Greiner-Hormann)
### Phase 7: Gradients + Paint
- Linear gradient (fragment shader evaluation)
- Radial gradient
- Oklab color interpolation
- Pattern fills
### Phase 8: Effects Pipeline
- Offscreen FBO rendering per effect group
- Separable Gaussian blur (2-pass)
- Drop shadow (offset + blur + tint)
- Outer glow (blur + additive blend)
- Clip/mask regions
### Phase 9: Canvas Immediate Layer
- Wrapper that creates transient scene nodes
- HTML5 Canvas-like API for rapid prototyping
- Auto-cleanup after endFrame()


## Future Enhancements
- Compute shader path tessellation (GL 4.3+ optional fast path)
- SVG path string import (M 10 20 L 30 40 C ...)
- Dash patterns for strokes
- Texture-backed SDF for custom shapes
- Multi-window / multi-context support
- Vulkan backend
- WASM/WebGL2 target
- Accessibility tree output for UI usage


| Feature | NanoVG | Skia | Cairo | Blend2D | VectorGL |
| --- | --- | --- | --- | --- | --- |
| Primitive rendering | CPU tessellation | CPU tessellation | CPU rasterization | SIMD CPU | GPU SDF evaluation |
| Antialiasing | Geometry fringe | MSAA / analytic | CPU AA | CPU AA | Analytic fragment shader |
| Mode | Immediate only | Immediate | Immediate | Immediate | Hybrid retained + immediate |
| Animation | None | None | None | None | Built-in spring/tween engine |
| Geometry caching | None | Partial | None | None | Full GPU-resident scene graph |
| Effects pipeline | None | SkImageFilter | None | None | Composable shader passes |
| Draw calls per frame | O(n) shapes | O(n) | O(n) | O(n) | O(dirty) — cached static |
| Instancing | No | No | No | No | Yes — 10k+ shapes, 1 call |

| Dependency | Purpose | Source |
| --- | --- | --- |
| GLFW 3.3+ | Window/context (example only) | CMake FetchContent |
| GLAD | OpenGL 3.3 Core loader | CMake FetchContent |
| stb_truetype | TTF → MSDF glyph atlas | CMake FetchContent |
| stb_image | PNG/JPG/BMP image loading | CMake FetchContent |
