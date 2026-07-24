#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <cmath>
#include <iostream>
#include <string>
#include <vectorgl/canvas.hpp>
#include <vectorgl/svg.hpp>
#include <vectorgl/svg_cache.hpp>

// Inline SVG test data — a collection of SVG shapes to demonstrate parsing

static const char* kSvgLogo = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="200" height="200" viewBox="0 0 200 200">
  <circle cx="100" cy="100" r="90" fill="#1a1a2e" stroke="#16213e" stroke-width="4"/>
  <circle cx="100" cy="100" r="70" fill="none" stroke="#0f3460" stroke-width="2"/>
  <path d="M100 30 L130 90 L170 100 L130 110 L100 170 L70 110 L30 100 L70 90 Z"
        fill="#e94560" stroke="#ffffff" stroke-width="2"/>
  <circle cx="100" cy="100" r="20" fill="#533483"/>
  <circle cx="100" cy="100" r="8" fill="#e94560"/>
</svg>
)SVG";

static const char* kSvgGears = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="300" height="200" viewBox="0 0 300 200">
  <g transform="translate(100,100)">
    <path d="M0,-50 L10,-45 L12,-35 L20,-38 L25,-48 L35,-42 L30,-32 L38,-25
             L45,-30 L50,-20 L42,-15 L38,-5 L45,0 L42,10 L35,5 L30,15
             L38,22 L32,30 L22,25 L15,32 L18,42 L8,45 L5,35 L-5,35 L-8,45
             L-18,42 L-15,32 L-22,25 L-32,30 L-38,22 L-30,15 L-35,5
             L-42,10 L-45,0 L-38,-5 L-42,-15 L-50,-20 L-45,-30 L-38,-25
             L-30,-32 L-35,-42 L-25,-48 L-20,-38 L-12,-35 L-10,-45 Z"
          fill="#4a6fa5" stroke="#6b93d6" stroke-width="1.5"/>
    <circle cx="0" cy="0" r="15" fill="#1a1a2e"/>
  </g>
  <g transform="translate(210,120)">
    <circle cx="0" cy="0" r="30" fill="#e07a5f" stroke="#f4845f" stroke-width="1.5"/>
    <circle cx="0" cy="0" r="10" fill="#1a1a2e"/>
    <line x1="-25" y1="0" x2="25" y2="0" stroke="#1a1a2e" stroke-width="3"/>
    <line x1="0" y1="-25" x2="0" y2="25" stroke="#1a1a2e" stroke-width="3"/>
  </g>
</svg>
)SVG";

static const char* kSvgChart = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="400" height="200" viewBox="0 0 400 200">
  <!-- Background -->
  <rect x="0" y="0" width="400" height="200" rx="10" fill="#1a1a2e"/>
  <!-- Grid lines -->
  <line x1="40" y1="170" x2="380" y2="170" stroke="#333" stroke-width="1"/>
  <line x1="40" y1="130" x2="380" y2="130" stroke="#222" stroke-width="0.5"/>
  <line x1="40" y1="90" x2="380" y2="90" stroke="#222" stroke-width="0.5"/>
  <line x1="40" y1="50" x2="380" y2="50" stroke="#222" stroke-width="0.5"/>
  <!-- Bars -->
  <rect x="60" y="80" width="30" height="90" rx="4" fill="#4cc9f0"/>
  <rect x="110" y="50" width="30" height="120" rx="4" fill="#4361ee"/>
  <rect x="160" y="100" width="30" height="70" rx="4" fill="#3a0ca3"/>
  <rect x="210" y="60" width="30" height="110" rx="4" fill="#7209b7"/>
  <rect x="260" y="40" width="30" height="130" rx="4" fill="#f72585"/>
  <rect x="310" y="90" width="30" height="80" rx="4" fill="#4cc9f0"/>
  <rect x="360" y="70" width="30" height="100" rx="4" fill="#4361ee"/>
  <!-- Line chart overlay -->
  <polyline points="75,75 125,45 175,95 225,55 275,35 325,85 375,65"
            fill="none" stroke="#f8961e" stroke-width="2.5"/>
