#include <glad/gl.h>

#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vectorgl/canvas.hpp>
#include <vectorgl/font.hpp>

#include "test_utils.hpp"

namespace
{
GLuint nextTexture = 0;
bool failAllocation = false;
std::unordered_set<GLuint> textures;
void GLAD_API_PTR genTextures(GLsizei count, GLuint* ids)
{
    for (int i = 0; i < count; ++i)
    {
        ids[i] = failAllocation ? 0 : ++nextTexture;
        if (ids[i])
            textures.insert(ids[i]);
    }
}
void GLAD_API_PTR deleteTextures(GLsizei count, const GLuint* ids)
{
    for (int i = 0; i < count; ++i)
        expect(textures.erase(ids[i]) == 1, "texture destroyed exactly once");
}
void GLAD_API_PTR bindTexture(GLenum, GLuint) {}
void GLAD_API_PTR texImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) {}
void GLAD_API_PTR texParameteri(GLenum, GLenum, GLint) {}
int uploads = 0;
void GLAD_API_PTR texSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)
{
    ++uploads;
}
void GLAD_API_PTR getIntegerv(GLenum name, GLint* value)
{
    *value = name == GL_UNPACK_ALIGNMENT ? 4 : 0;
}
void GLAD_API_PTR pixelStorei(GLenum, GLint) {}
void GLAD_API_PTR bindBuffer(GLenum, GLuint) {}
} // namespace

