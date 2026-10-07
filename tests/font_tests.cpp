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
} // namespace

int main()
{
    using namespace vectorgl;
    glad_glGenTextures = genTextures;
    glad_glDeleteTextures = deleteTextures;
    glad_glBindTexture = bindTexture;
    glad_glTexImage2D = texImage2D;
    glad_glTexParameteri = texParameteri;
    {
        Font font;
        expect(!font.load(VECTORGL_TEST_FONT, 1600), "oversized font is rejected before rasterization");
        expect(!font.load(VECTORGL_TEST_FONT, std::numeric_limits<float>::infinity()), "nonfinite size is rejected");
        expect(!font.load(VECTORGL_TEST_FONT, 256), "incomplete atlas is rejected");
        expect(textures.empty(), "failed atlas packing allocates no texture");
        expect(font.load(VECTORGL_TEST_FONT, 16), "complete atlas loads");
        expect(font.getGlyph('?') != nullptr && font.getGlyph(255) != nullptr, "complete glyph set is available");
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
}
