#pragma once
#include <array>
#include <cstdint>
#include <memory>
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
    Renderer();
    ~Renderer();
    Renderer(Renderer&&) noexcept;
    Renderer& operator=(Renderer&&) noexcept;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /*! @brief Initializes GPU resources (shaders, VAOs, FBOs). */
    void init();

    /*! @brief Releases all GPU resources. */
    void destroy();

    /*! @brief Begins a new frame.
     *  @param[in] fbWidth   Framebuffer width in pixels.
     *  @param[in] fbHeight  Framebuffer height in pixels.
     */
    void beginFrame(int fbWidth, int fbHeight);

    /*! @brief Ends the frame and flushes remaining batches. */
    void endFrame();

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

    /*! @brief Returns the current framebuffer width in pixels. */
    int fbWidth() const;

    /*! @brief Returns the current framebuffer height in pixels. */
    int fbHeight() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace vectorgl
