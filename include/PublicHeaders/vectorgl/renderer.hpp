#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "vectorgl/color.hpp"
#include "vectorgl/node.hpp"
#include "vectorgl/paint.hpp"
#include "vectorgl/path.hpp"

namespace vectorgl
{

/*! @brief Low-level GPU renderer for SDF shapes, paths, textures, and post-processing.
 *
 *  Most users should prefer the Canvas or Scene APIs.  Access the Renderer
 *  directly only when you need custom SDF batching, manual path rendering, or
 *  FBO-based effects.
 */
class Renderer
{
public:
    struct RoundedClip
    {
        Vec2 position{};
        Vec2 size{};
        float radius = 0.0f;
        Mat3x3 transform = Mat3x3::identity();
    };

    Renderer();
    ~Renderer();
    Renderer(Renderer&&) noexcept;
    Renderer& operator=(Renderer&&) noexcept;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /*! @brief Initializes GPU resources (shaders, VAOs, FBOs).
     *  GLAD must be loaded and a valid OpenGL context must be current.
     *  @throws std::runtime_error if GLAD or the context is unavailable.
     *  @throws std::logic_error if the renderer is already initialized.
     */
    void init();

    /*! @brief Releases all GPU resources. */
    void destroy();

    /*! @brief Begins a new frame.
     *  @param[in] fbWidth   Framebuffer width in pixels.
     *  @param[in] fbHeight  Framebuffer height in pixels.
     *  @throws std::logic_error if the renderer is uninitialized or a frame is already active.
     *  @throws std::invalid_argument if either dimension is not positive.
     */
    void beginFrame(int fbWidth, int fbHeight);

    /*! @brief Ends the frame and flushes remaining batches. */
    void endFrame();

    /*! @brief Enables an axis-aligned scissor rectangle in framebuffer coordinates.
     *
     *  The rectangle uses VectorGL's top-left origin. Pending SDF batches are
     *  flushed before the GPU scissor state changes.
     */
    void setClipRect(int x, int y, int width, int height);

    /*! @brief Replaces the active nested rounded-rectangle clip stack.
     *
     *  Rounded clips are rendered into the stencil buffer in order, producing
     *  their geometric intersection. The current framebuffer must provide a
     *  stencil buffer.
     */
    void setRoundedClips(const std::vector<RoundedClip>& clips);

    /*! @brief Disables the active scissor rectangle after flushing pending work. */
    void clearClip();

    /*! @brief Returns whether GPU resources have been initialized. */
    [[nodiscard]] bool isInitialized() const noexcept;

    /*! @brief Returns whether a frame is currently being recorded. */
    [[nodiscard]] bool isFrameActive() const noexcept;

    // --- SDF primitive rendering (GPU-evaluated shapes) ---

    /*! @brief Submits an SDF rectangle to the batch.
     *  @param[in] pos        Top-left position.
     *  @param[in] size       Rectangle dimensions.
     *  @param[in] style      Fill / stroke style.
     *  @param[in] transform  World transform matrix.
     */
    void drawSDFRect(Vec2 pos, Vec2 size, const NodeStyle& style, const Mat3x3& transform);

    /*! @brief Submits an SDF circle to the batch.
     *  @param[in] center     Center position.
     *  @param[in] radius     Circle radius.
     *  @param[in] style      Fill / stroke style.
     *  @param[in] transform  World transform matrix.
     */
    void drawSDFCircle(Vec2 center, float radius, const NodeStyle& style, const Mat3x3& transform);

    /*! @brief Submits an SDF ellipse to the batch.
     *  @param[in] center     Center position.
     *  @param[in] radii      Horizontal / vertical radii as Vec2.
     *  @param[in] style      Fill / stroke style.
     *  @param[in] transform  World transform matrix.
     */
    void drawSDFEllipse(Vec2 center, Vec2 radii, const NodeStyle& style, const Mat3x3& transform);

    /*! @brief Submits an SDF rounded rectangle to the batch.
     *  @param[in] pos          Top-left position.
     *  @param[in] size         Rectangle dimensions.
     *  @param[in] cornerRadii  Per-corner radii {TL, TR, BR, BL}.
     *  @param[in] style        Fill / stroke style.
     *  @param[in] transform    World transform matrix.
     */
    void drawSDFRoundedRect(Vec2 pos, Vec2 size, const std::array<float, 4>& cornerRadii, const NodeStyle& style,
                            const Mat3x3& transform);

