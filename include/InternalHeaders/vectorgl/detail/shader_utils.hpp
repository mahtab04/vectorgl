#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace vectorgl::detail
{

std::string loadShaderSource(const char* shaderDirectory, const char* fileName);
std::string embeddedShaderSource(std::string_view fileName);
uint32_t compileShaderFromSource(uint32_t type, const std::string& source, const char* label);
uint32_t linkShaderProgram(uint32_t vert, uint32_t frag, const char* label);
uint32_t buildShaderProgramFromSource(const std::string& vertexSource, const std::string& fragmentSource,
                                      const char* vertexLabel, const char* fragmentLabel, const char* programLabel);

} // namespace vectorgl::detail
