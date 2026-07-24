// ============================================================================
// VectorGL — Comprehensive Feature Demo
// ============================================================================
// A single application showcasing ALL features of vectorgl, organized into
// labeled sections (like Dear ImGui's demo window).
//
// Sections:
//   1. Basic Shapes (Rect, Circle, Ellipse, RoundedRect)
//   2. Stroke & Fill Styles
//   3. Path Drawing (Lines, Bezier, Quadratic, Arcs, Star)
//   4. Transforms (Translate, Rotate, Scale, Nested)
//   5. Colors & Blending (Named colors, Hex, Oklab lerp)
//   6. Text Rendering (SDF fonts, sizes, colors)
//   7. Scene Graph (Retained-mode nodes)
//   8. Animations (Tween, Spring, Keyframe)
//   9. Effects (Shadow, Blur, Glow)
//  10. Stress Test (Instanced SDF batch)
// ============================================================================

#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <vectorgl/animator.hpp>
#include <vectorgl/canvas.hpp>
#include <vectorgl/scene.hpp>

static constexpr float PI = 3.14159265f;
static constexpr int WIN_W = 1600;
static constexpr int WIN_H = 950;

// Section layout helpers
struct Section
{
    const char* title;
    float x, y, w, h;
};

static const std::array<Section, 10> kSections = {{
    {"1. Basic Shapes", 20, 20, 370, 260},
    {"2. Stroke & Fill", 410, 20, 370, 260},
    {"3. Path Drawing", 800, 20, 370, 260},
    {"4. Transforms", 1190, 20, 390, 260},
    {"5. Colors", 20, 300, 370, 260},
    {"6. Text Rendering", 410, 300, 370, 260},
    {"7. Scene Graph", 800, 300, 370, 260},
    {"8. Animations", 1190, 300, 390, 260},
    {"9. Effects", 20, 580, 760, 350},
    {"10. Stress Test", 800, 580, 780, 350},
}};

// Draw a section panel background + title
static void drawSectionPanel(vectorgl::Canvas& c, const Section& s, bool hasFont)
{
    // Panel background
    c.setFillColor(vectorgl::Color{0.08f, 0.09f, 0.12f, 0.92f});
    c.fillRoundedRect(s.x, s.y, s.w, s.h, 8.0f);

    // Panel border
    c.setStrokeColor(vectorgl::Color{0.25f, 0.3f, 0.4f, 0.6f});
    c.setLineWidth(1.0f);
    c.strokeRoundedRect(s.x, s.y, s.w, s.h, 8.0f);

    // Title
    if (hasFont)
    {
        c.setFillColor(vectorgl::Color{0.7f, 0.8f, 1.0f, 1.0f});
        c.fillText(s.title, s.x + 12, s.y + 22);
    }
}

// ============================================================================
// Section renderers
// ============================================================================

static void drawBasicShapes(vectorgl::Canvas& c, float t)
{
    float x = kSections[0].x + 20;
    float y = kSections[0].y + 45;

    // Filled rect
    c.setFillColor(vectorgl::Color{0.2f, 0.5f, 0.9f, 1.0f});
    c.fillRect(x, y, 60, 40);

    // Stroked rect
    c.setStrokeColor(vectorgl::Color{0.9f, 0.4f, 0.2f, 1.0f});
    c.setLineWidth(2.0f);
    c.strokeRect(x + 80, y, 60, 40);

    // Filled circle
    c.setFillColor(vectorgl::Color{0.2f, 0.9f, 0.4f, 1.0f});
    c.fillCircle(x + 200, y + 20, 20);

    // Stroked circle
    c.setStrokeColor(vectorgl::Color{1.0f, 0.8f, 0.2f, 1.0f});
    c.setLineWidth(2.5f);
    c.strokeCircle(x + 260, y + 20, 20);

    // Ellipses
    c.setFillColor(vectorgl::Color{0.8f, 0.3f, 0.9f, 0.8f});
    c.fillEllipse(x + 50, y + 90, 40, 20);

    c.setStrokeColor(vectorgl::Color{0.3f, 0.9f, 0.9f, 1.0f});
    c.setLineWidth(2.0f);
    c.strokeEllipse(x + 170, y + 90, 30, 18);

    // Rounded rects with varying radii
    float pulse = 0.9f + 0.1f * std::sin(t * 2);
    c.setFillColor(vectorgl::Color{0.9f, 0.6f, 0.2f, pulse});
    c.fillRoundedRect(x + 240, y + 70, 80, 45, 12);

    // Another rounded rect, stroked
    c.setStrokeColor(vectorgl::Color{0.4f, 0.8f, 1.0f, 1.0f});
    c.setLineWidth(2.0f);
    c.strokeRoundedRect(x, y + 140, 100, 50, 16);

    // Small animated circle
    float cx = x + 220 + 40 * std::cos(t);
    float cy = y + 170 + 20 * std::sin(t * 1.5f);
    c.setFillColor(vectorgl::Color{1.0f, 1.0f, 1.0f, 0.9f});
    c.fillCircle(cx, cy, 8);
}