int main()
{
    using namespace vectorgl;
    glad_glGenTextures = genTextures;
    glad_glDeleteTextures = deleteTextures;
    glad_glBindTexture = bindTexture;
    glad_glTexImage2D = texImage2D;
    glad_glTexParameteri = texParameteri;
    glad_glTexSubImage2D = texSubImage2D;
    glad_glGetIntegerv = getIntegerv;
    glad_glPixelStorei = pixelStorei;
    glad_glBindBuffer = bindBuffer;
    {
        Font font;
        expect(!font.load(VECTORGL_TEST_FONT, 1600), "oversized font is rejected before rasterization");
        expect(!font.load(VECTORGL_TEST_FONT, std::numeric_limits<float>::infinity()), "nonfinite size is rejected");
        expect(font.load(VECTORGL_TEST_FONT, 256), "large fonts load without packing every glyph upfront");
        font.destroy();
        expect(textures.empty(), "large font atlas released");
        expect(font.load(VECTORGL_TEST_FONT, 16), "complete atlas loads");
        expect(font.getGlyph('?') != nullptr && font.getGlyph(255) != nullptr, "complete glyph set is available");
        expect(font.hasGlyph(0x3A9) && font.hasGlyph(0x1F600), "BMP and supplementary-plane cmap entries");
        const auto cached = font.glyphCacheSize();
        const auto before = uploads;
        auto layout = font.layoutText("A\xCE\xA9V");
        expect(layout.glyphs.size() == 3 && font.glyphCacheSize() == cached && uploads == before,
               "Unicode measurement does not rasterize or upload");
        const auto* omega = font.getGlyph(0x3A9);
        expect(omega && omega->xadvance > font.getGlyph('A')->xadvance, "dynamic Unicode glyph has its own metrics");
        const auto after = uploads;
        expect(font.getGlyph(0x3A9) == omega && uploads == after, "cached glyph is not uploaded again");
        expect(font.getGlyph(0x10FFFF) == font.getGlyph(0xFFFD), "missing glyph resolves to replacement");
        expectNear(font.layoutText("AV").width, 17.6f, 0.001f, "known AV kerning pair affects measurement");
        TextLayoutOptions options;
        options.maxWidth = 26;
        auto wrapped = font.layoutText("AA A", options);
        expect(wrapped.lines.size() == 2 && wrapped.lines[0].byteEnd == 2 && wrapped.lines[1].byteStart == 3,
               "word wrapping trims break whitespace");
        expectNear(wrapped.lines[0].width, 19.2f, 0.001f, "wrapped line width");
        options.maxWidth = 20;
        expect(font.layoutText("AAAA", options).lines.size() == 2, "long words fall back to codepoint wrapping");
        options.maxWidth = 16;
        options.wrap = TextWrap::Character;
        auto unicode = font.layoutText("A\xCE\xA9V", options);
        expect(unicode.lines.size() == 3 && unicode.lines[1].byteStart == 1 && unicode.lines[1].byteEnd == 3,
               "wrapping does not split UTF-8 codepoints");
        options.wrap = TextWrap::None;
        options.maxWidth = 40;
        options.align = TextAlign::Center;
        auto centered = font.layoutText("AV", options);
        expectNear(centered.lines[0].x, 11.2f, 0.001f, "center alignment");
        expectNear(centered.glyphs[1].x, 19.2f, 0.001f, "aligned glyph includes kerning");
        expectNear(centered.lines[0].carets[1].x, centered.glyphs[1].x, 0.001f, "caret uses rendered kerning");
        options.align = TextAlign::Right;
        expectNear(font.layoutText("AV", options).lines[0].x, 22.4f, 0.001f, "right alignment");
        auto lines = font.layoutText("\r\nA\n");
        expect(lines.lines.size() == 3 && lines.lines[0].byteEnd == 0 && lines.lines[2].byteStart == 4,
               "CRLF and trailing empty lines");
        expectNear(font.advance('A'), 9.6f, 0.001f, "font size uses em units independent of ascender span");
        expectNear(font.lineHeight(), 24, 0.001f, "line height retains actual font metrics");
        expectNear(lines.height, 72, 0.001f, "multiline height");
        options.lineSpacing = 1.5f;
        expectNear(font.layoutText("A\nV", options).height, 60, 0.001f, "line spacing");
        expect(font.layoutText("").lines.size() == 1 && font.layoutText("").width == 0, "empty text layout");
        options.lineSpacing = 0;
        bool invalidLayout = false;
        try
        {
            (void)font.layoutText("A", options);
        }
        catch (const std::invalid_argument&)
        {
            invalidLayout = true;
        }
        expect(invalidLayout, "invalid layout options rejected");
        GLuint texture = font.atlasTexture();
        Font moved(std::move(font));
        expect(moved.atlasTexture() == texture && font.atlasTexture() == 0, "move transfers atlas ownership");
        moved.destroy();
        moved.destroy();
        expect(textures.empty(), "destroy is idempotent");
        failAllocation = true;
        expect(!font.load(VECTORGL_TEST_FONT, 16), "texture allocation failure is reported");
        failAllocation = false;
    }
    {
        Canvas canvas;
        canvas.setFontCacheLimit(2);
        expect(canvas.setFont(VECTORGL_TEST_FONT, 16), "first cached font loads");
        GLuint first = nextTexture;
        expect(canvas.setFont(VECTORGL_TEST_FONT, 18), "second cached font loads");
        GLuint second = nextTexture;
        expect(canvas.setFont(VECTORGL_TEST_FONT, 16), "cache hit reuses font");
        expect(nextTexture == second, "cache hit allocates no texture");
        expect(canvas.setFont(VECTORGL_TEST_FONT, 20), "third font loads");
        expect(canvas.fontCacheSize() == 2 && textures.size() == 2, "cache limit bounds atlas memory");
        expect(textures.contains(first) && !textures.contains(second), "least recently used font is evicted");
        canvas.setFontCacheLimit(1);
        expect(canvas.fontCacheSize() == 1 && canvas.lineHeight() > 0, "shrinking cache preserves active font");
        expect(!canvas.setFont(VECTORGL_TEST_FONT, 1600), "failed font selection leaves cache intact");
        expect(canvas.fontCacheSize() == 1 && canvas.lineHeight() > 0, "failed selection preserves active font");
        bool rejected = false;
        try
        {
            canvas.setFontCacheLimit(0);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        expect(rejected, "zero cache capacity is rejected");
        canvas.clearFontCache();
        expect(canvas.fontCacheSize() == 0 && canvas.lineHeight() == 0 && textures.empty(),
               "clearing cache releases atlases");
        expect(canvas.setFont(VECTORGL_TEST_FONT, 16), "font can be loaded after cache clear");
    }
    expect(textures.empty(), "canvas destruction releases cached textures");
    {
        Font font;
        expect(font.load(VECTORGL_TEST_FONT, 256), "page-growth font loads");
        const auto* first = font.getGlyph('A');
        const auto texture = first->texture;
        const float u0 = first->u0;
        for (int cp = 0xE000; cp < 0xE100; ++cp)
            expect(font.getGlyph(cp) != nullptr, "cache-full fallback available");
        expect(font.atlasPageCount() == 4 && font.glyphCacheSize() <= 4096, "glyph atlas memory is bounded");
        expect(first->texture == texture && first->u0 == u0, "page growth preserves existing glyphs and pointers");
        const auto before = uploads;
        for (int cp = 0xE000; cp < 0xE100; ++cp)
            (void)font.getGlyph(cp);
        expect(uploads == before, "saturated cache does not repeatedly upload or rebuild glyphs");
    }
    expect(textures.empty(), "all dynamic pages released");
}
