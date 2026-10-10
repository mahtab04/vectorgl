add_executable(vectorgl_example main.cpp)

add_executable(vectorgl_large_scene large_scene_demo.cpp)
target_link_libraries(vectorgl_large_scene PRIVATE vectorgl glfw)

add_executable(vectorgl_hit_testing hit_testing_demo.cpp)
target_link_libraries(vectorgl_hit_testing PRIVATE vectorgl glfw)

add_executable(vectorgl_text_layout text_layout_demo.cpp)
target_link_libraries(vectorgl_text_layout PRIVATE vectorgl glfw)

add_executable(vectorgl_text_raster text_raster_demo.cpp)
target_link_libraries(vectorgl_text_raster PRIVATE vectorgl glfw)

add_executable(vectorgl_scene_content scene_content_demo.cpp)
target_link_libraries(vectorgl_scene_content PRIVATE vectorgl glfw)
target_compile_definitions(vectorgl_scene_content PRIVATE
    VECTORGL_DEMO_IMAGE="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/quad.tga"
)
target_link_libraries(vectorgl_example PRIVATE vectorgl glfw)


add_executable(vectorgl_demo demo_all.cpp)
target_link_libraries(vectorgl_demo PRIVATE vectorgl glfw)

add_executable(vectorgl_showcase showcase.cpp)
target_link_libraries(vectorgl_showcase PRIVATE vectorgl glfw)

add_executable(vectorgl_clipping clipping_demo.cpp)
target_link_libraries(vectorgl_clipping PRIVATE vectorgl glfw)


add_executable(vectorgl_svg svg_demo.cpp)
target_link_libraries(vectorgl_svg PRIVATE vectorgl glfw)


add_executable(vectorgl_paths paths_demo.cpp)
target_link_libraries(vectorgl_paths PRIVATE vectorgl glfw)


add_executable(vectorgl_animation animation_demo.cpp)
target_link_libraries(vectorgl_animation PRIVATE vectorgl glfw)


add_executable(vectorgl_stress stress_test.cpp)
target_link_libraries(vectorgl_stress PRIVATE vectorgl glfw)


add_executable(vectorgl_font_image font_image_demo.cpp)
target_link_libraries(vectorgl_font_image PRIVATE vectorgl glfw)


add_executable(vectorgl_text_input text_input_demo.cpp)
target_link_libraries(vectorgl_text_input PRIVATE vectorgl glfw)


add_subdirectory(node_editor)
