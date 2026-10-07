#include "vectorgl/font.hpp"

#include <glad/gl.h>

#include <cmath>
#include <fstream>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace vectorgl
{

Font::~Font()
{
    destroy();
}

Font::Font(Font&& other) noexcept
    : glyphs_(std::move(other.glyphs_)), atlasTexture_(other.atlasTexture_), lineHeight_(other.lineHeight_),
      ascent_(other.ascent_), size_(other.size_), renderScale_(other.renderScale_)
{
    other.atlasTexture_ = 0;
    other.lineHeight_ = 0;
    other.ascent_ = 0;
    other.size_ = 0;
    other.renderScale_ = 1.0f;
    other.glyphs_.clear();
}

Font& Font::operator=(Font&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        glyphs_ = std::move(other.glyphs_);
        atlasTexture_ = other.atlasTexture_;
        lineHeight_ = other.lineHeight_;
        ascent_ = other.ascent_;
        size_ = other.size_;
        renderScale_ = other.renderScale_;
        other.atlasTexture_ = 0;
        other.lineHeight_ = 0;
        other.ascent_ = 0;
        other.size_ = 0;
        other.renderScale_ = 1.0f;
        other.glyphs_.clear();
    }
    return *this;
}

bool Font::load(const std::string& path, float size)
{
    destroy();
    if (!(size > 0.0f) || !std::isfinite(size) || size > 256.0f)
        return false;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    auto fileSize = file.tellg();
    if (fileSize <= 0)
        return false;
    file.seekg(0);
    std::vector<uint8_t> fontData(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(fontData.data()), fileSize))
    {
        return false;
    }

    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontData.data(), 0))
        return false;

    // Generate atlas at higher internal resolution for better SDF precision,
    // then scale down at render time for the requested display size.
    constexpr float kMinAtlasSize = 48.0f;
    float internalSize = std::max(size, kMinAtlasSize);
    float renderScale = size / internalSize;
    float scale = stbtt_ScaleForPixelHeight(&fontInfo, internalSize);

    int iAscent, iDescent, iLineGap;
    stbtt_GetFontVMetrics(&fontInfo, &iAscent, &iDescent, &iLineGap);
    float ascent = iAscent * scale * renderScale;
    float lineHeight = (iAscent - iDescent + iLineGap) * scale * renderScale;

    constexpr int atlasW = 1024, atlasH = 1024;
    std::vector<uint8_t> atlasData(atlasW * atlasH, 0);
    std::unordered_map<int, GlyphInfo> glyphs;

    // SDF parameters — higher padding for better distance range
    int sdfPadding = 8;
    uint8_t sdfOnEdge = 128;
    float sdfPixelDist = static_cast<float>(sdfPadding);

    int penX = sdfPadding, penY = sdfPadding;
    int rowHeight = 0;

    // Keep the fixed atlas predictable while covering ASCII and Latin-1.
    // Canvas substitutes '?' for codepoints that are not present.
    for (int cp = 32; cp < 256; ++cp)
    {
        int glyph = stbtt_FindGlyphIndex(&fontInfo, cp);
        if (glyph == 0 && cp != 32)
            continue;

        int ix0, iy0, ix1, iy1;
        stbtt_GetGlyphBitmapBox(&fontInfo, glyph, scale, scale, &ix0, &iy0, &ix1, &iy1);

        int gw = ix1 - ix0;
        int gh = iy1 - iy0;
        int sdfW = gw + 2 * sdfPadding;
        int sdfH = gh + 2 * sdfPadding;
        if (sdfW + 2 * sdfPadding >= atlasW || sdfH + 2 * sdfPadding >= atlasH)
            return false;

        if (penX + sdfW >= atlasW)
        {
            penX = sdfPadding;
            penY += rowHeight + sdfPadding;
            rowHeight = 0;
        }
        if (penY + sdfH >= atlasH)
            return false;

        unsigned char* sdfBitmap = stbtt_GetGlyphSDF(&fontInfo, scale, glyph, sdfPadding, sdfOnEdge, sdfPixelDist,
                                                     &sdfW, &sdfH, nullptr, nullptr);

        if (sdfBitmap)
        {
            for (int row = 0; row < sdfH; ++row)
            {
                for (int col = 0; col < sdfW; ++col)
                {
                    int dx = penX + col;
                    int dy = penY + row;
                    if (dx < atlasW && dy < atlasH)
                        atlasData[dy * atlasW + dx] = sdfBitmap[row * sdfW + col];
                }
            }
            stbtt_FreeSDF(sdfBitmap, nullptr);
        }
        else if (gw > 0 && gh > 0)
            return false;

        int advW, lsb;
        stbtt_GetGlyphHMetrics(&fontInfo, glyph, &advW, &lsb);

        GlyphInfo gi{};
        gi.u0 = static_cast<float>(penX) / atlasW;
        gi.v0 = static_cast<float>(penY) / atlasH;
        gi.u1 = static_cast<float>(penX + sdfW) / atlasW;
        gi.v1 = static_cast<float>(penY + sdfH) / atlasH;
        gi.xoff = static_cast<float>(ix0 - sdfPadding) * renderScale;
        gi.yoff = static_cast<float>(iy0 - sdfPadding) * renderScale;
        gi.width = static_cast<float>(sdfW) * renderScale;
        gi.height = static_cast<float>(sdfH) * renderScale;
        gi.xadvance = advW * scale * renderScale;

        glyphs[cp] = gi;

        penX += sdfW + sdfPadding;
        rowHeight = std::max(rowHeight, sdfH);
    }

    if (!glyphs.contains('?'))
        return false;

    uint32_t atlasTexture = 0;
    glGenTextures(1, &atlasTexture);
    if (atlasTexture == 0)
        return false;
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlasW, atlasH, 0, GL_RED, GL_UNSIGNED_BYTE, atlasData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glyphs_ = std::move(glyphs);
    atlasTexture_ = atlasTexture;
    lineHeight_ = lineHeight;
    ascent_ = ascent;
    size_ = size;
    renderScale_ = renderScale;

    return true;
}

void Font::destroy()
{
    if (atlasTexture_)
    {
        glDeleteTextures(1, &atlasTexture_);
    }
    atlasTexture_ = 0;
    glyphs_.clear();
    lineHeight_ = 0;
    ascent_ = 0;
    size_ = 0;
    renderScale_ = 1.0f;
}

const GlyphInfo* Font::getGlyph(int codepoint) const
{
    auto it = glyphs_.find(codepoint);
    return it != glyphs_.end() ? &it->second : nullptr;
}

} // namespace vectorgl
