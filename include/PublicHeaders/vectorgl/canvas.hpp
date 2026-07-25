#pragma once
#include <memory>
#include <string>
#include <vector>

#include "vectorgl/color.hpp"
#include "vectorgl/font.hpp"
#include "vectorgl/image.hpp"
#include "vectorgl/path.hpp"
#include "vectorgl/renderer.hpp"

namespace vectorgl
{

/*! @brief The main drawing surface for 2D vector graphics.
 *
 *  Canvas provides an HTML5 Canvas-style API for rendering shapes, paths, text,
 *  and images.  Simple primitives (rect, circle, ellipse, rounded rect) are
 *  GPU-accelerated via signed distance fields (SDF).  Complex paths go through
 *  triangulation.
 *
 *  Typical usage:
 *  @code
 *  vectorgl::Canvas canvas;
 *  canvas.init();
 *
 *  // Each frame:
 *  canvas.beginFrame(fbWidth, fbHeight);
 *  canvas.setFillColor(vectorgl::Color::Red);
 *  canvas.fillRect(10, 10, 200, 100);
 *  canvas.endFrame();
 *
 *  canvas.destroy();
 *  @endcode
 */
class Canvas
{
public:
    Canvas();
    ~Canvas();

    /*! @brief Initializes GPU resources (shaders, buffers).
     *  Must be called once after a valid OpenGL context is current.
     */
    void init();

    /*! @brief Releases all GPU resources.
     *  Call before destroying the OpenGL context.
     */
    void destroy();

    /*! @brief Begins a new rendering frame.
     *
     *  Sets up the viewport and projection for the given framebuffer size.
     *  All drawing calls must occur between beginFrame() and endFrame().
     *
     *  @param[in] fbWidth   Framebuffer width in pixels.
     *  @param[in] fbHeight  Framebuffer height in pixels.
     */
    void beginFrame(int fbWidth, int fbHeight);

    /*! @brief Ends the current frame and flushes all pending draw commands. */
    void endFrame();

    /*! @brief Pushes the current state (transform, fill, stroke, line width) onto the stack.
     *  @sa restore
     */
    void save();

    /*! @brief Pops and restores the most recently saved state.
     *  @sa save
     */
    void restore();

    /*! @brief Intersects drawing with an axis-aligned rectangular clip.
     *
     *  The rectangle is transformed by the current transform and intersected
     *  with the existing clip. Clips participate in save()/restore().
     *  Rotated rectangles use their transformed axis-aligned bounding box.
     *
     *  @param[in] x       Rectangle left edge.
     *  @param[in] y       Rectangle top edge.
     *  @param[in] width   Non-negative rectangle width.
     *  @param[in] height  Non-negative rectangle height.
     *  @throws std::invalid_argument if width or height is negative.
     */
    void clipRect(float x, float y, float width, float height);

    /*! @brief Removes the active clip without changing other Canvas state. */
    void resetClip();

    /*! @brief Translates the current transform.
     *  @param[in] x  Horizontal offset in pixels.
     *  @param[in] y  Vertical offset in pixels.
     */
    void translate(float x, float y);

    /*! @brief Rotates the current transform.
     *  @param[in] angle  Rotation angle in radians (clockwise).
     */
    void rotate(float angle);

    /*! @brief Scales the current transform.
     *  @param[in] x  Horizontal scale factor.
     *  @param[in] y  Vertical scale factor.
     */
    void scale(float x, float y);

    /*! @brief Sets the fill color for subsequent fill operations.
     *  @param[in] c  The color to use.
     */
    void setFillColor(Color c);

    /*! @brief Sets the stroke color for subsequent stroke operations.
     *  @param[in] c  The color to use.
     */
    void setStrokeColor(Color c);

    /*! @brief Sets the stroke width for subsequent stroke operations.
     *  @param[in] w  Line width in pixels.
     */
    void setLineWidth(float w);

    /*! @brief Loads a TrueType font and sets it as the active font for text rendering.
     *
     *  @param[in] fontPath  Filesystem path to a `.ttf` file.
     *  @param[in] size      Font size in pixels.
     *  @return `true` if the font was loaded successfully.
     */
    bool setFont(const std::string& fontPath, float size);

    // --- SDF-accelerated shape primitives ---

    /*! @brief Fills an axis-aligned rectangle (GPU SDF-accelerated).
     *  @param[in] x  Left edge.  @param[in] y  Top edge.
     *  @param[in] w  Width.      @param[in] h  Height.
     */
    void fillRect(float x, float y, float w, float h);

    /*! @brief Strokes an axis-aligned rectangle (GPU SDF-accelerated).
     *  @param[in] x  Left edge.  @param[in] y  Top edge.
     *  @param[in] w  Width.      @param[in] h  Height.
     */
    void strokeRect(float x, float y, float w, float h);

    /*! @brief Fills a circle (GPU SDF-accelerated).
     *  @param[in] cx  Center X.  @param[in] cy  Center Y.  @param[in] r  Radius.
     */
    void fillCircle(float cx, float cy, float r);

    /*! @brief Strokes a circle (GPU SDF-accelerated).
     *  @param[in] cx  Center X.  @param[in] cy  Center Y.  @param[in] r  Radius.
     */
    void strokeCircle(float cx, float cy, float r);

    /*! @brief Fills an ellipse (GPU SDF-accelerated).
     *  @param[in] cx  Center X.  @param[in] cy  Center Y.
     *  @param[in] rx  Horizontal radius.  @param[in] ry  Vertical radius.
     */
    void fillEllipse(float cx, float cy, float rx, float ry);

