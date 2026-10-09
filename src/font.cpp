#include "vectorgl/font.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <unordered_map>
#include <vector>

#include "vectorgl/detail/gl_handle.hpp"
#include "vectorgl/detail/utf8.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#ifdef VECTORGL_HAS_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

namespace vectorgl
{
namespace
{
constexpr int atlasSize = 1024, padding = 8;
constexpr size_t maxPages = 4, maxGlyphs = 4096;

// Lazy uploads must also work in hosts using PBOs or nondefault unpack state.
struct UploadState
{
    GLint texture = 0, buffer = 0, alignment = 4, rowLength = 0, skipRows = 0, skipPixels = 0;
    UploadState()
    {
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    }
    ~UploadState()
    {
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(buffer));
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels);
    }
};

bool validDirectory(const std::vector<uint8_t>& data)
{
    if (data.size() < 12)
        return false;
    auto u32 = [&](size_t at) {
        return (uint32_t(data[at]) << 24) | (uint32_t(data[at + 1]) << 16) | (uint32_t(data[at + 2]) << 8) |
               data[at + 3];
    };
    const auto signature = u32(0);
    if (signature != 0x00010000 && signature != 0x4F54544F && signature != 0x74727565)
        return false;
    const size_t count = (size_t(data[4]) << 8) | data[5];
    if (count > (data.size() - 12) / 16)
        return false;
    for (size_t i = 0; i < count; ++i)
    {
        const size_t offset = u32(12 + i * 16 + 8), length = u32(12 + i * 16 + 12);
        if (offset > data.size() || length > data.size() - offset)
            return false;
    }
    return true;
}
} // namespace

struct Font::Impl
{
    struct Page
    {
        detail::GLTexture texture;
        int x = 1, y = 1, rowHeight = 0;
    };
    std::vector<uint8_t> data;
    stbtt_fontinfo info{};
    std::vector<Page> pages;
    std::unordered_map<uint64_t, GlyphInfo> glyphs;
    float size = 0;
#ifdef VECTORGL_HAS_FREETYPE
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    ~Impl()
    {
        if (face)
            FT_Done_Face(face);
        if (library)
            FT_Done_FreeType(library);
    }
#endif
    float scale = 0, rasterScale = 0, displayScale = 1, lineHeight = 0, ascent = 0;
    int fallback = 0;
    bool saturated = false;

