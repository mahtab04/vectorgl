# UI examples can be built independently of the full GLFW example collection.
if(VECTORGL_BUILD_UI AND VECTORGL_BUILD_UI_EXAMPLES)
    add_executable(vectorgl_ui_routing ui_routing_demo.cpp)
    target_link_libraries(vectorgl_ui_routing PRIVATE vectorgl::ui)
    if(TARGET vectorgl_glfw)
        add_executable(vectorgl_ui_list ui_list_demo.cpp)
        target_link_libraries(vectorgl_ui_list PRIVATE vectorgl::glfw)
        add_executable(vectorgl_ui_host ui_host_demo.cpp)
        target_link_libraries(vectorgl_ui_host PRIVATE vectorgl::glfw)
        add_executable(vectorgl_ui_layout ui_layout_demo.cpp)
        target_link_libraries(vectorgl_ui_layout PRIVATE vectorgl::glfw)
        add_executable(vectorgl_ui_workspace ui_workspace_demo.cpp)
        target_link_libraries(vectorgl_ui_workspace PRIVATE vectorgl::glfw)
        add_executable(vectorgl_ui_controls ui_controls_demo.cpp)
        target_link_libraries(vectorgl_ui_controls PRIVATE vectorgl::glfw)
    endif()
endif()

if(NOT VECTORGL_BUILD_EXAMPLES)
    return()
endif()

add_executable(vectorgl_example main.cpp)
add_executable(vectorgl_palette palette_demo.cpp)
target_link_libraries(vectorgl_palette PRIVATE vectorgl glfw)

add_executable(vectorgl_fill_rules fill_rules_demo.cpp)
target_link_libraries(vectorgl_fill_rules PRIVATE vectorgl glfw)

add_executable(vectorgl_strokes strokes_demo.cpp)
target_link_libraries(vectorgl_strokes PRIVATE vectorgl glfw)

add_executable(vectorgl_dashes dashes_demo.cpp)
target_link_libraries(vectorgl_dashes PRIVATE vectorgl glfw)

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