</svg>
)SVG";

static const char* kSvgIcon = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 24 24">
  <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2z"
        fill="#2196F3"/>
  <path d="M12 6l-1.5 3.5L7 11l3.5 1.5L12 16l1.5-3.5L17 11l-3.5-1.5z"
        fill="white"/>
</svg>
)SVG";

static const char* kSvgFlower = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="200" height="200" viewBox="0 0 200 200">
  <g transform="translate(100,100)">
    <ellipse cx="0" cy="-40" rx="20" ry="40" fill="#ff6b6b" opacity="0.8"/>
    <ellipse cx="38" cy="-12" rx="20" ry="40" fill="#ffa502" opacity="0.8"
             transform="rotate(72)"/>
    <ellipse cx="24" cy="32" rx="20" ry="40" fill="#ffd166" opacity="0.8"
             transform="rotate(144)"/>
    <ellipse cx="-24" cy="32" rx="20" ry="40" fill="#06d6a0" opacity="0.8"
             transform="rotate(216)"/>
    <ellipse cx="-38" cy="-12" rx="20" ry="40" fill="#118ab2" opacity="0.8"
             transform="rotate(288)"/>
    <circle cx="0" cy="0" r="18" fill="#ffd166"/>
    <circle cx="0" cy="0" r="10" fill="#ef476f"/>
  </g>
</svg>
)SVG";

static const char* kSvgBezierArt = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="300" height="200" viewBox="0 0 300 200">
  <path d="M10,100 C40,10 80,10 100,100 S160,190 200,100 S260,10 290,100"
        fill="none" stroke="#00b4d8" stroke-width="3"/>
  <path d="M10,100 Q75,20 150,100 T290,100"
        fill="none" stroke="#e63946" stroke-width="2" opacity="0.7"/>
  <path d="M50,180 C100,120 150,180 200,120 S300,60 250,180"
        fill="#90be6d" fill-opacity="0.4" stroke="#2d6a4f" stroke-width="1.5"/>
</svg>
)SVG";

// --- New feature test cases: Gradients, CSS <style>, Markers ---

static const char* kSvgGradients = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="300" height="200" viewBox="0 0 300 200">
  <defs>
    <linearGradient id="skyGrad" x1="0%" y1="0%" x2="0%" y2="100%">
      <stop offset="0%" stop-color="#0f2027"/>
      <stop offset="50%" stop-color="#203a43"/>
      <stop offset="100%" stop-color="#2c5364"/>
    </linearGradient>
    <radialGradient id="sunGrad" cx="50%" cy="50%" r="50%">
      <stop offset="0%" stop-color="#fceabb"/>
      <stop offset="70%" stop-color="#f8b500"/>
      <stop offset="100%" stop-color="#e65c00"/>
    </radialGradient>
    <linearGradient id="groundGrad" x1="0%" y1="0%" x2="100%" y2="0%">
      <stop offset="0%" stop-color="#2d6a4f"/>
      <stop offset="100%" stop-color="#40916c"/>
    </linearGradient>
  </defs>
  <!-- Sky -->
  <rect x="0" y="0" width="300" height="140" fill="url(#skyGrad)"/>
  <!-- Sun -->
  <circle cx="220" cy="50" r="35" fill="url(#sunGrad)"/>
  <!-- Ground -->
  <rect x="0" y="140" width="300" height="60" fill="url(#groundGrad)"/>
  <!-- Mountains with solid fill for contrast -->
  <polygon points="0,140 60,80 120,140" fill="#1b4332"/>
  <polygon points="80,140 150,60 220,140" fill="#2d6a4f"/>
  <polygon points="180,140 250,90 300,140" fill="#1b4332"/>
</svg>
)SVG";