    int glyphIndex(uint32_t cp) const
    {
        int index = detail::isScalar(cp) ? stbtt_FindGlyphIndex(&info, static_cast<int>(cp)) : 0;
        return index ? index : fallback;
    }
    bool addPage()
    {
        if (pages.size() >= maxPages)
            return false;
        Page page;
        page.texture.create();
        if (!page.texture.get())
            return false;
        UploadState state;
        glBindTexture(GL_TEXTURE_2D, page.texture.get());
        std::vector<uint8_t> zero(atlasSize * atlasSize, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlasSize, atlasSize, 0, GL_RED, GL_UNSIGNED_BYTE, zero.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        pages.push_back(std::move(page));
        return true;
    }
    const GlyphInfo* storeBitmap(uint64_t key, GlyphInfo glyph, int width, int height, const uint8_t* bitmap)
    {
        Page* page = &pages.back();
        if (page->x + width + 1 > atlasSize)
        {
            page->x = 1;
            page->y += page->rowHeight + 1;
            page->rowHeight = 0;
        }
        if (page->y + height + 1 > atlasSize)
        {
            if (!addPage())
            {
                saturated = pages.size() >= maxPages;
                return nullptr;
            }
            page = &pages.back();
        }
        UploadState state;
        glBindTexture(GL_TEXTURE_2D, page->texture.get());
        glTexSubImage2D(GL_TEXTURE_2D, 0, page->x, page->y, width, height, GL_RED, GL_UNSIGNED_BYTE, bitmap);
        glyph.texture = page->texture.get();
        glyph.u0 = float(page->x) / atlasSize;
        glyph.v0 = float(page->y) / atlasSize;
        glyph.u1 = float(page->x + width) / atlasSize;
        glyph.v1 = float(page->y + height) / atlasSize;
        page->x += width + 1;
        page->rowHeight = std::max(page->rowHeight, height);
        return &glyphs.emplace(key, glyph).first->second;
    }
    const GlyphInfo* cacheBitmap(int index, int pixelSize, int phaseX)
    {
#ifdef VECTORGL_HAS_FREETYPE
        if (!face)
            return nullptr;
        const uint64_t key =
            uint64_t(index) | (uint64_t(1) << 32) | (uint64_t(pixelSize) << 33) | (uint64_t(phaseX) << 40);
        if (auto it = glyphs.find(key); it != glyphs.end())
            return &it->second;
        if (saturated || glyphs.size() >= maxGlyphs)
            return nullptr;
        int x0, y0, x1, y1;
        const float bitmapScale = stbtt_ScaleForMappingEmToPixels(&info, static_cast<float>(pixelSize));
        stbtt_GetGlyphBitmapBox(&info, index, bitmapScale, bitmapScale, &x0, &y0, &x1, &y1);
        if (x1 - x0 + 4 > atlasSize || y1 - y0 + 4 > atlasSize)
            return nullptr;
        if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixelSize)))
            return nullptr;
        FT_Vector delta{phaseX * 16, 0};
        FT_Set_Transform(face, nullptr, &delta);
        if (FT_Load_Glyph(face, static_cast<FT_UInt>(index),
                          FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_LIGHT | FT_LOAD_FORCE_AUTOHINT) ||
            FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL))
            return nullptr;
        const auto& source = face->glyph->bitmap;
        GlyphInfo glyph{};
        glyph.sdf = false;
        glyph.xadvance = advanceForGlyph(index);
        if (source.width == 0 || source.rows == 0)
        {
            glyph.texture = pages.front().texture.get();
            return &glyphs.emplace(key, glyph).first->second;
        }
        if (source.pixel_mode != FT_PIXEL_MODE_GRAY || source.width + 4 > atlasSize || source.rows + 4 > atlasSize)
            return nullptr;
        // A transparent border prevents filtering from bleeding into neighbours.
        const int width = static_cast<int>(source.width) + 2, height = static_cast<int>(source.rows) + 2;
        std::vector<uint8_t> bitmap(size_t(width) * height, 0);
        for (unsigned row = 0; row < source.rows; ++row)
        {
            const auto* pixels = source.buffer + (source.pitch >= 0 ? row : source.rows - 1 - row) *
                                                     static_cast<size_t>(std::abs(source.pitch));
            std::copy_n(pixels, source.width, bitmap.data() + size_t(row + 1) * width + 1);
        }
        glyph.xoff = static_cast<float>(face->glyph->bitmap_left - 1);
        glyph.yoff = static_cast<float>(-face->glyph->bitmap_top - 1);
        glyph.width = static_cast<float>(width);
        glyph.height = static_cast<float>(height);
        return storeBitmap(key, glyph, width, height, bitmap.data());
#else
        (void)index;
        (void)pixelSize;
        (void)phaseX;
        return nullptr;
#endif
    }
    float advanceForGlyph(int index) const
    {
        int advance, bearing;
        stbtt_GetGlyphHMetrics(&info, index, &advance, &bearing);
        return advance * scale;
    }
    const GlyphInfo* cacheGlyph(int index)
    {
        if (auto it = glyphs.find(index); it != glyphs.end())
            return &it->second;
        if (saturated || glyphs.size() >= maxGlyphs)
            return nullptr;
        GlyphInfo glyph{};
        int advance, bearing;
        stbtt_GetGlyphHMetrics(&info, index, &advance, &bearing);
        glyph.xadvance = advance * scale;
        if (stbtt_IsGlyphEmpty(&info, index))
        {
            glyph.texture = pages.front().texture.get();
            return &glyphs.emplace(index, glyph).first->second;
        }
        int x0, y0, x1, y1;
        stbtt_GetGlyphBitmapBox(&info, index, rasterScale, rasterScale, &x0, &y0, &x1, &y1);
        if (x1 - x0 + 2 * padding + 2 > atlasSize || y1 - y0 + 2 * padding + 2 > atlasSize)
            return nullptr;
        int width, height, xoff, yoff;
        auto freeSdf = [](unsigned char* p) { stbtt_FreeSDF(p, nullptr); };
        std::unique_ptr<unsigned char, decltype(freeSdf)> bitmap(
            stbtt_GetGlyphSDF(&info, rasterScale, index, padding, 128, 16.0f, &width, &height, &xoff, &yoff), freeSdf);
        if (!bitmap || width + 2 > atlasSize || height + 2 > atlasSize)
            return nullptr;
        glyph.xoff = xoff * displayScale;
        glyph.yoff = yoff * displayScale;
        glyph.width = width * displayScale;
        glyph.height = height * displayScale;
        return storeBitmap(static_cast<uint64_t>(index), glyph, width, height, bitmap.get());
    }
};

Font::Font() = default;
Font::~Font() = default;
Font::Font(Font&&) noexcept = default;
Font& Font::operator=(Font&&) noexcept = default;

