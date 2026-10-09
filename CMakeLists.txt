cmake_minimum_required(VERSION 3.20)
project(vectorgl VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(VECTORGL_BUILD_EXAMPLES "Build vectorgl example applications" ${PROJECT_IS_TOP_LEVEL})
option(VECTORGL_BUILD_TESTS "Build vectorgl regression tests" ${PROJECT_IS_TOP_LEVEL})
option(VECTORGL_BUILD_GPU_TESTS "Build OpenGL integration tests (requires GLFW and a display)" OFF)
option(VECTORGL_ENABLE_HINTED_TEXT "Use FreeType for hinted small-text bitmap rendering" ON)

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

add_library(vectorgl STATIC
    src/canvas.cpp
    src/path.cpp
    src/font.cpp
    src/text.cpp
    src/image.cpp
    src/text_box.cpp
    src/renderer.cpp
    src/shader_utils.cpp
    ${VECTORGL_EMBEDDED_SHADER_SOURCE}
    src/node.cpp
    src/scene.cpp
    src/animator.cpp
    src/svg.cpp
    src/svg_cache.cpp
    src/svg/xml_parser.cpp
    src/svg/svg_color.cpp
    src/svg/svg_style.cpp
    src/svg/svg_transform.cpp
    src/svg/svg_path.cpp
    src/svg/svg_renderer.cpp
    src/svg/svg_defs.cpp
    src/svg/svg_gradient.cpp
    src/svg/svg_marker.cpp
    src/svg/svg_css.cpp
    "${VECTORGL_GLAD_DIR}/src/gl.c"
    include/PublicHeaders/vectorgl/text_box.hpp
    include/PublicHeaders/vectorgl/glfw_text_box.hpp
    include/PublicHeaders/vectorgl/ui.hpp
)

add_library(vectorgl::vectorgl ALIAS vectorgl)

target_include_directories(vectorgl PUBLIC
    $<BUILD_INTERFACE:${VECTORGL_PUBLIC_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${VECTORGL_GLAD_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_include_directories(vectorgl PRIVATE
    ${VECTORGL_INTERNAL_INCLUDE_DIR}
    ${stb_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/svg
)

target_compile_definitions(vectorgl PRIVATE
    VECTORGL_SHADER_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders"
    VECTORGL_INSTALL_SHADER_DIR="${CMAKE_INSTALL_FULL_DATADIR}/vectorgl/shaders"
)

target_compile_features(vectorgl PUBLIC cxx_std_20)
target_link_libraries(vectorgl PRIVATE ${CMAKE_DL_LIBS})
if(VECTORGL_ENABLE_HINTED_TEXT)
    target_link_libraries(vectorgl PRIVATE freetype)
    target_compile_definitions(vectorgl PRIVATE VECTORGL_HAS_FREETYPE=1)
endif()

setup_clang_tooling(vectorgl)

install(TARGETS vectorgl
    EXPORT vectorglTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${VECTORGL_PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/vectorgl
)

install(FILES ${VECTORGL_SHADER_FILES}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/vectorgl/shaders
)

install(EXPORT vectorglTargets
    FILE vectorglTargets.cmake
    NAMESPACE vectorgl::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vectorgl
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/vectorglConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/vectorglConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vectorgl
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/vectorglConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/vectorglConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/vectorglConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vectorgl
)

if(VECTORGL_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

if(VECTORGL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
