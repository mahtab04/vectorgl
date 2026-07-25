const featureCards = [
    {
        title: "Immediate-mode Canvas",
        text: "Use Canvas when your app already owns the frame loop and you want to issue draw commands directly each frame. It handles SDF primitives, HTML5-style paths, text, images, and SVG rendering through helper classes.",
        bullets: [
            "Best for overlays, editors, visualizers, HUDs, and custom widgets.",
            "Requires beginFrame() / endFrame() around every frame.",
            "Stateful: fill color, stroke color, line width, and transform stay active until changed."
        ]
    },
    {
        title: "Retained-mode Scene",
        text: "Use Scene when you want reusable nodes, hierarchy, animations, transform inheritance, dirty tracking, and z-ordering. Scene owns nodes and coordinates the Animator.",
        bullets: [
            "Factory methods create typed nodes such as rect(), circle(), ellipse(), and path().",
            "Children inherit world transforms from their parents.",
            "Each frame usually calls scene.update(dt) followed by scene.render(...)."
        ]
    },
    {
        title: "SDF primitives",
        text: "Rectangles, circles, ellipses, and rounded rectangles are rendered with signed distance field evaluation in the fragment shader, giving sharp edges and low draw overhead.",
        bullets: [
            "Ideal for UI panels, controls, badges, chips, and smooth icons.",
            "Use Canvas primitive calls or Scene shape factories for these.",
            "Prefer Path2D only when the geometry is not one of the built-in primitives."
        ]
    },
    {
        title: "Asset support",
        text: "VectorGL ships with raster image loading, TrueType font atlas generation, a paint system for gradients and patterns, and SVG loaders for both single assets and handle-based caches.",
        bullets: [
            "SvgImage is the simple one-off wrapper.",
            "SvgCache is better for many SVG assets used repeatedly.",
            "Font and Image are move-only GPU resources."
        ]
    }
];

const homePageCards = [
    {
        title: "Overview",
        text: "Start here if you need the big picture: rendering model, setup requirements, build commands, and the Scene versus Canvas split.",
        bullets: [
            "Architecture and lifecycle summary.",
            "Minimal Canvas and Scene startup snippets.",
            "Good first stop before reading individual APIs."
        ],
        href: "overview.html"
    },
    {
        title: "Guides",
        text: "Task-oriented usage notes for drawing shapes, building paths, rendering text, loading images, composing nodes, and using SVG assets.",
        bullets: [
            "Written as quick recipes rather than raw declarations.",
            "Code snippets are copyable.",
            "Best when you already know what you want to draw."
        ],
        href: "guides.html"
    },
    {
        title: "API Reference",
        text: "Search across the public headers and inspect grouped method descriptions for every major class and supporting type.",
        bullets: [
            "Covers Canvas, Scene, Renderer, Node, Path2D, SVG, Font, Image, Animator, and more.",
            "Useful when you already know the type name.",
            "Search by class, method, header, or topic."
        ],
        href: "api.html"
    },
    {
        title: "Examples",
        text: "Summaries of the existing repository demos, including what each example proves and what pattern you can reuse in your own app.",
        bullets: [
            "Maps docs back to the shipped C++ examples.",
            "Shows how the library is intended to be used in real loops.",
            "Useful for GitHub readers who want practical context fast."
        ],
        href: "examples.html"
    }
];

const guides = [
    {
        title: "Clip drawing to a region",
        text: "Use clipRect() to constrain subsequent drawing to a transformed rectangular region. Nested clips intersect automatically, and save()/restore() makes clipping convenient for reusable widgets.",
        bullets: [
            "Clip coordinates use VectorGL's top-left coordinate system.",
            "The current transform is applied when the clip is created.",
            "Call resetClip() to remove all clipping without restoring state."
        ],
        code: `canvas.save();
canvas.clipRect(40.0f, 40.0f, 280.0f, 160.0f);

canvas.setFillColor(vectorgl::Color::hex(0x38BDF8));
canvas.fillCircle(300.0f, 120.0f, 100.0f);

canvas.clipRect(120.0f, 70.0f, 140.0f, 90.0f);
canvas.setFillColor(vectorgl::Color::hex(0xA78BFA));
canvas.fillRect(80.0f, 40.0f, 240.0f, 150.0f);
canvas.restore();`
    },
    {
        title: "Draw standard shapes",
        text: "Use Canvas primitive methods when the shape is a rectangle, rounded rectangle, circle, or ellipse. These go through the SDF pipeline and are the most efficient route for common UI geometry.",
        bullets: [
            "Call setFillColor() or setStrokeColor() first.",
            "Use setLineWidth() before stroke operations.",
            "Wrap the entire sequence with beginFrame() and endFrame()."
        ],
        code: `vectorgl::Canvas canvas;
canvas.init();

canvas.beginFrame(fbWidth, fbHeight);
canvas.setFillColor(vectorgl::Color::hex(0x22C55E));
canvas.fillRoundedRect(24.0f, 24.0f, 220.0f, 120.0f, 18.0f);

canvas.setStrokeColor(vectorgl::Color::White);
canvas.setLineWidth(3.0f);
canvas.strokeCircle(340.0f, 84.0f, 48.0f);
canvas.endFrame();`
    },
    {
        title: "Draw custom paths",
        text: "Use beginPath(), moveTo(), lineTo(), bezierCurveTo(), quadraticCurveTo(), arc(), and closePath() for geometry that is not a built-in SDF primitive. Paths are tessellated and then filled or stroked.",
        bullets: [
            "beginPath() resets the current path buffer.",
            "fill() uses the active fill color; stroke() uses the active stroke color and line width.",
            "Use fillWithPaint() for gradient and pattern fills."
        ],
        code: `canvas.beginPath();
canvas.moveTo(60.0f, 200.0f);
canvas.bezierCurveTo(110.0f, 140.0f, 210.0f, 260.0f, 280.0f, 190.0f);
canvas.lineTo(320.0f, 240.0f);
canvas.closePath();
canvas.fill();`
    },
    {
        title: "Render text from a TTF font",
        text: "Load a font with Canvas::setFont() once, then call fillText() for each label. Under the hood VectorGL generates an SDF atlas with stb_truetype and renders glyph quads through the textured shader.",
        bullets: [
            "setFont() expects a `.ttf` path and a pixel size.",
            "fillText() uses the current fill color.",
            "Make sure the OpenGL context is active before font loading."
        ],
        code: `canvas.setFont("assets/Inter-Regular.ttf", 24.0f);
canvas.setFillColor(vectorgl::Color::White);
canvas.fillText("VectorGL text", 48.0f, 72.0f);`
    },
    {
        title: "Render SVG assets",
        text: "Use SvgImage for one file at a time, or SvgCache when you need many icons or illustrations alive together. Both render into an existing Canvas frame.",
        bullets: [
            "SvgImage owns a single parsed document.",
            "SvgCache returns lightweight integer handles similar to NanoVG-style image handles.",
            "render() is safe only inside a valid Canvas frame."
        ],
        code: `vectorgl::SvgImage logo;
if (logo.load("assets/logo.svg")) {
    logo.render(canvas, 32.0f, 32.0f, 1.5f);
}

vectorgl::SvgCache cache;
int icon = cache.load("assets/icon.svg");
cache.render(canvas, icon, 240.0f, 32.0f, 1.0f);`
    },
    {
        title: "Use Scene nodes and hierarchy",
        text: "Use Scene for retained composition. Nodes keep transform, style, effects, visibility, z-index, and children. This is better for reusable UIs and animated object graphs than redrawing everything manually every frame.",
        bullets: [
            "Factories create the correct ShapeType automatically.",
            "Use addChild() for transform inheritance and composition.",
            "Effects such as setShadow() or setGlow() are attached to nodes."
        ],
        code: `vectorgl::Scene scene(renderer);
auto card = scene.roundedRect(80.0f, 80.0f, 280.0f, 160.0f, 14.0f);
card->setFill(vectorgl::Color::hex(0x111827));
card->setShadow(10.0f, {6.0f, 8.0f}, vectorgl::Color(0.0f, 0.0f, 0.0f, 0.25f));

auto badge = scene.circle(320.0f, 120.0f, 18.0f);
badge->setFill(vectorgl::Color::hex(0x34D399));
card->addChild(badge);`
    },
    {
        title: "Animate nodes",
        text: "Animator owns active animations and applies them to Scene nodes by ID. Use TweenAnimation for eased transitions, SpringAnimation for physically responsive movement, and KeyframeAnimation when you have multiple timed stops.",
        bullets: [
            "Animations target nodes by node->id().",
            "update(dt) advances time, applyTo(scene) writes values back.",
            "LoopMode supports one-shot, looping, and ping-pong playback."
        ],
        code: `vectorgl::AnimTarget from, to;
from.x = 80.0f;
from.mask = vectorgl::AnimTarget::PosX;
to.x = 420.0f;
to.mask = vectorgl::AnimTarget::PosX;

scene.animator().emplace<vectorgl::TweenAnimation>(
    card->id(), from, to, 0.8f, vectorgl::Ease::OutCubic);`
    }
];