static void drawStrokeAndFill(vectorgl::Canvas& c, float t)
{
    float x = kSections[1].x + 20;
    float y = kSections[1].y + 45;

    // Different line widths
    float widths[] = {1.0f, 2.0f, 3.0f, 5.0f, 8.0f};
    for (int i = 0; i < 5; ++i)
    {
        float hue = static_cast<float>(i) / 5.0f;
        c.setStrokeColor(vectorgl::Color{0.5f + 0.5f * std::cos(hue * 6.28f),
                                         0.5f + 0.5f * std::cos(hue * 6.28f + 2.09f),
                                         0.5f + 0.5f * std::cos(hue * 6.28f + 4.18f), 1.0f});
        c.setLineWidth(widths[i]);
        c.beginPath();
        c.moveTo(x, y + i * 22.0f);
        c.lineTo(x + 150 + 30 * std::sin(t + i), y + i * 22.0f);
        c.stroke();
    }

    // Filled + stroked combo
    c.setFillColor(vectorgl::Color{0.2f, 0.3f, 0.7f, 0.8f});
    c.fillRoundedRect(x, y + 130, 140, 60, 10);
    c.setStrokeColor(vectorgl::Color{1.0f, 1.0f, 1.0f, 0.9f});
    c.setLineWidth(2.0f);
    c.strokeRoundedRect(x, y + 130, 140, 60, 10);

    // Alpha blending demo
    for (int i = 0; i < 5; ++i)
    {
        float alpha = 0.2f + 0.2f * i;
        c.setFillColor(vectorgl::Color{1.0f, 0.4f, 0.2f, alpha});
        c.fillCircle(x + 200 + i * 25.0f, y + 160, 20);
    }
}

static void drawPaths(vectorgl::Canvas& c, float t)
{
    float x = kSections[2].x + 20;
    float y = kSections[2].y + 45;

    // Bezier curve
    c.setStrokeColor(vectorgl::Color{0.3f, 0.9f, 0.6f, 1.0f});
    c.setLineWidth(2.5f);
    c.beginPath();
    c.moveTo(x, y + 40);
    float cp1y = y + 40 - 60 * std::sin(t);
    float cp2y = y + 40 + 60 * std::sin(t);
    c.bezierCurveTo(x + 50, cp1y, x + 100, cp2y, x + 150, y + 40);
    c.stroke();

    // Quadratic curve
    c.setStrokeColor(vectorgl::Color{0.9f, 0.5f, 0.9f, 1.0f});
    c.setLineWidth(2.0f);
    c.beginPath();
    c.moveTo(x + 180, y + 40);
    c.quadraticCurveTo(x + 250, y + 40 - 50 * std::cos(t * 1.3f), x + 320, y + 40);
    c.stroke();

    // Star
    c.save();
    c.translate(x + 80, y + 140);
    c.rotate(t * 0.5f);
    int pts = 5;
    float outerR = 35, innerR = 15;
    c.beginPath();
    for (int i = 0; i <= pts * 2; ++i)
    {
        float angle = PI * i / pts - PI / 2;
        float r = (i % 2 == 0) ? outerR : innerR;
        float px = r * std::cos(angle);
        float py = r * std::sin(angle);
        if (i == 0)
            c.moveTo(px, py);
        else
            c.lineTo(px, py);
    }
    c.closePath();
    c.setFillColor(vectorgl::Color{1.0f, 0.8f, 0.1f, 0.9f});
    c.fill();
    c.setStrokeColor(vectorgl::Color{1.0f, 1.0f, 1.0f, 0.7f});
    c.setLineWidth(1.5f);
    c.stroke();
    c.restore();

    // Arc
    c.setStrokeColor(vectorgl::Color{0.2f, 0.7f, 1.0f, 1.0f});
    c.setLineWidth(3.0f);
    c.beginPath();
    c.arc(x + 250, y + 150, 35, 0, PI + std::sin(t) * 0.5f);
    c.stroke();
}

