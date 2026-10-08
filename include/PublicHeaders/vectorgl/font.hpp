#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "vectorgl/text.hpp"

namespace vectorgl
{
/*! Glyph metrics and immutable UV coordinates in an atlas page. */
struct GlyphInfo
{
    float u0, v0, u1, v1;
    float xoff, yoff, xadvance;
    float width, height;
    uint32_t texture = 0;
};
/*! TrueType font with lazily rasterized Unicode SDF glyphs. Font data is retained
 * for metrics and rasterization. The cache is bounded to four 1024x1024 R8 pages
 * and 4096 distinct glyphs. Pages never relocate or evict queued glyphs.
 * Load, getGlyph and destruction require a current OpenGL context.
 */
class Font
{
public:
    Font();
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;
    /*! Loads a trusted TrueType outline font at an em size in pixels in (0, 256].
     * CFF outlines and font collections are not supported by this SDF loader.
     */
    [[nodiscard]] bool load(const std::string& path, float size);
    void destroy();
    /*! Lazily uploads a glyph from the font. Missing glyphs or a full cache use
     * the replacement glyph (U+FFFD, then '?', then .notdef). The returned
     * pointer stays valid until destroy/reload; use GlyphInfo::texture to draw.
     */
    [[nodiscard]] const GlyphInfo* getGlyph(int codepoint) const;
    [[nodiscard]] bool hasGlyph(uint32_t codepoint) const;
    /*! Metrics and layout never allocate textures or upload glyphs. */
    [[nodiscard]] float advance(uint32_t codepoint) const;
    [[nodiscard]] float kerning(uint32_t left, uint32_t right) const;
    [[nodiscard]] TextLayout layoutText(std::string_view text, const TextLayoutOptions& options = {}) const;
    /*! First atlas page; individual glyphs can live on other pages. */
    [[nodiscard]] uint32_t atlasTexture() const;
    [[nodiscard]] std::size_t atlasPageCount() const;
    [[nodiscard]] std::size_t glyphCacheSize() const;
    [[nodiscard]] float lineHeight() const;
    [[nodiscard]] float ascent() const;
    [[nodiscard]] float renderScale() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace vectorgl