    /*! @brief Strokes an ellipse (GPU SDF-accelerated).
     *  @param[in] cx  Center X.  @param[in] cy  Center Y.
     *  @param[in] rx  Horizontal radius.  @param[in] ry  Vertical radius.
     */
    void strokeEllipse(float cx, float cy, float rx, float ry);

    /*! @brief Fills a rounded rectangle (GPU SDF-accelerated).
     *  @param[in] x       Left edge.  @param[in] y       Top edge.
     *  @param[in] w       Width.      @param[in] h       Height.
     *  @param[in] radius  Corner radius applied to all four corners.
     */
    void fillRoundedRect(float x, float y, float w, float h, float radius);

    /*! @brief Strokes a rounded rectangle (GPU SDF-accelerated).
     *  @param[in] x       Left edge.  @param[in] y       Top edge.
     *  @param[in] w       Width.      @param[in] h       Height.
     *  @param[in] radius  Corner radius applied to all four corners.
     */
    void strokeRoundedRect(float x, float y, float w, float h, float radius);

    // --- Path drawing (HTML5 Canvas style) ---

    /*! @brief Clears the current path and begins a new one. */
    void beginPath();

    /*! @brief Moves the path cursor to a new position without drawing.
     *  @param[in] x  Target X.  @param[in] y  Target Y.
     */
    void moveTo(float x, float y);

    /*! @brief Adds a straight line segment from the current position.
     *  @param[in] x  End X.  @param[in] y  End Y.
     */
    void lineTo(float x, float y);

    /*! @brief Adds a cubic Bézier curve from the current position.
     *  @param[in] cp1x,cp1y  First control point.
     *  @param[in] cp2x,cp2y  Second control point.
     *  @param[in] x,y        End point.
     */
    void bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y);

    /*! @brief Adds a quadratic Bézier curve from the current position.
     *  @param[in] cpx,cpy  Control point.
     *  @param[in] x,y      End point.
     */
    void quadraticCurveTo(float cpx, float cpy, float x, float y);

    /*! @brief Adds a circular arc to the current path.
     *  @param[in] cx     Center X.
     *  @param[in] cy     Center Y.
     *  @param[in] r      Radius.
     *  @param[in] start  Start angle in radians.
     *  @param[in] end    End angle in radians.
     *  @param[in] ccw    If `true`, arc is drawn counter-clockwise.
     */
    void arc(float cx, float cy, float r, float start, float end, bool ccw = false);

    /*! @brief Closes the current sub-path with a straight line back to its start. */
    void closePath();

    /*! @brief Fills the current path with the current fill color. */
    void fill();

    /*! @brief Fills the current path with a Paint (supports gradients).
     *  @param[in] paint    Paint descriptor (solid, linear gradient, or radial gradient).
     *  @param[in] opacity  Overall opacity multiplier [0, 1].
     */
    void fillWithPaint(const Paint& paint, float opacity = 1.0f);

    /*! @brief Strokes the current path with the current stroke color and line width. */
    void stroke();

    // --- Text ---

    /*! @brief Draws filled text at the given position.
     *
     *  Requires a font to be loaded via @ref setFont.
     *
     *  @param[in] text  UTF-8 text string to render. Unsupported glyphs use `?`.
     *  @param[in] x     Left edge of the text baseline.
     *  @param[in] y     Vertical position of the text baseline.
     */
    void fillText(const std::string& text, float x, float y);

    /*! @brief Measures the untransformed width of a text string using the active font.
     *
     *  Requires a font to be loaded via @ref setFont.
     *
     *  @param[in] text  Text to measure.
     *  @return Width in pixels, or `0.0f` if no font is active.
     */
    [[nodiscard]] float measureText(const std::string& text) const;

    /*! @brief Returns the active font line height in pixels.
     *
     *  @return Line height in pixels, or `0.0f` if no font is active.
     */
    [[nodiscard]] float lineHeight() const;

    // --- Images ---

    /*! @brief Loads a raster image from disk (PNG, JPG, BMP, TGA).
     *  @param[in] path  Filesystem path to the image file.
     *  @return A valid Image on success; check with Image::valid().
     */
    Image loadImage(const std::string& path);

    /*! @brief Draws a raster image.
     *  @param[in] img  The image to draw (must be valid).
     *  @param[in] x    Left edge.  @param[in] y    Top edge.
     *  @param[in] w    Width (-1 = original width).
     *  @param[in] h    Height (-1 = original height).
     */
    void drawImage(const Image& img, float x, float y, float w = -1, float h = -1);

    Renderer& renderer()
    {
        return renderer_;
    }

private:
    struct State
    {
        struct ClipRect
        {
            float x = 0.0f;
            float y = 0.0f;
            float width = 0.0f;
            float height = 0.0f;
            bool enabled = false;
        };

        Color fillColor{Color::Black};
        Color strokeColor{Color::Black};
        float lineWidth = 1.0f;
        Mat3x3 transform = Mat3x3::identity();
        ClipRect clip{};
    };

    void applyClipState();

    Renderer renderer_;
    Path2D currentPath_;
    State currentState_;
    std::vector<State> stateStack_;

    struct FontEntry
    {
        std::string path;
        float size;
        Font font;
    };
    std::vector<FontEntry> fonts_;
    Font* activeFont_ = nullptr;
};

} // namespace vectorgl