static void drawTransforms(vectorgl::Canvas& c, float t)
{
    float x = kSections[3].x + 20;
    float y = kSections[3].y + 45;

    // Translated rects
    for (int i = 0; i < 4; ++i)
    {
        c.save();
        float off = 15.0f * std::sin(t + i * 0.5f);
        c.translate(x + 20 + i * 85, y + 30 + off);
        float hue = static_cast<float>(i) / 4.0f;
        c.setFillColor(vectorgl::Color{0.5f + 0.5f * std::cos(hue * 6.28f), 0.5f + 0.5f * std::cos(hue * 6.28f + 2.09f),
                                       0.5f + 0.5f * std::cos(hue * 6.28f + 4.18f), 0.85f});
        c.fillRoundedRect(-25, -15, 50, 30, 6);
        c.restore();
    }

    // Rotating squares
    for (int i = 0; i < 3; ++i)
    {
        c.save();
        c.translate(x + 60 + i * 120, y + 120);
        c.rotate(t * (0.5f + i * 0.3f));
        float size = 25.0f - i * 3;
        c.setFillColor(vectorgl::Color{0.3f + i * 0.3f, 0.8f - i * 0.2f, 0.5f, 0.8f});
        c.fillRect(-size, -size, size * 2, size * 2);
        c.restore();
    }

    // Scaled circles
    for (int i = 0; i < 5; ++i)
    {
        c.save();
        c.translate(x + 30 + i * 75, y + 190);
        float s = 0.5f + 0.5f * std::sin(t + i);
        c.scale(s, s);
        c.setFillColor(vectorgl::Color{0.9f, 0.6f, 0.1f, 0.9f});
        c.fillCircle(0, 0, 15);
        c.restore();
    }
}

