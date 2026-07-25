add_executable(vectorgl_example main.cpp)
target_link_libraries(vectorgl_example PRIVATE vectorgl glfw)


add_executable(vectorgl_demo demo_all.cpp)
target_link_libraries(vectorgl_demo PRIVATE vectorgl glfw)

add_executable(vectorgl_showcase showcase.cpp)
target_link_libraries(vectorgl_showcase PRIVATE vectorgl glfw)


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
