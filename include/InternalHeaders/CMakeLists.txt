set(VECTORGL_INTERNAL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" PARENT_SCOPE)
set(VECTORGL_INTERNAL_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/vectorgl/detail/gl_handle.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vectorgl/detail/shader_utils.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vectorgl/detail/svg_parser.hpp"
    PARENT_SCOPE
)