static void drawColors(vectorgl::Canvas& c, float t)
{
    float x = kSections[4].x + 15;
    float y = kSections[4].y + 45;

    // Named colors grid
    const vectorgl::Color named[] = {
        vectorgl::Color::Red,  vectorgl::Color::Green,    vectorgl::Color::Blue,      vectorgl::Color::Yellow,
        vectorgl::Color::Cyan, vectorgl::Color::Magenta,  vectorgl::Color::Orange,    vectorgl::Color::White,
        vectorgl::Color::Gray, vectorgl::Color::DarkGray, vectorgl::Color::LightGray, vectorgl::Color::Black,
    };
    for (int i = 0; i < 12; ++i)
    {
        int col = i % 6;
        int row = i / 6;
        c.setFillColor(named[i]);
        c.fillRoundedRect(x + col * 56.0f, y + row * 35.0f, 48, 28, 4);
    }

    // Hex color demo
    c.setFillColor(vectorgl::Color::hex(0xFF6B35));
    c.fillRoundedRect(x, y + 85, 60, 25, 4);
    c.setFillColor(vectorgl::Color::hex(0x004E89));
    c.fillRoundedRect(x + 70, y + 85, 60, 25, 4);
    c.setFillColor(vectorgl::Color::hex(0x1A936F));
    c.fillRoundedRect(x + 140, y + 85, 60, 25, 4);
    c.setFillColor(vectorgl::Color::hex(0xC490D1));
    c.fillRoundedRect(x + 210, y + 85, 60, 25, 4);

    // Oklab lerp color ramp
    vectorgl::Color a = vectorgl::Color::hex(0x0077B6);
    vectorgl::Color b = vectorgl::Color::hex(0xF72585);
    int steps = 16;
    for (int i = 0; i < steps; ++i)
    {
        float frac = static_cast<float>(i) / (steps - 1);
        // Animate the ramp
        float animFrac = std::fmod(frac + t * 0.1f, 1.0f);
        vectorgl::Color col = vectorgl::Color::lerpOklab(a, b, animFrac);
        c.setFillColor(col);
        c.fillRect(x + i * 21.0f, y + 125, 19, 22);
    }

    // Linear vs Oklab comparison
    for (int i = 0; i < steps; ++i)
    {
        float frac = static_cast<float>(i) / (steps - 1);
        vectorgl::Color col = vectorgl::Color::lerp(a, b, frac);
        c.setFillColor(col);
        c.fillRect(x + i * 21.0f, y + 155, 19, 22);
    }
}

static void drawText(vectorgl::Canvas& c, float t, bool hasFont)
{
    float x = kSections[5].x + 15;
    float y = kSections[5].y + 50;

    if (!hasFont)
    {
        c.setFillColor(vectorgl::Color{0.6f, 0.6f, 0.6f, 1.0f});
        c.fillRoundedRect(x, y, 340, 80, 8);
        return;
    }

    // Different colored text
    c.setFillColor(vectorgl::Color::White);
    c.fillText("SDF Text Rendering", x, y);

    c.setFillColor(vectorgl::Color{0.3f, 0.9f, 0.5f, 1.0f});
    c.fillText("Green Text Sample", x, y + 35);

    c.setFillColor(vectorgl::Color{1.0f, 0.5f, 0.2f, 1.0f});
    c.fillText("Orange Highlighted", x, y + 70);

    c.setFillColor(vectorgl::Color{0.5f, 0.7f, 1.0f, 1.0f});
    c.fillText("Smooth at any size!", x, y + 105);

    // Animated color text
    float hue = std::fmod(t * 0.3f, 1.0f);
    c.setFillColor(vectorgl::Color{0.5f + 0.5f * std::cos(hue * 6.28f), 0.5f + 0.5f * std::cos(hue * 6.28f + 2.09f),
                                   0.5f + 0.5f * std::cos(hue * 6.28f + 4.18f), 1.0f});
    c.fillText("Rainbow Animated!", x, y + 140);

    // Alpha text
    for (int i = 0; i < 5; ++i)
    {
        c.setFillColor(vectorgl::Color{1.0f, 1.0f, 1.0f, 0.2f + i * 0.2f});
        c.fillText("A", x + 220 + i * 20, y + 140);
    }
}

