add_library(vectorgl STATIC
    canvas.cpp
    path.cpp
    path_tessellation.cpp
    stroke_tessellation.cpp
    ${VECTORGL_LIBTESS2_DIR}/Source/bucketalloc.c
    ${VECTORGL_LIBTESS2_DIR}/Source/dict.c
    ${VECTORGL_LIBTESS2_DIR}/Source/geom.c
    ${VECTORGL_LIBTESS2_DIR}/Source/mesh.c
    ${VECTORGL_LIBTESS2_DIR}/Source/priorityq.c
    ${VECTORGL_LIBTESS2_DIR}/Source/sweep.c
    ${VECTORGL_LIBTESS2_DIR}/Source/tess.c
    font.cpp
    text.cpp
    image.cpp
    text_box.cpp
    renderer.cpp
    shader_utils.cpp
    ${VECTORGL_EMBEDDED_SHADER_SOURCE}
    node.cpp
    scene.cpp
    animator.cpp
    svg.cpp
    svg_cache.cpp
    svg/xml_parser.cpp
    svg/svg_color.cpp
    svg/svg_style.cpp
    svg/svg_transform.cpp
    svg/svg_path.cpp
    svg/svg_renderer.cpp
    svg/svg_defs.cpp
    svg/svg_gradient.cpp
    svg/svg_marker.cpp
    svg/svg_css.cpp
    "${VECTORGL_GLAD_DIR}/src/gl.c"
    ${PROJECT_SOURCE_DIR}/include/PublicHeaders/vectorgl/text_box.hpp
    ${PROJECT_SOURCE_DIR}/include/PublicHeaders/vectorgl/glfw_text_box.hpp
    ${PROJECT_SOURCE_DIR}/include/PublicHeaders/vectorgl/ui.hpp
)

add_library(vectorgl::vectorgl ALIAS vectorgl)

target_include_directories(vectorgl PUBLIC
    $<BUILD_INTERFACE:${VECTORGL_PUBLIC_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${VECTORGL_GLAD_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_include_directories(vectorgl PRIVATE
    ${VECTORGL_LIBTESS2_DIR}/Include
    ${VECTORGL_INTERNAL_INCLUDE_DIR}
    ${stb_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/svg
)

target_compile_definitions(vectorgl PRIVATE
    VECTORGL_SHADER_DIR="${PROJECT_BINARY_DIR}/shaders"
    VECTORGL_INSTALL_SHADER_DIR="${CMAKE_INSTALL_FULL_DATADIR}/vectorgl/shaders"
)

target_compile_features(vectorgl PUBLIC cxx_std_20)
target_link_libraries(vectorgl PRIVATE ${CMAKE_DL_LIBS})
if(VECTORGL_ENABLE_HINTED_TEXT)
    target_link_libraries(vectorgl PRIVATE ${VECTORGL_FREETYPE_TARGET})
    target_compile_definitions(vectorgl PRIVATE VECTORGL_HAS_FREETYPE=1)
endif()

setup_clang_tooling(vectorgl)


set_target_properties(vectorgl PROPERTIES CXX_EXTENSIONS OFF)
