#include <glad/gl.h>

#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vectorgl/canvas.hpp>
#include <vectorgl/font.hpp>
#include <vectorgl/scene.hpp>

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
std::vector<uint8_t> uploadedCoverage;
void GLAD_API_PTR texSubImage2D(GLenum, GLint, GLint, GLint, GLsizei width, GLsizei height, GLenum, GLenum,
                                const void* pixels)
{
    ++uploads;
    const auto* bytes = static_cast<const uint8_t*>(pixels);
    uploadedCoverage.assign(bytes, bytes + size_t(width) * height);
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
    {
        Scene scene;
        auto font = std::make_shared<Font>();
        expect(font->load(VECTORGL_TEST_FONT, 16), "scene font loads");
        const auto before = uploads;
        TextLayoutOptions options;
        options.maxWidth = 40;
        options.align = TextAlign::Right;
        auto text = scene.text("AV", 10, 20, font, options);
        expect(text->type() == ShapeType::Text && text->font() == font, "text factory shares resource");
        expect(scene.pick(35, 25) == text, "aligned text bounds hit");
        expect(!scene.pick(11, 25), "unused alignment area excluded");
        expect(!scene.pick(35, 45), "outside font line height excluded");
        auto layout = text->textLayout();
        expectNear(layout.width, 17.6f, 0.001f, "scene layout preserves kerning");
        text->setText("A\xCE\xA9V");
        options.maxWidth = 16;
        options.wrap = TextWrap::Character;
        text->setTextLayout(options);
        expect(text->textLayout().lines.size() == 3, "scene Unicode wrapping updates after edits");
        expect(scene.pick(22, 75) == text, "wrapped text height participates in picking");
        auto group = scene.group();
        group->setPosition(150, 100);
        group->setRotation(0.4f);
        group->setScale(-2, 0.8f);
        text->setPosition(3, 4);
        text->setRotation(-0.2f);
        group->addChild(text);
        scene.update(0);
        const auto point = text->worldTransform().transformPoint({12, 55});
        expect(scene.pick(point.x, point.y) == text, "text picking handles parent shear and reflection");
        group->setVisible(false);
        expect(!scene.pick(point.x, point.y), "hidden parent hides text");
        group->setVisible(true);
        text->setOpacity(0);
        expect(!scene.pick(point.x, point.y), "transparent text not picked");
        text->setOpacity(1);
        text->setFill(Color::Transparent);
        text->setStroke(Color::White, 8);
        expect(!scene.pick(point.x, point.y), "text stroke alone is not rendered or picked");
        text->setFill(Color::White);
        text->setScale(0, 1);
        expect(!scene.pick(point.x, point.y), "singular text transform skipped");
        text->setScale(1, 1);
        auto overlay = scene.text(text->text(), 3, 4, font, options);
        overlay->setRotation(-0.2f);
        group->addChild(overlay);
        expect(scene.pick(point.x, point.y) == overlay, "text draw order resolves ties");
        text->setZIndex(1);
        expect(scene.pick(point.x, point.y) == text, "text z-index respected");
        expect(uploads == before, "scene layout and picking never upload glyphs");
        std::weak_ptr<Font> resource = font;
        font.reset();
        expect(!resource.expired(), "nodes keep their font alive");
        text->setFont(nullptr);
        overlay->setFont(nullptr);
        expect(resource.expired(), "clearing node fonts releases the shared resource");
        expect(!scene.pick(point.x, point.y), "text without font is not picked");
    }
    expect(textures.empty(), "scene destruction releases font atlases");
    {
        Font font;
        expect(font.load(VECTORGL_TEST_FONT, 16), "bitmap test font loads");
        expectNear(font.emSize(), 16, 0.001f, "font exposes logical em size");
        const auto* sdf = font.getGlyph('A');
        expect(sdf && sdf->sdf, "direct getGlyph remains SDF for existing callers");
        const auto* bitmap = font.getBitmapGlyph('A', 16);
        if (font.hasBitmapSupport())
        {
            expect(bitmap && !bitmap->sdf && bitmap != sdf, "coverage and SDF variants have distinct cache entries");
            expectNear(bitmap->xadvance, sdf->xadvance, 0.001f, "bitmap keeps logical layout advance");
            expect(uploadedCoverage.front() == 0 && uploadedCoverage.back() == 0,
                   "bitmap atlas has transparent filtering guards");
            const auto count = uploads;
            expect(font.getBitmapGlyph('A', 16) == bitmap && uploads == count, "bitmap cache hit does not upload");
            const auto* fractional = font.getBitmapGlyph('A', 16, 1);
            expect(fractional && !fractional->sdf && fractional != bitmap, "quarter-pixel phase has a stable variant");
            expect(font.getBitmapGlyph('A', 24) != bitmap, "framebuffer em sizes have separate coverage");
            expect(font.getBitmapGlyph(0x3A9, 16)->xadvance > bitmap->xadvance,
                   "bitmap Unicode uses actual glyph metrics");
            expect(font.getBitmapGlyph(0x10FFFF, 16) == font.getBitmapGlyph(0xFFFD, 16),
                   "missing bitmap glyph uses the replacement outline");
            expect(font.getBitmapGlyph(' ', 16)->width == 0, "empty bitmap glyph retains zero coverage");
            const auto beforeLayout = uploads;
            expectNear(font.layoutText("AV").width, 17.6f, 0.001f, "raster mode does not change kerning/layout");
            expect(uploads == beforeLayout, "measurement does not rasterize coverage");
            for (int cp = 0xE000; cp < 0xE100; ++cp)
                for (int size : {12, 16, 20, 24})
                    for (int phase = 0; phase < 4; ++phase)
                        expect(font.getBitmapGlyph(cp, size, phase) != nullptr, "cache exhaustion has a fallback");
            expect(font.glyphCacheSize() <= 4096 && font.atlasPageCount() <= 4,
                   "all sizes/phases and SDF share one bounded cache");
            expect(font.getBitmapGlyph('A', 16) == bitmap && !bitmap->sdf,
                   "cache growth preserves coverage pointers/UVs");
            const auto saturatedUploads = uploads;
            for (int i = 0; i < 8; ++i)
                (void)font.getBitmapGlyph(0xE0FF, 24, 3);
            expect(uploads == saturatedUploads, "full bitmap cache does not repeatedly allocate/upload");
        }
        else
            expect(bitmap == sdf, "builds without FreeType fall back to existing SDF");
        expect(font.getBitmapGlyph('A', 0)->sdf && font.getBitmapGlyph('A', 65)->sdf &&
                   font.getBitmapGlyph('A', 16, 4)->sdf,
               "unsupported bitmap parameters fall back safely");
    }
    expect(textures.empty(), "bitmap and SDF pages released exactly once");
    {
        Font font;
        expect(font.load(VECTORGL_TEST_FONT, 16), "layout cache font loads");
        const auto uploadsBefore = uploads;
        const auto first = font.cachedLayoutText("AV");
        expect(first == font.cachedLayoutText("AV"), "cache hits reuse immutable layouts without copying");
        expectNear(first->width, font.layoutText("AV").width, 0.001f, "cached layout preserves kerning");
        TextLayoutOptions options;
        options.maxWidth = 40;
        auto variant = font.cachedLayoutText("AV", options);
        expect(variant != first, "width participates in cache key");
        options.align = TextAlign::Center;
        auto centered = font.cachedLayoutText("AV", options);
        expect(centered != variant && centered->lines.front().x > 0, "alignment participates in cache key");
        options.wrap = TextWrap::Character;
        expect(font.cachedLayoutText("AV", options) != centered, "wrap mode participates in cache key");
        options.lineSpacing = 2;
        auto spaced = font.cachedLayoutText("AV", options);
        expect(font.cachedLayoutText("AV", options) == spaced, "line spacing cache hit");
        for (int i = 0; i < 64; ++i)
            (void)font.cachedLayoutText("label" + std::to_string(i));
        expect(font.layoutCacheStats().entries == 32, "entry limit evicts old layouts");
        const auto recent = font.cachedLayoutText("label32");
        (void)font.cachedLayoutText("new label");
        expect(font.cachedLayoutText("label32") == recent, "hit refreshes LRU recency before eviction");
        expectNear(first->width, 17.6f, 0.001f, "caller-held layout survives eviction");
        expect(font.cachedLayoutText("AV") != first, "evicted layout is recomputed");
        std::weak_ptr<const TextLayout> retained = font.cachedLayoutText("cache-only label");
        expect(!retained.expired(), "cache retains immutable layout ownership");
        font.clearLayoutCache();
        expect(retained.expired(), "clear releases layouts with no external owners");
        expect(font.layoutCacheStats().entries == 0 && font.layoutCacheStats().bytes == 0 &&
                   font.layoutCacheStats().hits == 0,
               "clear releases cache and resets counters");
        for (int i = 0; i < 16; ++i)
            (void)font.cachedLayoutText(std::string(12000, 'A') + std::to_string(i));
        expect(font.layoutCacheStats().bytes <= 2 * 1024 * 1024 && font.layoutCacheStats().entries < 16,
               "byte budget evicts large layouts before entry limit");
        const auto entries = font.layoutCacheStats().entries;
        (void)font.cachedLayoutText(std::string(65537, 'A'));
        expect(font.layoutCacheStats().entries == entries, "oversized text bypasses retention");
        expect(uploads == uploadsBefore, "layout cache never uploads glyphs");
        Font moved = std::move(font);
        expect(moved.layoutCacheStats().entries == entries && font.layoutCacheStats().entries == 0,
               "cache follows font move ownership");
        expect(moved.load(VECTORGL_TEST_FONT, 24), "cached font reload succeeds");
        expect(moved.layoutCacheStats().entries == 0, "font reload invalidates cached metrics");
        expect(moved.cachedLayoutText("AV")->width > first->width, "reloaded font uses new metrics");
        moved.destroy();
        expectNear(first->width, 17.6f, 0.001f, "logical layouts survive font destruction");
    }
    expect(textures.empty(), "layout cache owns no GPU resources");
}