static void drawEffects(vectorgl::Canvas& c, float t)
{
    float x = kSections[8].x + 20;
    float y = kSections[8].y + 50;

    // Shadow demonstration
    float shadowOff = 4 + 2 * std::sin(t);

    // Shadow (manual: dark shape behind, then bright shape)
    c.setFillColor(vectorgl::Color{0, 0, 0, 0.4f});
    c.fillRoundedRect(x + shadowOff + 3, y + shadowOff + 3, 120, 70, 10);
    c.setFillColor(vectorgl::Color{0.2f, 0.5f, 0.95f, 1.0f});
    c.fillRoundedRect(x, y, 120, 70, 10);

    // Glow (bright circle with expanding ring)
    float glowR = 30 + 5 * std::sin(t * 2);
    for (int i = 4; i >= 0; --i)
    {
        float alpha = 0.1f * (5 - i);
        float extra = i * 6.0f;
        c.setFillColor(vectorgl::Color{0.2f, 0.9f, 1.0f, alpha});
        c.fillCircle(x + 220, y + 40, glowR + extra);
    }
    c.setFillColor(vectorgl::Color{0.4f, 1.0f, 1.0f, 1.0f});
    c.fillCircle(x + 220, y + 40, glowR - 5);

    // Soft shadow under cards
    for (int i = 0; i < 3; ++i)
    {
        float cx = x + 370 + i * 110.0f;
        float cy = y + 35;
        // shadow
        c.setFillColor(vectorgl::Color{0, 0, 0, 0.3f});
        c.fillRoundedRect(cx + 3, cy + 5, 90, 55, 8);
        // card
        float bright = 0.15f + i * 0.1f;
        c.setFillColor(vectorgl::Color{bright, bright + 0.05f, bright + 0.15f, 1.0f});
        c.fillRoundedRect(cx, cy, 90, 55, 8);
    }

    // Blur simulation: increasingly transparent concentric shapes
    float blurCx = x + 100;
    float blurCy = y + 180;
    for (int i = 8; i >= 0; --i)
    {
        float alpha = 0.08f * (9 - i);
        float spread = i * 4.0f;
        c.setFillColor(vectorgl::Color{0.9f, 0.3f, 0.6f, alpha});
        c.fillRoundedRect(blurCx - 50 - spread, blurCy - 30 - spread, 100 + spread * 2, 60 + spread * 2,
                          12 + spread * 0.5f);
    }
    c.setFillColor(vectorgl::Color{1.0f, 0.4f, 0.7f, 1.0f});
    c.fillRoundedRect(blurCx - 50, blurCy - 30, 100, 60, 12);

    // Layered glow ring
    float ringCx = x + 320;
    float ringCy = y + 200;
    for (int i = 6; i >= 0; --i)
    {
        float alpha = 0.12f * (7 - i);
        float radius = 35 + i * 7 + 3 * std::sin(t * 1.5f + i);
        c.setFillColor(vectorgl::Color{1.0f, 0.8f, 0.2f, alpha});
        c.fillCircle(ringCx, ringCy, radius);
    }
    c.setFillColor(vectorgl::Color{0.08f, 0.09f, 0.12f, 1.0f});
    c.fillCircle(ringCx, ringCy, 30);
    c.setStrokeColor(vectorgl::Color{1.0f, 0.85f, 0.3f, 1.0f});
    c.setLineWidth(2.0f);
    c.strokeCircle(ringCx, ringCy, 30);

    // Neon line effect
    float neonY = y + 270;
    for (int i = 4; i >= 0; --i)
    {
        float alpha = 0.15f * (5 - i);
        float w = 1.0f + i * 2.5f;
        c.setStrokeColor(vectorgl::Color{0.2f, 1.0f, 0.5f, alpha});
        c.setLineWidth(w);
        c.beginPath();
        c.moveTo(x + 450, neonY);
        for (int j = 1; j <= 40; ++j)
        {
            float px = x + 450 + j * 7.0f;
            float py = neonY + 20 * std::sin(j * 0.3f + t * 2);
            c.lineTo(px, py);
        }
        c.stroke();
    }
}

