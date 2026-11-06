cmake_minimum_required(VERSION 3.21)
project(vectorgl VERSION 1.0.0 LANGUAGES CXX C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/options.cmake)
add_subdirectory(include/PublicHeaders)
add_subdirectory(include/InternalHeaders)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")

set(VECTORGL_SHADER_FILES
    shaders/sdf.vert
    shaders/sdf.frag
    shaders/path.vert
    shaders/path.frag
    shaders/textured.vert
    shaders/textured.frag
    shaders/blur.vert
    shaders/blur.frag
)

foreach(shader_file IN LISTS VECTORGL_SHADER_FILES)
    get_filename_component(shader_name "${shader_file}" NAME)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/${shader_file}"
        "${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_name}"
        COPYONLY
    )
endforeach()

include(cmake/embed_shaders.cmake)

include(cmake/dependencies.cmake)
include(cmake/tooling.cmake)


add_subdirectory(src)
if(VECTORGL_BUILD_UI)
    add_subdirectory(src/ui)
endif()
include(cmake/install.cmake)

if(VECTORGL_BUILD_TESTS OR VECTORGL_BUILD_APP_TESTS)
    enable_testing()
endif()
if(VECTORGL_BUILD_EXAMPLES OR (VECTORGL_BUILD_UI AND VECTORGL_BUILD_UI_EXAMPLES))
    add_subdirectory(example)
endif()
if(VECTORGL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
if(VECTORGL_BUILD_APPS OR VECTORGL_BUILD_APP_TESTS)
    add_subdirectory(apps/diagram_editor)
endif()
vectorgl_add_format_targets()
include(cmake/summary.cmake)
