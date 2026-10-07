#include <glad/gl.h>

#include <array>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "test_utils.hpp"
#include "vectorgl/detail/shader_utils.hpp"

namespace
{
GLuint nextId = 0;
GLenum failedStage = 0;
bool failLink = false;
std::unordered_set<GLuint> shaders;
std::unordered_set<GLuint> programs;
std::unordered_set<GLuint> failedShaders;
std::unordered_map<GLuint, std::unordered_set<GLuint>> attachments;

GLuint GLAD_API_PTR createShader(GLenum stage)
{
    GLuint id = ++nextId;
    shaders.insert(id);
    if (stage == failedStage)
        failedShaders.insert(id);
    return id;
}
void GLAD_API_PTR shaderSource(GLuint, GLsizei, const GLchar* const*, const GLint*) {}
void GLAD_API_PTR compileShader(GLuint) {}
void GLAD_API_PTR getShaderiv(GLuint id, GLenum property, GLint* value)
{
    *value = property == GL_COMPILE_STATUS && !failedShaders.contains(id) ? GL_TRUE : GL_FALSE;
}
void GLAD_API_PTR deleteShader(GLuint id)
{
    expect(shaders.erase(id) == 1, "shader deleted exactly once");
    failedShaders.erase(id);
}
GLuint GLAD_API_PTR createProgram()
{
    GLuint id = ++nextId;
    programs.insert(id);
    return id;
}
void GLAD_API_PTR attachShader(GLuint program, GLuint shader)
{
    expect(attachments[program].insert(shader).second, "shader attached exactly once");
}
void GLAD_API_PTR detachShader(GLuint program, GLuint shader)
{
    expect(attachments[program].erase(shader) == 1, "shader detached exactly once");
}
void GLAD_API_PTR linkProgram(GLuint) {}
void GLAD_API_PTR getProgramiv(GLuint, GLenum property, GLint* value)
{
    *value = property == GL_LINK_STATUS && !failLink ? GL_TRUE : GL_FALSE;
}
void GLAD_API_PTR deleteProgram(GLuint id)
{
    expect(programs.erase(id) == 1, "program deleted exactly once");
    expect(attachments[id].empty(), "program no longer retains shaders");
    attachments.erase(id);
}
} // namespace

int main()
{
    glad_glCreateShader = createShader;
    glad_glShaderSource = shaderSource;
    glad_glCompileShader = compileShader;
    glad_glGetShaderiv = getShaderiv;
    glad_glDeleteShader = deleteShader;
    glad_glCreateProgram = createProgram;
    glad_glAttachShader = attachShader;
    glad_glDetachShader = detachShader;
    glad_glLinkProgram = linkProgram;
    glad_glGetProgramiv = getProgramiv;
    glad_glDeleteProgram = deleteProgram;

    for (GLenum stage : std::array<GLenum, 3>{GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, 0})
    {
        failedStage = stage;
        failLink = stage == 0;
        bool threw = false;
        try
        {
            vectorgl::detail::buildShaderProgramFromSource("vertex", "fragment", "vertex", "fragment", "test");
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        expect(threw, "shader build failure is reported");
        expect(shaders.empty(), "failed build releases all shaders");
        expect(programs.empty(), "failed build releases all programs");
    }

    failedStage = 0;
    failLink = false;
    GLuint program = vectorgl::detail::buildShaderProgramFromSource("vertex", "fragment", "vertex", "fragment", "test");
    expect(shaders.empty(), "successful build releases shader objects");
    expect(programs.size() == 1 && programs.contains(program), "successful program ownership transfers to caller");
    glDeleteProgram(program);
    expect(programs.empty(), "caller releases successful program");
}