static void drawStressTest(vectorgl::Canvas& c, float t)
{
    float x = kSections[9].x + 20;
    float y = kSections[9].y + 50;
    float w = kSections[9].w - 40;
    float h = kSections[9].h - 70;

    // 500 instanced circles — all batched in a single draw call!
    int count = 500;
    for (int i = 0; i < count; ++i)
    {
        float fi = static_cast<float>(i);
        float angle = fi * 0.0125664f + t * 0.3f; // 2*PI/500
        float radius = 50 + (h * 0.4f) * (fi / count);
        float cx = x + w * 0.5f + radius * std::cos(angle + fi * 0.02f);
        float cy = y + h * 0.5f + radius * std::sin(angle + fi * 0.03f) * 0.6f;
        float r = 2.0f + 4.0f * std::sin(fi * 0.1f + t);
        float hue = std::fmod(fi / count + t * 0.05f, 1.0f);

        c.setFillColor(vectorgl::Color{0.5f + 0.5f * std::cos(hue * 6.28f), 0.5f + 0.5f * std::cos(hue * 6.28f + 2.09f),
                                       0.5f + 0.5f * std::cos(hue * 6.28f + 4.18f),
                                       0.6f + 0.4f * std::sin(fi * 0.05f + t)});
        c.fillCircle(cx, cy, r);
    }

    // Also a grid of rects (200 more)
    int cols = 20, rows = 10;
    float cellW = (w * 0.4f) / cols;
    float cellH = (h * 0.5f) / rows;
    float gridX = x + w * 0.55f;
    float gridY = y + h * 0.55f;
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            float pulse = 0.3f + 0.7f * std::abs(std::sin(t + row * 0.3f + col * 0.2f));
            float hue = std::fmod((row * cols + col) / 200.0f + t * 0.02f, 1.0f);
            c.setFillColor(vectorgl::Color{0.5f + 0.5f * std::cos(hue * 6.28f),
                                           0.5f + 0.5f * std::cos(hue * 6.28f + 2.09f),
                                           0.5f + 0.5f * std::cos(hue * 6.28f + 4.18f), pulse});
            c.fillRect(gridX + col * cellW, gridY + row * cellH, cellW - 1, cellH - 1);
        }
    }
}

// ============================================================================
// Scene Graph demo objects
// ============================================================================
struct SceneDemo
{
    std::unique_ptr<vectorgl::Scene> scene;
    std::shared_ptr<vectorgl::Node> card;
    std::shared_ptr<vectorgl::Node> orbitCircle;
    std::shared_ptr<vectorgl::Node> group;
    bool initialized = false;
};

static void initSceneDemo(SceneDemo& sd, vectorgl::Canvas& canvas)
{
    sd.scene = std::make_unique<vectorgl::Scene>(canvas.renderer());

    // A card
    sd.card = sd.scene->roundedRect(kSections[6].x + 100, kSections[6].y + 150, 120, 70, 10);
    sd.card->style().fillColor = vectorgl::Color{0.15f, 0.35f, 0.8f, 0.9f};
    sd.card->style().strokeColor = vectorgl::Color{0.4f, 0.6f, 1.0f, 0.8f};
    sd.card->style().strokeWidth = 2.0f;

    // Orbiting circle
    sd.orbitCircle = sd.scene->circle(kSections[6].x + 280, kSections[6].y + 150, 15);
    sd.orbitCircle->style().fillColor = vectorgl::Color{1.0f, 0.5f, 0.2f, 1.0f};

    // Group of small shapes
    sd.group = sd.scene->group();
    for (int i = 0; i < 8; ++i)
    {
        float angle = 2.0f * PI * i / 8.0f;
        float gx = kSections[6].x + 185 + 50 * std::cos(angle);
        float gy = kSections[6].y + 200 + 40 * std::sin(angle);
        auto dot = sd.scene->circle(gx, gy, 5);
        float frac = static_cast<float>(i) / 8.0f;
        dot->style().fillColor = vectorgl::Color{frac, 1.0f - frac, 0.5f, 0.9f};
        sd.group->addChild(dot);
    }

    // Spring animation on card
    {
        vectorgl::AnimTarget to;
        to.y = kSections[6].y + 160.0f;
        to.mask = vectorgl::AnimTarget::PosY;
        auto anim = std::make_unique<vectorgl::SpringAnimation>(sd.card->id(), to, 100.0f, 10.0f);
        sd.scene->animator().add(std::move(anim));
    }

    // Tween on orbit circle opacity
    {
        vectorgl::AnimTarget from, to;
        from.opacity = 0.3f;
        from.mask = vectorgl::AnimTarget::Opacity;
        to.opacity = 1.0f;
        to.mask = vectorgl::AnimTarget::Opacity;
        auto anim = std::make_unique<vectorgl::TweenAnimation>(
            sd.orbitCircle->id(), from, to, 1.5f, vectorgl::Ease::InOutCubic, vectorgl::LoopMode::PingPong);
        sd.scene->animator().add(std::move(anim));
    }

    sd.initialized = true;
}

