#include "vectorgl/detail/shader_utils.hpp"

#include <glad/gl.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace vectorgl::detail
{

namespace
{

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open shader file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string shaderTypeName(uint32_t type)
{
    switch (type)
    {
    case GL_VERTEX_SHADER:
        return "vertex";
    case GL_FRAGMENT_SHADER:
        return "fragment";
    default:
        return "unknown";
    }
}

std::string getShaderInfoLog(uint32_t shader)
{
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1)
    {
        return {};
    }

    std::vector<char> buffer(static_cast<size_t>(length), '\0');
    glGetShaderInfoLog(shader, length, nullptr, buffer.data());
    return std::string(buffer.data());
}

std::string getProgramInfoLog(uint32_t program)
{
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1)
    {
        return {};
    }

    std::vector<char> buffer(static_cast<size_t>(length), '\0');
    glGetProgramInfoLog(program, length, nullptr, buffer.data());
    return std::string(buffer.data());
}

} // namespace

std::string loadShaderSource(const char* shaderDirectory, const char* fileName)
{
    const std::filesystem::path shaderPath = std::filesystem::path(shaderDirectory) / fileName;
    if (std::filesystem::is_regular_file(shaderPath))
        return readTextFile(shaderPath);

#ifdef VECTORGL_INSTALL_SHADER_DIR
    const std::filesystem::path installedPath = std::filesystem::path(VECTORGL_INSTALL_SHADER_DIR) / fileName;
    if (std::filesystem::is_regular_file(installedPath))
        return readTextFile(installedPath);
#endif

    if (auto embedded = embeddedShaderSource(fileName); !embedded.empty())
        return embedded;

    throw std::runtime_error("Failed to locate shader file '" + std::string(fileName) +
                             "' in the build, installed, or embedded shader sources");
}

uint32_t compileShaderFromSource(uint32_t type, const std::string& source, const char* label)
{
    const char* sourcePtr = source.c_str();
    uint32_t shader = glCreateShader(type);
    glShaderSource(shader, 1, &sourcePtr, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE)
    {
        return shader;
    }

    const std::string infoLog = getShaderInfoLog(shader);
    glDeleteShader(shader);
    throw std::runtime_error("Failed to compile " + shaderTypeName(type) + " shader '" + label + "'" +
                             (infoLog.empty() ? std::string() : "\n" + infoLog));
}

uint32_t linkShaderProgram(uint32_t vert, uint32_t frag, const char* label)
{
    uint32_t program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    glDeleteShader(vert);
    glDeleteShader(frag);

    if (status == GL_TRUE)
    {
        return program;
    }

    const std::string infoLog = getProgramInfoLog(program);
    glDeleteProgram(program);
    throw std::runtime_error("Failed to link shader program '" + std::string(label) + "'" +
                             (infoLog.empty() ? std::string() : "\n" + infoLog));
}

} // namespace vectorgl::detail
