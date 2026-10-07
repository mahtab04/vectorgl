#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace vectorgl
{

/*! @brief Glyph metrics and UV coordinates within the font atlas. */
struct GlyphInfo
{
    float u0, v0, u1, v1;
    float xoff, yoff, xadvance;
    float width, height;
};

/*! @brief A TrueType font loaded from disk, rasterized to a GPU texture atlas.
 *
 *  Used internally by Canvas::setFont / Canvas::fillText. Users typically
 *  interact with fonts through the Canvas API rather than directly.
 */
class Font
{
public:
    Font() = default;
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;

    /*! @brief Loads a TrueType font from a `.ttf` file.
     *  @param[in] path  Filesystem path to the font file.
     *  @param[in] size  Desired font size in pixels.
     *  Sizes above 256 pixels or glyph sets that exceed the fixed atlas are rejected.
     *  @return `true` if the font was loaded and atlas was generated.
     */
    [[nodiscard]] bool load(const std::string& path, float size);

    /*! @brief Releases the font atlas texture and all cached data. */
    void destroy();

    /*! @brief Looks up glyph metrics for a Unicode codepoint.
     *  @param[in] codepoint  Unicode codepoint (e.g. 'A' = 65).
     *  @return Pointer to GlyphInfo, or `nullptr` if the codepoint is not in the atlas.
     */
    [[nodiscard]] const GlyphInfo* getGlyph(int codepoint) const;

    /*! @brief Returns the OpenGL texture ID of the font atlas. */
    [[nodiscard]] uint32_t atlasTexture() const
    {
        return atlasTexture_;
    }
    /*! @brief Returns the line height in pixels. */
    [[nodiscard]] float lineHeight() const
    {
        return lineHeight_;
    }
    /*! @brief Returns the font ascent in pixels. */
    [[nodiscard]] float ascent() const
    {
        return ascent_;
    }
    /*! @brief Returns the render scale factor used for SDF generation. */
    [[nodiscard]] float renderScale() const
    {
        return renderScale_;
    }

private:
    std::unordered_map<int, GlyphInfo> glyphs_;
    uint32_t atlasTexture_ = 0;
    float lineHeight_ = 0;
    float ascent_ = 0;
    float size_ = 0;
    float renderScale_ = 1.0f;
};

} // namespace vectorgl