const exampleFlows = [
    {
        title: "Canvas frame lifecycle",
        text: "Context current -> canvas.init() -> beginFrame() -> issue draw calls -> endFrame() -> present swap chain -> destroy() before context teardown.",
        bullets: [
            "Good default for sample apps and tools.",
            "Keep resource loading after context creation.",
            "Text and image upload both depend on OpenGL state being valid."
        ]
    },
    {
        title: "Scene render loop",
        text: "Create nodes once, update the animator each frame, then render the scene. This keeps state in nodes instead of rebuilding every primitive manually.",
        bullets: [
            "Typical order: scene.update(dt), scene.render(renderer, w, h).",
            "Use z-index for ordering between siblings and roots.",
            "Hierarchy handles parent-child transform propagation."
        ]
    },
    {
        title: "SVG asset workflow",
        text: "Use SvgImage when one SVG is loaded and rendered directly. Use SvgCache when you need many icons, want lightweight handles, or want to batch asset management through one cache object.",
        bullets: [
            "SvgCache::kInvalidHandle marks load failure.",
            "width(handle) and height(handle) expose source dimensions.",
            "clear() invalidates all previously returned handles."
        ]
    },
    {
        title: "Paint and gradients",
        text: "Build Paint descriptors with solid(), linearGradient(), radialGradient(), or pattern() and feed them into fillWithPaint() or Renderer::fillPathWithPaint() for non-flat fills.",
        bullets: [
            "Gradient stops are normalized in the 0..1 range.",
            "Pattern paints require a valid OpenGL texture ID.",
            "Paint is a descriptor object, not a long-lived GPU resource."
        ]
    }
];

const repoExampleCards = [
    {
        title: "main.cpp",
        text: "Combines immediate-mode Canvas drawing with a retained Scene. It shows SDF primitives, animated nodes, grouped circles, and direct Canvas drawing layered on top of the scene.",
        bullets: [
            "Good reference for mixing Scene and Canvas in one frame.",
            "Shows animator usage with SpringAnimation and TweenAnimation.",
            "Demonstrates how a single renderer can back both APIs."
        ],
        code: `vectorgl::Canvas canvas;
canvas.init();

vectorgl::Scene scene(canvas.renderer());
auto card = scene.roundedRect(200, 300, 300, 200, 16);

scene.update(dt);
canvas.beginFrame(fbW, fbH);
scene.render();
canvas.fillRoundedRect(-60, -40, 120, 80, 12);
canvas.endFrame();`
    },
    {
        title: "paths_demo.cpp",
        text: "Focuses on the custom path pipeline. It builds flowers, stars, gears, waves, and spirograph-like curves using path commands and then fills or strokes them.",
        bullets: [
            "Best reference for beginPath(), moveTo(), lineTo(), and curve commands.",
            "Shows when to use paths instead of SDF primitives.",
            "Includes animated transforms and layered stroke/fill usage."
        ],
        code: `canvas.beginPath();
canvas.moveTo(0, 0);
canvas.bezierCurveTo(cx1, cy1, cx2, cy2, ex, ey);
canvas.closePath();
canvas.fill();`
    },
    {
        title: "font_image_demo.cpp",
        text: "Demonstrates loading a font, drawing text at multiple sizes, showing animated labels, and rendering raster images through the Canvas image path.",
        bullets: [
            "Good reference for setFont() and fillText().",
            "Shows a practical fallback flow when no font can be loaded.",
            "Demonstrates multiple text sizes in one frame."
        ],
        code: `canvas.setFont(fontPath, 48.0f);
canvas.setFillColor(vectorgl::Color::White);
canvas.fillText("VectorGL - SDF Text Rendering", 40, 30);

canvas.drawImage(image, 800, 350, 200, 150);`
    },
    {
        title: "svg_demo.cpp",
        text: "Covers inline SVG strings, SvgImage, SvgCache, gradients, CSS-style rules, markers, symbol/use reuse, and text/tspan rendering flows.",
        bullets: [
            "Best reference for SVG integration behavior.",
            "Shows when loadFromString() is useful.",
            "Includes explicit parser coverage examples for symbol/use and text/tspan nodes."
        ],
        code: `vectorgl::SvgCache cache;
int panel = cache.loadFromString(svgMarkupWithSymbolAndText);

if (cache.valid(panel)) {
    cache.render(canvas, panel, 240.0f, 32.0f, 1.0f);
}`
    },
    {
        title: "animation_demo.cpp",
        text: "Shows the animation subsystem in isolation with springs, tweens, easing curves, orbit-like movement, and opacity pulsing patterns.",
        bullets: [
            "Good reference for AnimTarget masks and loop modes.",
            "Demonstrates multiple concurrent animations across many nodes.",
            "Useful for UI motion and scene choreography."
        ],
        code: `vectorgl::AnimTarget from, to;
from.opacity = 0.2f;
from.mask = vectorgl::AnimTarget::Opacity;
to.opacity = 1.0f;
to.mask = vectorgl::AnimTarget::Opacity;

animator.emplace<vectorgl::TweenAnimation>(
    dot->id(), from, to, duration, vectorgl::Ease::InOutQuad,
    vectorgl::LoopMode::PingPong);`
    },
    {
        title: "clipping_demo.cpp",
        text: "A focused clipping showcase with animated overflow, nested intersections, a scrolling feed, and clip state restored through save()/restore().",
        bullets: [
            "Best reference for clipRect() and resetClip().",
            "Demonstrates nested clips intersecting predictably.",
            "Shows how clipping supports cards, viewports, and scrollable UI."
        ],
        code: `canvas.save();
canvas.clipRect(panelX, panelY, panelWidth, panelHeight);
drawAnimatedContent(canvas, time);

canvas.clipRect(innerX, innerY, innerWidth, innerHeight);
drawNestedContent(canvas);
canvas.restore();`
    }
];