    /*! @brief Flushes all batched SDF instances to the GPU. */
    void flushSDF();
    /*! @brief Flushes pending shapes and glyphs without ending the frame. */
    void flush();

    // --- Complex path rendering ---

    /*! @brief Fills a tessellated path.
     *  @param[in] subPaths   Tessellated sub-paths (each a vector of points).
     *  @param[in] color      Fill color.
     *  @param[in] transform  World transform matrix.
     */
    void fillPath(const std::vector<std::vector<Vec2>>& subPaths, Color color, const Mat3x3& transform);

    /*! @brief Fills a complex path with a Paint (supports gradients).
     *  @param[in] subPaths   Tessellated sub-paths.
     *  @param[in] paint      Paint descriptor (solid, linear gradient, or radial gradient).
     *  @param[in] opacity    Overall opacity multiplier [0, 1].
     *  @param[in] transform  World transform matrix.
     */
    void fillPathWithPaint(const std::vector<std::vector<Vec2>>& subPaths, const Paint& paint, float opacity,
                           const Mat3x3& transform);

    /*! @brief Strokes a complex path.
     *  @param[in] subPaths   Tessellated sub-paths.
     *  @param[in] color      Stroke color.
     *  @param[in] width      Stroke width in pixels.
     *  @param[in] transform  World transform matrix.
     */
    void strokePath(const std::vector<std::vector<Vec2>>& subPaths, Color color, float width, const Mat3x3& transform);

    // --- Textured rendering ---

    /*! @brief Draws a textured quad.
     *  @param[in] x,y      Top-left position.
     *  @param[in] w,h      Quad dimensions.
     *  @param[in] texture   OpenGL texture ID.
     *  @param[in] tint      Tint color (modulated with texture).
     *  @param[in] transform World transform matrix.
     */
    void drawTexturedQuad(float x, float y, float w, float h, uint32_t texture, Color tint, const Mat3x3& transform);

    /*! @brief Draws a single glyph quad from a font atlas.
     *  Consecutive glyphs share a draw call. Keep the atlas alive until flush() or endFrame().
     *  @param[in] x,y       Position.     @param[in] w,h  Quad size.
     *  @param[in] u0,v0     Top-left UV.  @param[in] u1,v1  Bottom-right UV.
     *  @param[in] texture   Font atlas texture ID.
     *  @param[in] color     Text color.
     *  @param[in] transform World transform matrix.
     */
    void drawGlyph(float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t texture,
                   Color color, const Mat3x3& transform);

    // --- FBO-based post-processing effects ---

    /*! @brief Begins capturing draw calls into an off-screen FBO for effects.
     *  @param[in] x,y  Top-left corner of the effect region.
     *  @param[in] w,h  Dimensions of the effect region.
     */
    void beginEffectPass(float x, float y, float w, float h);

    /*! @brief Ends the effect capture pass. */
    void endEffectPass();

    /*! @brief Applies a Gaussian blur post-process to the effect FBO.
     *  @param[in] radius  Blur radius in pixels.
     */
    void applyBlur(float radius);

    /*! @brief Applies a drop shadow post-process.
     *  @param[in] blur    Shadow blur radius.
     *  @param[in] offset  Shadow offset.
     *  @param[in] color   Shadow color.
     */
    void applyShadow(float blur, Vec2 offset, Color color);

    /*! @brief Applies an outer glow post-process.
     *  @param[in] radius  Glow radius in pixels.
     *  @param[in] color   Glow color.
     */
    void applyGlow(float radius, Color color);

    // --- High-level ---

    /*! @brief Renders a scene-graph Node and all its children.
     *  @param[in] node  Pointer to the root node to render.
     */
    void renderNode(Node* node);

    /*! Draws a loaded font using the same UTF-8 layout and snapping as Canvas.
     * x/y are the layout top-left. Keep the font alive until flush/endFrame;
     * renderNode() retains shared node fonts automatically for queued glyphs.
     */
    void drawText(const Font& font, std::string_view text, float x, float y, Color color, const Mat3x3& transform,
                  const TextLayoutOptions& options = {}, bool pixelSnap = true);

    /*! @brief Returns the current framebuffer width in pixels. */
    int fbWidth() const;

    /*! @brief Returns the current framebuffer height in pixels. */
    int fbHeight() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vectorgl
