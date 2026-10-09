function(vectorgl_add_test TEST_NAME)
    add_executable(${TEST_NAME} ${ARGN})
    target_link_libraries(${TEST_NAME} PRIVATE vectorgl::vectorgl)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

vectorgl_add_test(vectorgl_core_tests core_tests.cpp)
vectorgl_add_test(vectorgl_path_tests path_tests.cpp)
vectorgl_add_test(vectorgl_scene_tests scene_tests.cpp)
vectorgl_add_test(vectorgl_picking_tests picking_tests.cpp)
vectorgl_add_test(vectorgl_animator_tests animator_tests.cpp)
vectorgl_add_test(vectorgl_shader_tests shader_tests.cpp)
vectorgl_add_test(vectorgl_svg_tests svg_tests.cpp)
vectorgl_add_test(vectorgl_font_tests font_tests.cpp)
vectorgl_add_test(vectorgl_text_box_tests text_box_tests.cpp)
target_include_directories(vectorgl_text_box_tests PRIVATE ${VECTORGL_INTERNAL_INCLUDE_DIR})
target_compile_definitions(vectorgl_font_tests PRIVATE
    VECTORGL_TEST_FONT="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf"
)
set_tests_properties(vectorgl_svg_tests vectorgl_animator_tests PROPERTIES TIMEOUT 10)
target_include_directories(vectorgl_shader_tests PRIVATE ${VECTORGL_INTERNAL_INCLUDE_DIR})

if(WIN32)
    find_program(VECTORGL_POWERSHELL NAMES powershell pwsh)
    if(VECTORGL_POWERSHELL)
        add_test(NAME vectorgl_build_helper_tests
            COMMAND ${VECTORGL_POWERSHELL} -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/build_helper_tests.ps1
        )
    endif()
endif()

if(VECTORGL_BUILD_GPU_TESTS)
    if(NOT TARGET glfw)
        message(FATAL_ERROR "VECTORGL_BUILD_GPU_TESTS requires the GLFW target")
    endif()

    add_executable(vectorgl_gpu_tests gpu_tests.cpp)
    target_link_libraries(vectorgl_gpu_tests PRIVATE vectorgl::vectorgl glfw)
    target_compile_definitions(vectorgl_gpu_tests PRIVATE
        VECTORGL_TEST_FONT="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf"
        VECTORGL_TEST_IMAGE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/quad.tga"
    )

    if(UNIX AND NOT APPLE)
        find_program(XVFB_RUN_EXECUTABLE NAMES xvfb-run)
    endif()

    if(XVFB_RUN_EXECUTABLE)
        add_test(NAME vectorgl_gpu_tests
            COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:vectorgl_gpu_tests>
        )
    else()
        add_test(NAME vectorgl_gpu_tests COMMAND vectorgl_gpu_tests)
    endif()

    if(TARGET vectorgl_hit_testing)
        if(XVFB_RUN_EXECUTABLE)
            add_test(NAME vectorgl_hit_testing_demo_smoke
                COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:vectorgl_hit_testing> --smoke-test
            )
        else()
            add_test(NAME vectorgl_hit_testing_demo_smoke
                COMMAND vectorgl_hit_testing --smoke-test
            )
        endif()
        set_tests_properties(vectorgl_hit_testing_demo_smoke PROPERTIES TIMEOUT 15)
    endif()
    if(TARGET vectorgl_scene_content)
        if(XVFB_RUN_EXECUTABLE)
            add_test(NAME vectorgl_scene_content_demo_smoke
                COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:vectorgl_scene_content>
                    --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf
            )
        else()
            add_test(NAME vectorgl_scene_content_demo_smoke COMMAND vectorgl_scene_content
                --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf
            )
        endif()
        set_tests_properties(vectorgl_scene_content_demo_smoke PROPERTIES TIMEOUT 15)
    endif()
    if(TARGET vectorgl_text_layout)
        if(TARGET vectorgl_text_raster)
            if(XVFB_RUN_EXECUTABLE)
                add_test(NAME vectorgl_text_raster_demo_smoke
                    COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:vectorgl_text_raster>
                        --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf
                )
            else()
                add_test(NAME vectorgl_text_raster_demo_smoke COMMAND vectorgl_text_raster
                    --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf
                )
            endif()
            set_tests_properties(vectorgl_text_raster_demo_smoke PROPERTIES TIMEOUT 15)
        endif()
        if(XVFB_RUN_EXECUTABLE)
            add_test(NAME vectorgl_text_layout_demo_smoke
                COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:vectorgl_text_layout>
                    --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf
            )
        else()
            add_test(NAME vectorgl_text_layout_demo_smoke COMMAND vectorgl_text_layout
                --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf
            )
        endif()
        set_tests_properties(vectorgl_text_layout_demo_smoke PROPERTIES TIMEOUT 15)
    endif()
endif()