const apiReference = [
    {
        name: "Canvas",
        kind: "Class",
        header: "vectorgl/canvas.hpp",
        tags: ["immediate-mode", "drawing", "text", "svg", "images"],
        summary: "Primary immediate-mode drawing surface. Canvas wraps frame lifecycle, draw state, transforms, text, image drawing, and path construction. Use it when you want HTML5-style command submission each frame.",
        usage: [
            "Requires a valid OpenGL context before init(), setFont(), or image loading.",
            "All drawing must occur between beginFrame() and endFrame().",
            "Current fill/stroke state persists until changed or restored from the stack."
        ],
        groups: [
            {
                title: "Lifecycle",
                methods: [
                    { signature: "Canvas() / ~Canvas()", description: "Constructs and destroys the high-level drawing surface. Destruction does not replace an explicit destroy() call when you need deterministic OpenGL teardown." },
                    { signature: "void init()", description: "Creates shader programs, buffers, and other GPU resources through the internal Renderer. Call once after the OpenGL context is current." },
                    { signature: "void destroy()", description: "Releases GPU resources. Call before destroying the OpenGL context." },
                    { signature: "void beginFrame(int fbWidth, int fbHeight)", description: "Begins a frame, sets viewport/projection state, and prepares batching for the specified framebuffer size." },
                    { signature: "void endFrame()", description: "Flushes pending draw commands and completes the frame." }
                ]
            },
            {
                title: "Transform stack and state",
                methods: [
                    { signature: "void save() / void restore()", description: "Pushes and pops the current drawing state including transform, colors, line width, and clipping region.", notes: ["Use these when rendering nested widgets or SVG content.", "restore() only undoes changes made after the matching save()."] },
                    { signature: "void translate(float x, float y)", description: "Applies a translation to the current transform." },
                    { signature: "void rotate(float angle)", description: "Applies clockwise rotation in radians to the current transform." },
                    { signature: "void scale(float x, float y)", description: "Applies non-uniform scaling to the current transform." },
                    { signature: "void clipRect(float x, float y, float width, float height)", description: "Intersects the active clipping region with a transformed rectangle. Rotated rectangles use their transformed axis-aligned bounds.", notes: ["Width and height must be non-negative.", "Clipping affects only subsequent drawing and is restored by restore()."] },
                    { signature: "void resetClip()", description: "Removes the active clipping region for subsequent drawing." },
                    { signature: "void setFillColor(Color c)", description: "Sets the active fill color for fillRect(), fillCircle(), fill(), fillText(), and other fill operations." },
                    { signature: "void setStrokeColor(Color c)", description: "Sets the active stroke color for strokeRect(), strokeCircle(), stroke(), and other stroke operations." },
                    { signature: "void setLineWidth(float w)", description: "Sets the current stroke width in pixels." },
                    { signature: "bool setFont(const std::string& fontPath, float size)", description: "Loads a TrueType font and makes it the active font for subsequent fillText() calls.", notes: ["Returns false if the file cannot be loaded or atlas generation fails.", "This is the normal public entry point for text rendering."] }
                ]
            },
            {
                title: "SDF primitive drawing",
                methods: [
                    { signature: "void fillRect(float x, float y, float w, float h)", description: "Fills an axis-aligned rectangle through the SDF primitive pipeline." },
                    { signature: "void strokeRect(float x, float y, float w, float h)", description: "Strokes an axis-aligned rectangle using the current stroke color and line width." },
                    { signature: "void fillCircle(float cx, float cy, float r)", description: "Fills a circle using analytic signed distance evaluation in the shader." },
                    { signature: "void strokeCircle(float cx, float cy, float r)", description: "Strokes a circle using the current stroke width." },
                    { signature: "void fillEllipse(float cx, float cy, float rx, float ry)", description: "Fills an ellipse as an SDF primitive." },
                    { signature: "void strokeEllipse(float cx, float cy, float rx, float ry)", description: "Strokes an ellipse as an SDF primitive." },
                    { signature: "void fillRoundedRect(float x, float y, float w, float h, float radius)", description: "Fills a rounded rectangle with a uniform radius on all corners. Radius is clamped internally to a valid range." },
                    { signature: "void strokeRoundedRect(float x, float y, float w, float h, float radius)", description: "Strokes a rounded rectangle with a uniform corner radius." }
                ]
            },
            {
                title: "Path drawing",
                methods: [
                    { signature: "void beginPath()", description: "Clears the current path buffer." },
                    { signature: "void moveTo(float x, float y)", description: "Moves the path cursor without drawing." },
                    { signature: "void lineTo(float x, float y)", description: "Adds a line segment from the current point." },
                    { signature: "void bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)", description: "Adds a cubic Bezier segment." },
                    { signature: "void quadraticCurveTo(float cpx, float cpy, float x, float y)", description: "Adds a quadratic Bezier segment." },
                    { signature: "void arc(float cx, float cy, float r, float start, float end, bool ccw = false)", description: "Adds a circular arc segment to the current path." },
                    { signature: "void closePath()", description: "Closes the current sub-path with a straight segment back to the start." },
                    { signature: "void fill()", description: "Fills the current path using the current fill color." },
                    { signature: "void fillWithPaint(const Paint& paint, float opacity = 1.0f)", description: "Fills the current path with a solid, gradient, or pattern paint descriptor." },
                    { signature: "void stroke()", description: "Strokes the current path with the current stroke color and line width." }
                ]
            },
            {
                title: "Text and images",
                methods: [
                    { signature: "void fillText(const std::string& text, float x, float y)", description: "Renders UTF-8 text at the baseline position using the active font and the current fill color.", notes: ["Requires setFont() to succeed first.", "Text is rendered from an SDF atlas for smooth edges across sizes."] },
                    { signature: "Image loadImage(const std::string& path)", description: "Convenience helper that loads a raster image from disk and returns an Image object." },
                    { signature: "void drawImage(const Image& img, float x, float y, float w = -1, float h = -1)", description: "Draws a raster image at the given destination. When width or height is -1 the original image size is used." },
                    { signature: "Renderer& renderer()", description: "Returns the underlying Renderer instance when you need lower-level access or want to bind a Scene to the same renderer." }
                ]
            }
        ]
    },
    {
        name: "Scene",
        kind: "Class",
        header: "vectorgl/scene.hpp",
        tags: ["retained-mode", "nodes", "animation", "hierarchy"],
        summary: "Retained-mode scene graph with typed node factories, animation integration, and hierarchical composition.",
        usage: [
            "Use Scene when geometry and styling should live longer than a single frame.",
            "Shape factory calls create nodes with the correct ShapeType and initial geometry.",
            "Call update(dt) before render() if animations are active."
        ],
        groups: [
            {
                title: "Construction and rendering",
                methods: [
                    { signature: "Scene()", description: "Creates an unbound scene. Use render(Renderer&, width, height) when no renderer was supplied at construction." },
                    { signature: "explicit Scene(Renderer& renderer)", description: "Binds a renderer so parameterless render() can be used." },
                    { signature: "void update(float dt)", description: "Advances active animations and refreshes world transforms for root nodes." },
                    { signature: "void render(Renderer& renderer, int fbWidth, int fbHeight)", description: "Renders the scene through the provided renderer and framebuffer size." },
                    { signature: "void render()", description: "Renders using the renderer passed to the constructor. Does nothing if the scene is unbound." },
                    { signature: "Animator& animator()", description: "Returns the built-in Animator used to manage node animations." }
                ]
            },
            {
                title: "Shape factories",
                methods: [
                    { signature: "std::shared_ptr<Node> rect(float x, float y, float w, float h)", description: "Creates a rectangle node." },
                    { signature: "std::shared_ptr<Node> roundedRect(float x, float y, float w, float h, float radius)", description: "Creates a rounded rectangle node with a uniform corner radius." },
                    { signature: "std::shared_ptr<Node> circle(float cx, float cy, float r)", description: "Creates a circle node." },
                    { signature: "std::shared_ptr<Node> ellipse(float cx, float cy, float rx, float ry)", description: "Creates an ellipse node." },
                    { signature: "std::shared_ptr<Node> line(float x1, float y1, float x2, float y2)", description: "Creates a line node. The line is represented as a length plus rotation internally." },
                    { signature: "std::shared_ptr<Node> path(const Path2D& path)", description: "Creates a complex path node from a Path2D object." },
                    { signature: "std::shared_ptr<Node> group()", description: "Creates an empty group node for layout and transform inheritance without direct drawing." }
                ]
            },
            {
                title: "Root management",
                methods: [
                    { signature: "void addRoot(std::shared_ptr<Node> node)", description: "Adds a node to the scene root list." },
                    { signature: "void removeRoot(const std::shared_ptr<Node>& node)", description: "Removes a node from the root list and node lookup map." },
                    { signature: "std::shared_ptr<Node> findNode(uint32_t id) const", description: "Finds a node by its unique ID." },
                    { signature: "const std::vector<std::shared_ptr<Node>>& roots() const", description: "Returns the root node list." },
                    { signature: "void collectRenderList(std::vector<Node*>& outList)", description: "Flattens visible renderable nodes into a list, useful for inspection or custom debug flows." }
                ]
            }
        ]
    },
    {
        name: "Renderer",
        kind: "Class",
        header: "vectorgl/renderer.hpp",
        tags: ["low-level", "gpu", "batching", "effects"],
        summary: "Low-level GPU interface for SDF primitives, tessellated path drawing, textured quads, glyphs, and post-processing effects. Most users should start with Canvas or Scene and only drop to Renderer when they need custom control.",
        usage: [
            "Renderer is move-only.",
            "Use drawSDF*() for direct primitive batching or renderNode() to draw a Scene node.",
            "flushSDF() is required whenever you need batched primitives emitted before switching pipelines."
        ],
        groups: [
            {
                title: "Lifecycle and frame",
                methods: [
                    { signature: "void init() / void destroy()", description: "Create and release GPU resources such as shaders, VAOs, and FBOs." },
                    { signature: "void beginFrame(int fbWidth, int fbHeight)", description: "Starts a frame and resets batching state for the given framebuffer size." },
                    { signature: "void endFrame()", description: "Completes the frame and flushes remaining batched SDF primitives." },
                    { signature: "int fbWidth() const / int fbHeight() const", description: "Returns the current framebuffer size tracked by the renderer." }
                ]
            },
            {
                title: "Clipping",
                methods: [
                    { signature: "void setClipRect(int x, int y, int width, int height)", description: "Sets a framebuffer-space scissor rectangle using VectorGL's top-left coordinate system. Pending SDF work is flushed before the state change." },
                    { signature: "void clearClip()", description: "Disables scissor clipping after flushing pending SDF work." }
                ]
            },
            {
                title: "SDF primitive batching",
                methods: [
                    { signature: "void drawSDFRect(Vec2 pos, Vec2 size, const NodeStyle& style, const Mat3x3& transform)", description: "Submits an SDF rectangle instance to the batch." },
                    { signature: "void drawSDFCircle(Vec2 center, float radius, const NodeStyle& style, const Mat3x3& transform)", description: "Submits an SDF circle instance to the batch." },
                    { signature: "void drawSDFEllipse(Vec2 center, Vec2 radii, const NodeStyle& style, const Mat3x3& transform)", description: "Submits an SDF ellipse instance to the batch." },
                    { signature: "void drawSDFRoundedRect(Vec2 pos, Vec2 size, const std::array<float, 4>& cornerRadii, const NodeStyle& style, const Mat3x3& transform)", description: "Submits an SDF rounded rectangle instance with per-corner radii." },
                    { signature: "void flushSDF()", description: "Emits the current SDF batch to the GPU." }
                ]
            },
            {
                title: "Paths, textures, effects, nodes",
                methods: [
                    { signature: "void fillPath(const std::vector<std::vector<Vec2>>& subPaths, Color color, const Mat3x3& transform)", description: "Fills tessellated path geometry with a solid color." },
                    { signature: "void fillPathWithPaint(const std::vector<std::vector<Vec2>>& subPaths, const Paint& paint, float opacity, const Mat3x3& transform)", description: "Fills path geometry with a Paint descriptor such as a gradient or pattern." },
                    { signature: "void strokePath(const std::vector<std::vector<Vec2>>& subPaths, Color color, float width, const Mat3x3& transform)", description: "Strokes tessellated path outlines." },
                    { signature: "void drawTexturedQuad(float x, float y, float w, float h, uint32_t texture, Color tint, const Mat3x3& transform)", description: "Draws a raster-textured quad." },
                    { signature: "void drawGlyph(float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t texture, Color color, const Mat3x3& transform)", description: "Draws a single font atlas glyph quad." },
                    { signature: "void beginEffectPass(float x, float y, float w, float h) / void endEffectPass()", description: "Captures subsequent drawing into an off-screen FBO region for post-processing." },
                    { signature: "void applyBlur(float radius) / applyShadow(float blur, Vec2 offset, Color color) / applyGlow(float radius, Color color)", description: "Applies FBO-based blur, shadow, and glow passes." },
                    { signature: "void renderNode(Node* node)", description: "Draws one Scene node according to its ShapeType, style, and world transform." }
                ]
            }
        ]
    },
    {
        name: "Path2D",
        kind: "Class",
        header: "vectorgl/path.hpp",
        tags: ["path", "geometry", "bezier", "svg-like"],
        summary: "HTML5 Canvas-style path builder used by Canvas and Scene path nodes for custom geometry.",
        usage: [
            "Path2D stores commands and points, then exposes sub-paths for tessellation.",
            "Use it when your geometry cannot be expressed as a simple SDF primitive.",
            "Paths are cursor-based, so drawing order matters."
        ],
        groups: [
            {
                title: "Building paths",
                methods: [
                    { signature: "void moveTo(float x, float y)", description: "Starts a new sub-path at the given point." },
                    { signature: "void lineTo(float x, float y)", description: "Adds a straight line segment." },
                    { signature: "void bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)", description: "Adds a cubic Bezier curve using adaptive subdivision internally." },
                    { signature: "void quadraticCurveTo(float cpx, float cpy, float x, float y)", description: "Adds a quadratic Bezier curve." },
                    { signature: "void arc(float cx, float cy, float r, float startAngle, float endAngle, bool ccw = false)", description: "Adds a circular arc." },
                    { signature: "void ellipse(float cx, float cy, float rx, float ry, float rotation, float startAngle, float endAngle, bool ccw = false)", description: "Adds an elliptical arc with rotation." },
                    { signature: "void rect(float x, float y, float w, float h)", description: "Adds a rectangle as a closed sub-path." },
                    { signature: "void closePath()", description: "Closes the current sub-path." }
                ]
            },
            {
                title: "Utility and output",
                methods: [
                    { signature: "void clear()", description: "Clears all stored path commands and resets the internal cursor." },
                    { signature: "const std::vector<PathPoint>& points() const", description: "Returns the raw point command list." },
                    { signature: "bool empty() const", description: "Reports whether the path contains any commands." },
                    { signature: "std::vector<std::vector<Vec2>> getSubPaths() const", description: "Expands the command list into concrete sub-path point arrays for tessellation and rendering." }
                ]
            }
        ]
    },
    {
        name: "Node",
        kind: "Class",
        header: "vectorgl/node.hpp",
        tags: ["scene", "node", "effects", "transform"],
        summary: "Scene graph element representing a shape, path, line, group, text slot, or image slot with transform, style, effects, visibility, hierarchy, and cached world transforms.",
        usage: [
            "Most nodes should be created through Scene factory methods rather than direct construction.",
            "setPath() is meaningful when the node type is ShapeType::Path.",
            "Effects are descriptors stored on the node and interpreted by the renderer."
        ],
        groups: [
            {
                title: "Identity and transform",
                methods: [
                    { signature: "uint32_t id() const / ShapeType type() const", description: "Returns the stable node ID and node type." },
                    { signature: "void setPosition(float x, float y)", description: "Sets local position." },
                    { signature: "void setSize(float w, float h)", description: "Sets local dimensions." },
                    { signature: "void setRotation(float radians)", description: "Sets clockwise local rotation in radians." },
                    { signature: "void setScale(float sx, float sy)", description: "Sets non-uniform local scale." },
                    { signature: "Vec2 position() const / Vec2 size() const / float rotation() const", description: "Returns current transform properties." },
                    { signature: "const Mat3x3& worldTransform() const / void updateWorldTransform(const Mat3x3& parentTransform)", description: "Accesses or recomputes the cached world transform." }
                ]
            },
            {
                title: "Style and effects",
                methods: [
                    { signature: "void setFill(Color c)", description: "Sets fill color." },
                    { signature: "void setStroke(Color c, float width)", description: "Sets stroke color and stroke width." },
                    { signature: "void setOpacity(float o)", description: "Sets node opacity." },
                    { signature: "void setCornerRadii(float tl, float tr, float br, float bl)", description: "Sets per-corner radii for rounded rectangles." },
                    { signature: "void setCornerRadius(float r)", description: "Sets the same radius on all four corners." },
                    { signature: "const NodeStyle& style() const / NodeStyle& style()", description: "Returns the style object for inspection or direct editing." },
                    { signature: "void setShadow(float blur, Vec2 offset, Color color)", description: "Applies a shadow descriptor." },
                    { signature: "void setBlur(float radius)", description: "Applies a blur descriptor." },
                    { signature: "void setGlow(float radius, Color color)", description: "Applies a glow descriptor." },
                    { signature: "void clearEffects() / const NodeEffects& effects() const / bool hasEffects() const", description: "Clears or inspects active effect descriptors." }
                ]
            },
            {
                title: "Hierarchy, geometry, visibility",
                methods: [
                    { signature: "void addChild(std::shared_ptr<Node> child) / void removeChild(const std::shared_ptr<Node>& child)", description: "Adds or removes a child node for hierarchical composition." },
                    { signature: "const std::vector<std::shared_ptr<Node>>& children() const / Node* parent() const", description: "Inspects the hierarchy." },
                    { signature: "void setZIndex(int z) / int zIndex() const", description: "Controls render ordering. Higher z-index values are drawn later." },
                    { signature: "void setPath(const Path2D& path) / const Path2D& path() const", description: "Sets or returns the node path geometry." },
                    { signature: "DirtyFlag dirtyFlags() const / void markDirty(DirtyFlag flags) / void clearDirty() / bool isDirty() const", description: "Dirty tracking helpers used to know when transforms, style, or geometry changed." },
                    { signature: "bool visible() const / void setVisible(bool v)", description: "Shows or hides the node from rendering." }
                ]
            }
        ]
    },
    {
        name: "Animator and animation types",
        kind: "Class group",
        header: "vectorgl/animator.hpp",
        tags: ["animation", "tween", "spring", "keyframe"],
        summary: "Animation subsystem for driving Scene nodes over time. Includes easing evaluation, loop modes, target property snapshots, and concrete animation classes.",
        usage: [
            "All animations target nodes by node ID, not by pointer.",
            "AnimTarget.mask selects which properties participate in interpolation.",
            "Use Animator::add() with std::make_unique<...>() to transfer ownership."
        ],
        groups: [
            {
                title: "Enums and target data",
                methods: [
                    { signature: "enum class Ease", description: "Defines easing curves such as Linear, InQuad, OutCubic, InOutBack, InElastic, OutBounce, and more." },
                    { signature: "float evalEasing(Ease ease, float t)", description: "Evaluates an easing curve at normalized time t." },
                    { signature: "enum class LoopMode { None, Loop, PingPong }", description: "Controls playback behavior when an animation reaches the end." },
                    { signature: "struct AnimTarget", description: "Snapshot of animatable node properties such as x, y, width, height, rotation, scale, opacity, fill, and stroke. Use the Prop mask bits to mark active fields." }
                ]
            },
            {
                title: "Animation base and concrete types",
                methods: [
                    { signature: "class Animation", description: "Abstract base with update(dt), apply(Node&), isFinished(), and targetNodeId()." },
                    { signature: "TweenAnimation(uint32_t nodeId, AnimTarget from, AnimTarget to, float duration, Ease easing, LoopMode loop = LoopMode::None)", description: "Duration-based interpolation with easing and optional loop or ping-pong." },
                    { signature: "SpringAnimation(uint32_t nodeId, AnimTarget target, float stiffness = 200.0f, float damping = 20.0f)", description: "Physically-based motion that converges toward a target state." },
                    { signature: "struct Keyframe { float time; AnimTarget target; }", description: "One stop in a keyframe animation." },
                    { signature: "KeyframeAnimation(uint32_t nodeId, std::vector<Keyframe> keyframes, LoopMode loop = LoopMode::None)", description: "Interpolates between keyed targets over time." }
                ]
            },
            {
                title: "Animator engine",
                methods: [
                    { signature: "void add(std::unique_ptr<Animation> anim)", description: "Adds an animation and transfers ownership to the Animator." },
                    { signature: "void update(float dt)", description: "Advances all animations by the supplied delta time." },
                    { signature: "void applyTo(Scene& scene)", description: "Writes the current interpolated animation state to target nodes in the scene." },
                    { signature: "void clear()", description: "Removes all active animations." },
                    { signature: "bool hasActiveAnimations() const", description: "Returns true while any animations are still alive." }
                ]
            }
        ]
    },
    {
        name: "Paint and gradient types",
        kind: "Struct group",
        header: "vectorgl/paint.hpp",
        tags: ["paint", "gradient", "pattern", "fill"],
        summary: "Descriptor types used to describe how geometry should be filled. Paint values can represent solid colors, linear gradients, radial gradients, and texture patterns.",
        usage: [
            "Paint is a lightweight descriptor object and can be created on the stack.",
            "Use with Canvas::fillWithPaint() or Renderer::fillPathWithPaint().",
            "Pattern paints require a valid OpenGL texture ID."
        ],
        groups: [
            {
                title: "Enums and supporting structs",
                methods: [
                    { signature: "enum class PaintType { Solid, LinearGradient, RadialGradient, Pattern }", description: "Identifies the fill strategy represented by a Paint instance." },
                    { signature: "enum class PatternRepeat { None, X, Y, Both }", description: "Controls pattern repetition axes." },
                    { signature: "struct GradientStop { float position; Color color; }", description: "One normalized stop within a gradient definition." }
                ]
            },
            {
                title: "Paint factories",
                methods: [
                    { signature: "static Paint solid(Color c)", description: "Creates a solid-color paint." },
                    { signature: "static Paint linearGradient(Vec2 start, Vec2 end, std::vector<GradientStop> stops)", description: "Creates a linear gradient between two points." },
                    { signature: "static Paint radialGradient(Vec2 center, float inner, float outer, std::vector<GradientStop> stops)", description: "Creates a radial gradient centered at a point with inner and outer radii." },
                    { signature: "static Paint pattern(uint32_t texture, PatternRepeat repeat = PatternRepeat::Both)", description: "Creates a texture pattern paint from a texture ID." }
                ]
            }
        ]
    },
    {
        name: "Color",
        kind: "Struct",
        header: "vectorgl/color.hpp",
        tags: ["color", "utility", "math"],
        summary: "RGBA color helper with float components, factories for 8-bit or hex input, interpolation helpers, and predefined constants.",
        usage: [
            "Most APIs accept Color directly by value.",
            "Use hex(0xRRGGBB) for concise authoring.",
            "lerpOklab() is better than plain RGB interpolation when perceptual smoothness matters."
        ],
        groups: [
            {
                title: "Construction and helpers",
                methods: [
                    { signature: "Color(float r, float g, float b, float a = 1.0f)", description: "Constructs a color from float components in the 0..1 range." },
                    { signature: "static constexpr Color rgba(uint8_t r, uint8_t g, uint8_t b, float a = 1.0f)", description: "Creates a color from 8-bit RGB values." },
                    { signature: "static constexpr Color rgba(float r, float g, float b, float a)", description: "Creates a color from float RGBA values." },
                    { signature: "static constexpr Color hex(uint32_t h) / hex(uint32_t h, float a)", description: "Creates a color from a 24-bit hex RGB value, optionally with custom alpha." },
                    { signature: "static Color lerp(Color a, Color b, float t)", description: "Linearly interpolates two colors in RGB space." },
                    { signature: "static Color lerpOklab(Color a, Color b, float t)", description: "Interpolates two colors in Oklab space for more perceptually uniform transitions." },
                    { signature: "Color withAlpha(float newAlpha) const / Color premultiplied() const", description: "Returns alpha-adjusted or premultiplied variants of a color." }
                ]
            },
            {
                title: "Predefined constants",
                methods: [
                    { signature: "Color::Black, White, Red, Green, Blue, Yellow, Cyan, Magenta, Transparent, Gray, DarkGray, LightGray, Orange", description: "Common built-in colors ready for direct use." }
                ]
            }
        ]
    },
    {
        name: "Font and GlyphInfo",
        kind: "Class group",
        header: "vectorgl/font.hpp",
        tags: ["font", "text", "sdf", "atlas"],
        summary: "TrueType font atlas support and glyph metrics access. Font is used internally by Canvas::setFont() and fillText(), but is also available directly when lower-level text integration is needed.",
        usage: [
            "Font is move-only.",
            "load() generates an SDF atlas texture.",
            "getGlyph() returns atlas UVs, offsets, advance, and glyph quad size."
        ],
        groups: [
            {
                title: "Glyph metrics",
                methods: [
                    { signature: "struct GlyphInfo { u0, v0, u1, v1, xoff, yoff, xadvance, width, height }", description: "Stores atlas UV coordinates plus per-glyph positioning and advance metrics." }
                ]
            },
            {
                title: "Font methods",
                methods: [
                    { signature: "bool load(const std::string& path, float size)", description: "Loads a `.ttf` font file and generates the atlas texture." },
                    { signature: "void destroy()", description: "Releases the atlas texture and cached font data." },
                    { signature: "const GlyphInfo* getGlyph(int codepoint) const", description: "Returns glyph metrics for a Unicode codepoint or nullptr if missing." },
                    { signature: "uint32_t atlasTexture() const", description: "Returns the OpenGL texture ID for the atlas." },
                    { signature: "float lineHeight() const / float ascent() const / float renderScale() const", description: "Returns font metrics and the render scale used during SDF generation." }
                ]
            }
        ]
    },
    {
        name: "Image",
        kind: "Class",
        header: "vectorgl/image.hpp",
        tags: ["image", "texture", "asset"],
        summary: "Move-only GPU-resident raster image abstraction used for loading and drawing PNG, JPG, BMP, and TGA files.",
        usage: [
            "Use load() directly or Canvas::loadImage().",
            "Always check valid() before drawing.",
            "Call destroy() for deterministic GPU cleanup if required."
        ],
        groups: [
            {
                title: "Image methods",
                methods: [
                    { signature: "bool load(const std::string& path)", description: "Loads an image from disk and uploads it to the GPU." },
                    { signature: "void destroy()", description: "Releases the texture. Safe to call multiple times." },
                    { signature: "uint32_t texture() const", description: "Returns the OpenGL texture ID." },
                    { signature: "int width() const / int height() const", description: "Returns image dimensions in pixels." },
                    { signature: "bool valid() const", description: "Returns true if a texture is loaded." }
                ]
            }
        ]
    },
    {
        name: "SvgImage",
        kind: "Class",
        header: "vectorgl/svg.hpp",
        tags: ["svg", "asset", "canvas"],
        summary: "Single-document SVG wrapper. Owns one parsed SVG DOM tree and renders it into a Canvas.",
        usage: [
            "Prefer SvgImage when one SVG asset maps cleanly to one object in your app.",
            "loadFromString() is useful for embedded assets or generated SVG.",
            "render() preserves the Canvas transform stack for you."
        ],
        groups: [
            {
                title: "SVG image methods",
                methods: [
                    { signature: "bool load(const std::string& path)", description: "Loads and parses an SVG file from disk." },
                    { signature: "bool loadFromString(const std::string& svgData)", description: "Parses an SVG document from an in-memory string." },
                    { signature: "void render(Canvas& canvas, float x = 0, float y = 0, float scale = 1.0f) const", description: "Renders the SVG into an existing Canvas frame." },
                    { signature: "float width() const / float height() const", description: "Returns the intrinsic SVG width and height in user units." },
                    { signature: "bool valid() const", description: "Reports whether a valid SVG document is loaded." },
                    { signature: "void destroy()", description: "Resets the object to an unloaded state and releases resources." }
                ]
            }
        ]
    },
    {
        name: "SvgCache",
        kind: "Class",
        header: "vectorgl/svg_cache.hpp",
        tags: ["svg", "cache", "asset-manager"],
        summary: "Handle-based manager for many SVG assets. Stores SVGs internally and returns lightweight integer handles instead of one object per asset.",
        usage: [
            "Best when your app needs lots of icons or SVG assets reused over time.",
            "Invalid handles are safe no-ops for render() and return zero-ish query values.",
            "Handles may be recycled after remove()."
        ],
        groups: [
            {
                title: "Cache methods",
                methods: [
                    { signature: "static constexpr int kInvalidHandle = -1", description: "Sentinel value returned when loading fails." },
                    { signature: "int load(const std::string& path) / int loadFromString(const std::string& svgData)", description: "Loads an SVG from disk or memory and returns a non-negative handle on success." },
                    { signature: "void render(Canvas& canvas, int handle, float x = 0, float y = 0, float scale = 1.0f) const", description: "Renders an SVG by handle." },
                    { signature: "float width(int handle) const / float height(int handle) const", description: "Returns intrinsic dimensions for a handle." },
                    { signature: "bool valid(int handle) const", description: "Checks whether a handle currently refers to a loaded SVG." },
                    { signature: "void remove(int handle)", description: "Removes one SVG and recycles its slot." },
                    { signature: "void clear()", description: "Removes all SVGs and invalidates all handles." },
                    { signature: "int count() const", description: "Returns the number of active cached SVGs." }
                ]
            }
        ]
    },
    {
        name: "Effects, math, and node support types",
        kind: "Struct group",
        header: "vectorgl/effects.hpp + vectorgl/node.hpp",
        tags: ["effects", "math", "support-types"],
        summary: "Supporting public types used across the API surface, including EffectChain, Vec2, Mat3x3, DirtyFlag, ShapeType, NodeStyle, and NodeEffects.",
        usage: [
            "Vec2 and Mat3x3 are used by path, paint, renderer, and node APIs.",
            "EffectChain is a fluent descriptor, while NodeEffects stores per-node effect state.",
            "ShapeType identifies how a node is rendered."
        ],
        groups: [
            {
                title: "Effects",
                methods: [
                    { signature: "struct EffectChain", description: "Fluent builder for post-processing descriptors. Use empty(), shadow(), blur(), and glow()." },
                    { signature: "EffectChain& shadow(float blur, Vec2 offset, Color color)", description: "Enables shadow settings and returns the same chain." },
                    { signature: "EffectChain& blur(float radius)", description: "Enables blur settings." },
                    { signature: "EffectChain& glow(float radius, Color color)", description: "Enables glow settings." },
                    { signature: "bool empty() const", description: "Checks whether the chain has any configured effects." },
                    { signature: "struct ShadowEffect / BlurEffect / GlowEffect / NodeEffects", description: "Low-level descriptor storage for node-attached effect state." }
                ]
            },
            {
                title: "Math and enums",
                methods: [
                    { signature: "struct Vec2", description: "2D vector with arithmetic, length(), normalized(), and perp() helpers." },
                    { signature: "struct Mat3x3", description: "3x3 affine transform matrix with identity(), translation(), rotation(), scaling(), operator*(), and transformPoint()." },
                    { signature: "enum class DirtyFlag", description: "Bitflags that classify transform, style, and geometry dirtiness." },
                    { signature: "enum class ShapeType", description: "Values: None, Rect, RoundedRect, Circle, Ellipse, Line, Path, Text, Image, Group." },
                    { signature: "struct NodeStyle", description: "Holds fillColor, strokeColor, strokeWidth, opacity, and per-corner radii." }
                ]
            }
        ]
    }
];

