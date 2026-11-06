set(VECTORGL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${VECTORGL_GENERATED_DIR}")
set(VECTORGL_EMBEDDED_SHADER_SOURCE "${VECTORGL_GENERATED_DIR}/embedded_shaders.cpp")

string(CONCAT shader_content
"#include <string>\n#include <string_view>\n\nnamespace vectorgl::detail\n{\n"
"std::string embeddedShaderSource(std::string_view fileName)\n{\n")

foreach(shader_file IN LISTS VECTORGL_SHADER_FILES)
    get_filename_component(shader_name "${shader_file}" NAME)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${shader_file}" shader_source)
    string(APPEND shader_content
        "    if (fileName == \"${shader_name}\")\n"
        "        return R\"VECTORGL_SHADER(${shader_source})VECTORGL_SHADER\";\n")
endforeach()

string(APPEND shader_content
"    return {};\n}\n\n} // namespace vectorgl::detail\n")

# Avoid recompiling the embedded shader unit after an unrelated reconfigure.
file(CONFIGURE OUTPUT "${VECTORGL_EMBEDDED_SHADER_SOURCE}" CONTENT "${shader_content}" @ONLY)