bool Font::load(const std::string& path, float size)
{
    destroy();
    if (!(size > 0) || !std::isfinite(size) || size > 256)
        return false;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return false;
    const auto length = file.tellg();
    if (length <= 0 || length > 64 * 1024 * 1024)
        return false;
    auto impl = std::make_unique<Impl>();
    impl->data.resize(static_cast<size_t>(length));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(impl->data.data()), length) || !validDirectory(impl->data) ||
        !stbtt_InitFont(&impl->info, impl->data.data(), 0) || impl->info.cff.size > 0)
        return false;
    impl->size = size;
#ifdef VECTORGL_HAS_FREETYPE
    if (!FT_Init_FreeType(&impl->library))
        (void)FT_New_Memory_Face(impl->library, impl->data.data(), static_cast<FT_Long>(impl->data.size()), 0,
                                 &impl->face);
#endif
    const float internalSize = std::max(size, 64.0f);
    // Font sizes specify the em square, not the ascent/descent span. Using
    // ScaleForPixelHeight made e.g. Segoe UI's requested 12px only a 9px em.
    impl->scale = stbtt_ScaleForMappingEmToPixels(&impl->info, size);
    impl->rasterScale = stbtt_ScaleForMappingEmToPixels(&impl->info, internalSize);
    impl->displayScale = size / internalSize;
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&impl->info, &ascent, &descent, &gap);
    impl->ascent = ascent * impl->scale;
    impl->lineHeight = (ascent - descent + gap) * impl->scale;
    if (!(impl->lineHeight > 0) || !std::isfinite(impl->lineHeight) || !std::isfinite(impl->ascent))
        return false;
    impl->fallback = stbtt_FindGlyphIndex(&impl->info, 0xFFFD);
    if (!impl->fallback)
        impl->fallback = stbtt_FindGlyphIndex(&impl->info, '?');
    if (!impl->addPage() || !impl->cacheGlyph(impl->fallback))
        return false;
    impl_ = std::move(impl);
    return true;
}

void Font::destroy()
{
    impl_.reset();
}
const GlyphInfo* Font::getGlyph(int cp) const
{
    if (!impl_)
        return nullptr;
    if (auto* glyph = impl_->cacheGlyph(impl_->glyphIndex(static_cast<uint32_t>(cp))))
        return glyph;
    return &impl_->glyphs.at(impl_->fallback);
}
const GlyphInfo* Font::getBitmapGlyph(int cp, int pixelSize, int phaseX) const
{
    if (impl_ && pixelSize >= 1 && pixelSize <= 64 && phaseX >= 0 && phaseX <= 3)
        if (auto* glyph = impl_->cacheBitmap(impl_->glyphIndex(static_cast<uint32_t>(cp)), pixelSize, phaseX))
            return glyph;
    return getGlyph(cp);
}
bool Font::hasBitmapSupport() const
{
#ifdef VECTORGL_HAS_FREETYPE
    return impl_ && impl_->face;
#else
    return false;
#endif
}
float Font::emSize() const
{
    return impl_ ? impl_->size : 0;
}
bool Font::hasGlyph(uint32_t cp) const
{
    return impl_ && detail::isScalar(cp) && stbtt_FindGlyphIndex(&impl_->info, static_cast<int>(cp)) != 0;
}
float Font::advance(uint32_t cp) const
{
    if (!impl_)
        return 0;
    int advance, bearing;
    stbtt_GetGlyphHMetrics(&impl_->info, impl_->glyphIndex(cp), &advance, &bearing);
    return advance * impl_->scale;
}
float Font::kerning(uint32_t left, uint32_t right) const
{
    return impl_ && left && right
               ? stbtt_GetGlyphKernAdvance(&impl_->info, impl_->glyphIndex(left), impl_->glyphIndex(right)) *
                     impl_->scale
               : 0;
}
uint32_t Font::atlasTexture() const
{
    return impl_ && !impl_->pages.empty() ? impl_->pages.front().texture.get() : 0;
}
size_t Font::atlasPageCount() const
{
    return impl_ ? impl_->pages.size() : 0;
}
size_t Font::glyphCacheSize() const
{
    return impl_ ? impl_->glyphs.size() : 0;
}
float Font::lineHeight() const
{
    return impl_ ? impl_->lineHeight : 0;
}
float Font::ascent() const
{
    return impl_ ? impl_->ascent : 0;
}
float Font::renderScale() const
{
    return impl_ ? impl_->displayScale : 1;
}
} // namespace vectorgl