function createCard({ title, text, bullets, href }) {
    const article = document.createElement("article");
    article.className = "feature-card";
    article.innerHTML = `
        <h3>${title}</h3>
        <p>${text}</p>
        <ul class="topic-list">
            ${bullets.map((bullet) => `<li>${bullet}</li>`).join("")}
        </ul>
        ${href ? `<div class="mt-3"><a class="docs-btn docs-btn-secondary" href="${href}">Open page</a></div>` : ""}
    `;
    return article;
}

const CODE_KEYWORDS = [
    "if", "else", "for", "while", "return", "auto", "const", "bool", "float", "int",
    "void", "class", "struct", "enum", "public", "private", "protected", "true", "false",
    "nullptr", "static", "using", "namespace", "include"
];

const CODE_TYPES = [
    "Canvas", "Scene", "Renderer", "Color", "Paint", "Image", "Font", "Path2D",
    "AnimTarget", "TweenAnimation", "SpringAnimation", "KeyframeAnimation", "LoopMode", "Ease"
];

function inferCodeFileName(title) {
    if (!title) {
        return "example.cpp";
    }

    if (/\.[a-z0-9]+$/i.test(title)) {
        return title;
    }

    return `${title.toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_|_$/g, "") || "example"}.cpp`;
}

function highlightCode(code) {
    const placeholders = [];
    const stash = (className, value) => {
        let index = placeholders.length;
        let label = "";
        do {
            label = String.fromCharCode(65 + (index % 26)) + label;
            index = Math.floor(index / 26) - 1;
        } while (index >= 0);
        const token = `@@CODE_TOKEN_${label}@@`;
        placeholders.push({ token, html: `<span class="${className}">${value}</span>` });
        return token;
    };

    let html = escapeHtml(code);

    html = html.replace(/(\/\/.*)$/gm, (match) => stash("code-inline-comment", match));
    html = html.replace(/(&quot;.*?&quot;)/g, (match) => stash("code-string", match));
    html = html.replace(/\b(\d+(?:\.\d+)?f?)\b/g, (match) => stash("code-number", match));
    html = html.replace(/\b(vectorgl)(?=::)/g, (match) => stash("code-type", match));

    const keywordPattern = new RegExp(`\\b(${CODE_KEYWORDS.join("|")})\\b`, "g");
    const typePattern = new RegExp(`\\b(${CODE_TYPES.join("|")})\\b`, "g");

    html = html.replace(keywordPattern, (match) => stash("code-keyword", match));
    html = html.replace(typePattern, (match) => stash("code-type", match));
    html = html.replace(/\b([A-Za-z_][A-Za-z0-9_]*)\s*(?=\()/g, (match) => stash("code-call", match));

    for (const placeholder of placeholders) {
        html = html.replaceAll(placeholder.token, placeholder.html);
    }

    return html;
}

function renderCodeBlock(code, fileName) {
    return `
        <div class="code-block">
            <button class="copy-button" data-copy-text="${escapeAttribute(code)}">Copy</button>
            <div class="code-header"><span class="traffic-dots"><i></i><i></i><i></i></span><span class="code-file">${escapeHtml(fileName)}</span></div>
            <pre><code>${highlightCode(code)}</code></pre>
        </div>`;
}

function createGuideCard({ title, text, bullets, code }) {
    const article = document.createElement("article");
    article.className = "guide-card";
    const codeBlock = code ? renderCodeBlock(code, inferCodeFileName(title)) : "";

    article.innerHTML = `
        <h3>${title}</h3>
        <p>${text}</p>
        <ul class="topic-list">
            ${bullets.map((bullet) => `<li>${bullet}</li>`).join("")}
        </ul>
        ${codeBlock}
    `;
    return article;
}

function createMethodCard(method) {
    const notes = method.notes && method.notes.length
        ? `<ul class="method-notes">${method.notes.map((note) => `<li>${note}</li>`).join("")}</ul>`
        : "";
    const example = method.example
        ? `<div class="method-example">${method.example}</div>`
        : "";

    return `
        <div class="method-card">
            <div class="method-signature">${escapeHtml(method.signature)}</div>
            <div class="method-description">${method.description}</div>
            ${notes}
            ${example}
        </div>
    `;
}

function renderApi(items) {
    const container = document.getElementById("api-container");
    container.innerHTML = "";

    if (!items.length) {
        const empty = document.createElement("div");
        empty.className = "api-block";
        empty.innerHTML = `<h3 class="api-title">No matching API items</h3><p class="api-summary">Try a broader search term such as canvas, svg, animation, font, or path.</p>`;
        container.appendChild(empty);
        return;
    }

    items.forEach((item) => {
        const block = document.createElement("section");
        block.className = "api-block";
        block.innerHTML = `
            <div class="api-header">
                <div>
                    <h3 class="api-title">${item.name}</h3>
                    <p class="api-summary">${item.summary}</p>
                </div>
                <div class="api-metadata">
                    <span class="meta-pill">${item.kind}</span>
                    <span class="meta-pill">${item.header}</span>
                </div>
            </div>
            <ul class="topic-list">
                ${item.usage.map((line) => `<li>${line}</li>`).join("")}
            </ul>
            ${item.groups.map((group) => `
                <div class="method-group">
                    <div class="method-group-title">${group.title}</div>
                    ${group.methods.map(createMethodCard).join("")}
                </div>
            `).join("")}
        `;
        container.appendChild(block);
    });
}

function escapeHtml(value) {
    return value
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;")
        .replaceAll("'", "&#39;");
}

function escapeAttribute(value) {
    return escapeHtml(value).replaceAll("\n", "&#10;");
}

function applyTheme(theme) {
    document.body.dataset.theme = theme;
    document.querySelectorAll("[data-theme-toggle]").forEach((button) => {
        button.textContent = theme === "light" ? "Dark mode" : "Light mode";
    });
}

function installThemeToggle() {
    const preferredTheme = localStorage.getItem("vectorgl-docs-theme") || "dark";
    applyTheme(preferredTheme);

    document.querySelectorAll("[data-theme-toggle]").forEach((button) => {
        button.addEventListener("click", () => {
            const nextTheme = document.body.dataset.theme === "light" ? "dark" : "light";
            localStorage.setItem("vectorgl-docs-theme", nextTheme);
            applyTheme(nextTheme);
        });
    });
}

function installRailTracking() {
    const sections = Array.from(document.querySelectorAll("section[id]"));
    const railLinks = Array.from(document.querySelectorAll(".rail-link"));
    if (!sections.length || !railLinks.length || typeof IntersectionObserver === "undefined") {
        return;
    }

    const linkMap = new Map(railLinks.map((link) => [link.getAttribute("href"), link]));
    const observer = new IntersectionObserver((entries) => {
        const visibleEntry = entries
            .filter((entry) => entry.isIntersecting)
            .sort((left, right) => right.intersectionRatio - left.intersectionRatio)[0];

        if (!visibleEntry) {
            return;
        }

        railLinks.forEach((link) => link.classList.remove("is-active"));
        const activeLink = linkMap.get(`#${visibleEntry.target.id}`);
        activeLink?.classList.add("is-active");
    }, { rootMargin: "-25% 0px -55% 0px", threshold: [0.1, 0.3, 0.6] });

    sections.forEach((section) => observer.observe(section));
}

function upgradeStaticCodeBlocks() {
    document.querySelectorAll(".code-block").forEach((block) => {
        const codeElement = block.querySelector("code");
        if (!codeElement) {
            return;
        }

        if (!block.querySelector(".code-header")) {
            const header = document.createElement("div");
            header.className = "code-header";
            header.innerHTML = '<span class="traffic-dots"><i></i><i></i><i></i></span><span class="code-file">snippet.cpp</span>';
            const pre = block.querySelector("pre");
            if (pre) {
                block.insertBefore(header, pre);
            }
        }

        if (!codeElement.dataset.highlighted) {
            codeElement.innerHTML = highlightCode(codeElement.textContent || "");
            codeElement.dataset.highlighted = "true";
        }
    });
}

function installCopyHandlers() {
    document.querySelectorAll(".copy-button").forEach((button) => {
        button.addEventListener("click", async () => {
            const targetId = button.dataset.copyTarget;
            const inlineText = button.dataset.copyText;
            const text = targetId
                ? document.getElementById(targetId)?.textContent ?? ""
                : inlineText ?? "";

            try {
                await navigator.clipboard.writeText(text);
                const previous = button.textContent;
                button.textContent = "Copied";
                setTimeout(() => {
                    button.textContent = previous;
                }, 1200);
            } catch {
                button.textContent = "Copy failed";
            }
        });
    });
}

function installSearch() {
    const input = document.getElementById("api-search");
    input.addEventListener("input", () => {
        const query = input.value.trim().toLowerCase();
        if (!query) {
            renderApi(apiReference);
            installCopyHandlers();
            return;
        }

        const filtered = apiReference.filter((item) => {
            const haystack = [
                item.name,
                item.kind,
                item.header,
                item.summary,
                ...item.tags,
                ...item.usage,
                ...item.groups.flatMap((group) => [
                    group.title,
                    ...group.methods.flatMap((method) => [method.signature, method.description, ...(method.notes || [])])
                ])
            ].join(" ").toLowerCase();
            return haystack.includes(query);
        });

        renderApi(filtered);
        installCopyHandlers();
    });
}

function init() {
    installThemeToggle();

    const homeGrid = document.getElementById("home-page-grid");
    if (homeGrid) {
        homePageCards.forEach((card) => homeGrid.appendChild(createCard(card)));
    }

    const featureGrid = document.getElementById("feature-grid");
    if (featureGrid) {
        featureCards.forEach((card) => featureGrid.appendChild(createCard(card)));
    }

    const guidesGrid = document.getElementById("guides-grid");
    if (guidesGrid) {
        guides.forEach((guide) => guidesGrid.appendChild(createGuideCard(guide)));
    }

    const exampleGrid = document.getElementById("example-flow-grid");
    if (exampleGrid) {
        exampleFlows.forEach((flow) => exampleGrid.appendChild(createCard(flow)));
    }

    const repoExampleGrid = document.getElementById("repo-example-grid");
    if (repoExampleGrid) {
        repoExampleCards.forEach((item) => repoExampleGrid.appendChild(createGuideCard(item)));
    }

    const apiContainer = document.getElementById("api-container");
    if (apiContainer) {
        renderApi(apiReference);
        installSearch();
    }

    upgradeStaticCodeBlocks();
    installCopyHandlers();
    installRailTracking();
}

init();