static void drawAnimations(vectorgl::Canvas& c, float t)
{
    float x = kSections[7].x + 20;
    float y = kSections[7].y + 50;

    // Easing showcase: show multiple easing curves
    const char* names[] = {"Linear", "InOutCubic", "OutBounce", "InElastic", "OutBack"};
    vectorgl::Ease easings[] = {vectorgl::Ease::Linear, vectorgl::Ease::InOutCubic, vectorgl::Ease::OutBounce,
                                vectorgl::Ease::InElastic, vectorgl::Ease::OutBack};

    float loopT = std::fmod(t, 3.0f) / 3.0f; // 0-1 over 3 seconds

    for (int i = 0; i < 5; ++i)
    {
        float eased = vectorgl::evalEasing(easings[i], loopT);
        float posX = x + 10 + eased * 300.0f;
        float posY = y + i * 38.0f;

        // Track line
        c.setStrokeColor(vectorgl::Color{0.3f, 0.3f, 0.4f, 0.5f});
        c.setLineWidth(1.0f);
        c.beginPath();
        c.moveTo(x + 10, posY + 8);
        c.lineTo(x + 310, posY + 8);
        c.stroke();

        // Moving dot
        float hue = static_cast<float>(i) / 5.0f;
        c.setFillColor(vectorgl::Color{0.5f + 0.5f * std::cos(hue * 6.28f), 0.5f + 0.5f * std::cos(hue * 6.28f + 2.09f),
                                       0.5f + 0.5f * std::cos(hue * 6.28f + 4.18f), 1.0f});
        c.fillCircle(posX, posY + 8, 7);
    }
}

// ============================================================================
// Main
// ============================================================================
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

    GLFWwindow* window = glfwCreateWindow(WIN_W, WIN_H, "VectorGL — Complete Feature Demo", nullptr, nullptr);
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

    // Try loading a system font
    bool hasFont = false;
    const char* fontPaths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/consola.ttf",
    };
    for (auto* fp : fontPaths)
    {
        if (canvas.setFont(fp, 18.0f))
        {
            hasFont = true;
            break;
        }
    }

    // Scene graph demo
    SceneDemo sceneDemo;
    initSceneDemo(sceneDemo, canvas);

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;
        float t = static_cast<float>(now);

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);

        glClearColor(0.03f, 0.03f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        canvas.beginFrame(fbW, fbH);

        // Draw section panels
        for (auto& s : kSections)
        {
            drawSectionPanel(canvas, s, hasFont);
        }

        // Render each section
        drawBasicShapes(canvas, t);
        drawStrokeAndFill(canvas, t);
        drawPaths(canvas, t);
        drawTransforms(canvas, t);
        drawColors(canvas, t);
        drawText(canvas, t, hasFont);
        drawAnimations(canvas, t);
        drawEffects(canvas, t);
        drawStressTest(canvas, t);

        // Scene graph update + render
        if (sceneDemo.initialized)
        {
            sceneDemo.scene->update(dt);
            sceneDemo.scene->render();
        }

        // FPS counter
        if (hasFont)
        {
            static int frameCount = 0;
            static float fpsTimer = 0;
            static float displayFps = 0;
            frameCount++;
            fpsTimer += dt;
            if (fpsTimer >= 0.5f)
            {
                displayFps = frameCount / fpsTimer;
                frameCount = 0;
                fpsTimer = 0;
            }
            canvas.setFillColor(vectorgl::Color{0.8f, 0.8f, 0.8f, 0.8f});
            canvas.fillText("FPS: " + std::to_string(static_cast<int>(displayFps)), fbW - 100.0f, 15.0f);
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