static const char* kSvgCssStyle = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="280" height="200" viewBox="0 0 280 200">
  <style>
    .bg { fill: #1a1a2e; }
    .card { fill: #16213e; stroke: #0f3460; stroke-width: 2; }
    .title-bar { fill: #e94560; }
    .dot { fill: #ffffff; }
    .content { fill: #0f3460; }
    circle.accent { fill: #533483; }
    rect.highlight { fill: #e94560; opacity: 0.6; }
  </style>
  <!-- Background -->
  <rect class="bg" x="0" y="0" width="280" height="200"/>
  <!-- Card -->
  <rect class="card" x="20" y="20" width="240" height="160" rx="8"/>
  <!-- Title bar -->
  <rect class="title-bar" x="20" y="20" width="240" height="30" rx="8"/>
  <!-- Window dots -->
  <circle class="dot" cx="40" cy="35" r="5"/>
  <circle class="dot" cx="58" cy="35" r="5"/>
  <circle class="dot" cx="76" cy="35" r="5"/>
  <!-- Content area -->
  <rect class="content" x="35" y="65" width="210" height="15" rx="3"/>
  <rect class="content" x="35" y="90" width="170" height="15" rx="3"/>
  <rect class="content" x="35" y="115" width="190" height="15" rx="3"/>
  <!-- Accents -->
  <circle class="accent" cx="220" cy="150" r="18"/>
  <rect class="highlight" x="35" y="145" width="80" height="20" rx="4"/>
</svg>
)SVG";

static const char* kSvgMarkers = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="300" height="200" viewBox="0 0 300 200">
  <defs>
    <marker id="arrowEnd" markerWidth="10" markerHeight="7"
            refX="10" refY="3.5" orient="auto">
      <polygon points="0,0 10,3.5 0,7" fill="#e63946"/>
    </marker>
    <marker id="circleMarker" markerWidth="8" markerHeight="8"
            refX="4" refY="4" orient="auto">
      <circle cx="4" cy="4" r="3" fill="#457b9d"/>
    </marker>
    <marker id="diamondMarker" markerWidth="10" markerHeight="10"
            refX="5" refY="5" orient="auto">
      <polygon points="5,0 10,5 5,10 0,5" fill="#2a9d8f"/>
    </marker>
  </defs>
  <!-- Background -->
  <rect x="0" y="0" width="300" height="200" rx="6" fill="#1d3557"/>
  <!-- Arrow lines -->
  <line x1="30" y1="170" x2="270" y2="170" stroke="#a8dadc" stroke-width="2"
        marker-end="url(#arrowEnd)"/>
  <line x1="30" y1="170" x2="30" y2="20" stroke="#a8dadc" stroke-width="2"
        marker-end="url(#arrowEnd)"/>
  <!-- Data line with circle markers -->
  <polyline points="50,140 100,100 150,120 200,60 250,80"
            fill="none" stroke="#f1faee" stroke-width="2"
            marker-start="url(#circleMarker)"
            marker-mid="url(#circleMarker)"
            marker-end="url(#circleMarker)"/>
  <!-- Separate path with diamond markers -->
  <path d="M60,40 L120,70 L180,30 L240,50"
        fill="none" stroke="#e9c46a" stroke-width="1.5"
        marker-start="url(#diamondMarker)"
        marker-mid="url(#diamondMarker)"
        marker-end="url(#diamondMarker)"/>
</svg>
)SVG";

static const char* kSvgSymbolText = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="360" height="200" viewBox="0 0 360 200">
  <defs>
    <symbol id="tile" viewBox="0 0 72 72">
      <rect x="6" y="6" width="60" height="60" rx="16" fill="#16213e" stroke="#4cc9f0" stroke-width="3"/>
      <circle cx="36" cy="28" r="11" fill="#f72585"/>
      <path d="M20 48 C28 38 44 38 52 48" fill="none" stroke="#f1faee" stroke-width="4" stroke-linecap="round"/>
    </symbol>
  </defs>

  <rect x="0" y="0" width="360" height="200" rx="12" fill="#0f172a"/>
  <use href="#tile" x="22" y="26" width="72" height="72"/>
  <use href="#tile" x="108" y="26" width="72" height="72"/>
  <use href="#tile" x="194" y="26" width="72" height="72"/>

  <text x="22" y="132" font-size="20" fill="#e2e8f0">symbol + use</text>
  <text x="22" y="162" font-size="18" fill="#94a3b8">
    <tspan x="22" y="162">text</tspan>
    <tspan x="86" y="162" fill="#4cc9f0"> + tspan</tspan>
  </text>
</svg>
)SVG";

int main(int argc, char* argv[])
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1400, 800, "VectorGL — SVG Rendering Demo", nullptr, nullptr);
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

    // Load font for labels
    bool hasFont = false;
    const char* fontPaths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf",
    };
    for (auto* fp : fontPaths)
    {
        if (canvas.setFont(fp, 18.0f))
        {
            hasFont = true;
            break;
        }
    }

    // Parse inline SVGs using handle-based SvgCache (NanoVG-style)
    vectorgl::SvgCache svgCache;

    int svgLogo = svgCache.loadFromString(kSvgLogo);
    int svgGears = svgCache.loadFromString(kSvgGears);
    int svgChart = svgCache.loadFromString(kSvgChart);
    int svgIcon = svgCache.loadFromString(kSvgIcon);
    int svgFlower = svgCache.loadFromString(kSvgFlower);
    int svgBezier = svgCache.loadFromString(kSvgBezierArt);
    int svgGradients = svgCache.loadFromString(kSvgGradients);
    int svgCssStyle = svgCache.loadFromString(kSvgCssStyle);
    int svgMarkers = svgCache.loadFromString(kSvgMarkers);
    int svgSymbolText = svgCache.loadFromString(kSvgSymbolText);

    if (svgLogo < 0 || svgGears < 0 || svgChart < 0 || svgIcon < 0 || svgFlower < 0 || svgBezier < 0 ||
      svgGradients < 0 || svgCssStyle < 0 || svgMarkers < 0 || svgSymbolText < 0)
    {
        std::cerr << "Failed to parse one or more inline SVGs\n";
        return 1;
    }

    // Try loading from file if argument provided
    int svgFile = vectorgl::SvgCache::kInvalidHandle;
    bool hasFileSvg = false;
    if (argc >= 2)
    {
        svgFile = svgCache.load(argv[1]);
        hasFileSvg = svgCache.valid(svgFile);
        if (hasFileSvg)
        {
            std::cout << "Loaded SVG: " << argv[1] << " (" << svgCache.width(svgFile) << "x"
                      << svgCache.height(svgFile) << ")\n";
        }
        else
        {
            std::cerr << "Failed to load SVG: " << argv[1] << "\n";
        }
    }

    std::cout << "VectorGL SVG Demo — custom parser, no external dependencies\n";
    std::cout << "Supports: path (M/L/C/Q/S/T/A/Z), rect, circle, ellipse,\n";
    std::cout << "          line, polyline, polygon, groups, transforms, styles,\n";
    std::cout << "          linear/radial gradients, CSS <style> blocks, markers,\n";
    std::cout << "          symbol/use reuse, text, and tspan content\n";

    while (!glfwWindowShouldClose(window))
    {
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        float t = static_cast<float>(glfwGetTime());

        glClearColor(0.04f, 0.04f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        canvas.beginFrame(fbW, fbH);

        // Title
        if (hasFont)
        {
            canvas.setFillColor(vectorgl::Color{0.8f, 0.85f, 0.95f, 1.0f});
            canvas.fillText("VectorGL SVG Rendering (Custom Parser)", 20, 22);
        }

        // Section 1: Logo
        if (hasFont)
        {
            canvas.setFillColor(vectorgl::Color{0.6f, 0.7f, 0.9f, 0.8f});
            canvas.fillText("Logo (paths + circles)", 30, 62);
        }
        svgCache.render(canvas, svgLogo, 50, 80, 1.0f);

        // Section 2: Gears
        if (hasFont)
        {
            canvas.setFillColor(vectorgl::Color{0.6f, 0.7f, 0.9f, 0.8f});
            canvas.fillText("Gears (groups + transforms)", 300, 62);
        }
        svgCache.render(canvas, svgGears, 300, 80, 0.9f);

        // Section 3: Chart (rects + polyline)
        if (hasFont)
        {
            canvas.setFillColor(vectorgl::Color{0.6f, 0.7f, 0.9f, 0.8f});
            canvas.fillText("Chart (rect + polyline)", 650, 62);
        }
        svgCache.render(canvas, svgChart, 650, 80, 1.0f);

        // Section 4: Icon at multiple scales
        if (hasFont)
        {
            canvas.setFillColor(vectorgl::Color{0.6f, 0.7f, 0.9f, 0.8f});
            canvas.fillText("Icon at different scales", 30, 320);
        }
        svgCache.render(canvas, svgIcon, 30, 340, 1.0f);  // 64px
        svgCache.render(canvas, svgIcon, 110, 340, 1.5f); // 96px
        svgCache.render(canvas, svgIcon, 220, 340, 2.0f); // 128px
        svgCache.render(canvas, svgIcon, 370, 340, 3.0f); // 192px

        // Section 5: Flower
        if (hasFont)
        {
            canvas.setFillColor(vectorgl::Color{0.6f, 0.7f, 0.9f, 0.8f});
            canvas.fillText("Flower (ellipses + opacity)", 600, 320);
        }
        svgCache.render(canvas, svgFlower, 620, 340, 1.2f);

        // Section 6: Bezier art
        if (hasFont)
        {
            canvas.setFillColor(vectorgl::Color{0.6f, 0.7f, 0.9f, 0.8f});
            canvas.fillText("Bezier Curves (C/S/Q/T)", 30, 560);
        }
        svgCache.render(canvas, svgBezier, 30, 580, 1.5f);

        // Section 7: Gradients (linear + radial)
        if (hasFont)
        {
            canvas.setFillColor(vectorgl::Color{0.6f, 0.7f, 0.9f, 0.8f});
            canvas.fillText("Gradients (linear + radial)", 900, 62);
        }
        svgCache.render(canvas, svgGradients, 900, 80, 1.0f);

        // Section 8: CSS <style> blocks
        if (hasFont)
        {
            canvas.setFillColor(vectorgl::Color{0.6f, 0.7f, 0.9f, 0.8f});
            canvas.fillText("CSS <style> (class selectors)", 550, 560);
        }
        svgCache.render(canvas, svgCssStyle, 550, 580, 1.0f);

        // Section 9: Markers
        if (hasFont)
        {
            canvas.setFillColor(vectorgl::Color{0.6f, 0.7f, 0.9f, 0.8f});
            canvas.fillText("Markers (arrows + data points)", 900, 560);
        }
        svgCache.render(canvas, svgMarkers, 900, 580, 1.0f);

        // Section 10: Symbol reuse + text/tspan
        if (hasFont)
        {
            canvas.setFillColor(vectorgl::Color{0.6f, 0.7f, 0.9f, 0.8f});
          canvas.fillText("Symbol reuse + text/tspan", 900, 320);
        }
        svgCache.render(canvas, svgSymbolText, 900, 340, 1.0f);

        // Section 11: File SVG if loaded
        if (hasFileSvg)
        {
            if (hasFont)
            {
                canvas.setFillColor(vectorgl::Color{0.6f, 0.7f, 0.9f, 0.8f});
                canvas.fillText("From file: " + std::string(argv[1]), 600, 560);
            }
            // Scale to fit ~300px wide
            float fileScale = 300.0f / std::max(svgCache.width(svgFile), 1.0f);
            svgCache.render(canvas, svgFile, 600, 580, fileScale);
        }

        // FPS
        if (hasFont)
        {
            static int fc = 0;
            static float timer = 0, fps = 0;
            static float lastT = t;
            float dt = t - lastT;
            lastT = t;
            fc++;
            timer += dt;
            if (timer >= 0.5f)
            {
                fps = fc / timer;
                fc = 0;
                timer = 0;
            }
            canvas.setFillColor(vectorgl::Color{0.7f, 0.7f, 0.7f, 0.7f});
            canvas.fillText("FPS: " + std::to_string(static_cast<int>(fps)), fbW - 100.0f, 22.0f);
        }

        canvas.endFrame();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    canvas.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
