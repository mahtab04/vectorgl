function(vectorgl_add_test TEST_NAME)
    add_executable(${TEST_NAME} ${ARGN})
    target_link_libraries(${TEST_NAME} PRIVATE vectorgl::vectorgl)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

vectorgl_add_test(vectorgl_core_tests core_tests.cpp)
vectorgl_add_test(vectorgl_color_tests color_tests.cpp)
target_include_directories(vectorgl_color_tests PRIVATE ${VECTORGL_INTERNAL_INCLUDE_DIR})
if(VECTORGL_BUILD_UI)
    add_executable(vectorgl_ui_list_tests ui/list_tests.cpp)
    target_link_libraries(vectorgl_ui_list_tests PRIVATE vectorgl::ui)
    add_test(NAME vectorgl_ui_list_tests COMMAND vectorgl_ui_list_tests)
    set_tests_properties(vectorgl_ui_list_tests PROPERTIES TIMEOUT 15)
    add_executable(vectorgl_ui_workspace_control_tests ui/workspace_control_tests.cpp)
    target_link_libraries(vectorgl_ui_workspace_control_tests PRIVATE vectorgl::ui)
    add_test(NAME vectorgl_ui_workspace_control_tests COMMAND vectorgl_ui_workspace_control_tests)
    set_tests_properties(vectorgl_ui_workspace_control_tests PROPERTIES TIMEOUT 15)
    add_executable(vectorgl_ui_overlay_tests ui/overlay_tests.cpp)
    target_link_libraries(vectorgl_ui_overlay_tests PRIVATE vectorgl::ui)
    add_test(NAME vectorgl_ui_overlay_tests COMMAND vectorgl_ui_overlay_tests)
    set_tests_properties(vectorgl_ui_overlay_tests PROPERTIES TIMEOUT 15)
    add_executable(vectorgl_ui_control_tests ui/control_tests.cpp)
    target_link_libraries(vectorgl_ui_control_tests PRIVATE vectorgl::ui)
    add_test(NAME vectorgl_ui_control_tests COMMAND vectorgl_ui_control_tests)
    set_tests_properties(vectorgl_ui_control_tests PROPERTIES TIMEOUT 15)
    add_executable(vectorgl_ui_container_tests ui/container_tests.cpp)
    target_link_libraries(vectorgl_ui_container_tests PRIVATE vectorgl::ui)
    add_test(NAME vectorgl_ui_container_tests COMMAND vectorgl_ui_container_tests)
    set_tests_properties(vectorgl_ui_container_tests PROPERTIES TIMEOUT 15)
    add_executable(vectorgl_ui_layout_tests ui/layout_tests.cpp)
    target_link_libraries(vectorgl_ui_layout_tests PRIVATE vectorgl::ui)
    add_test(NAME vectorgl_ui_layout_tests COMMAND vectorgl_ui_layout_tests)
    set_tests_properties(vectorgl_ui_layout_tests PROPERTIES TIMEOUT 15)
    add_executable(vectorgl_ui_tests ui/context_tests.cpp)
    target_link_libraries(vectorgl_ui_tests PRIVATE vectorgl::ui)
    add_test(NAME vectorgl_ui_tests COMMAND vectorgl_ui_tests)
    set_tests_properties(vectorgl_ui_tests PROPERTIES TIMEOUT 15)
    if(TARGET vectorgl_ui_routing)
        add_test(NAME vectorgl_ui_routing_smoke COMMAND vectorgl_ui_routing)
        set_tests_properties(vectorgl_ui_routing_smoke PROPERTIES TIMEOUT 15)
    endif()
endif()
vectorgl_add_test(vectorgl_path_tests path_tests.cpp)
vectorgl_add_test(vectorgl_fill_rule_tests fill_rule_tests.cpp)
target_include_directories(vectorgl_fill_rule_tests PRIVATE ${VECTORGL_INTERNAL_INCLUDE_DIR})
vectorgl_add_test(vectorgl_stroke_tests stroke_tests.cpp)
target_include_directories(vectorgl_stroke_tests PRIVATE ${VECTORGL_INTERNAL_INCLUDE_DIR})
vectorgl_add_test(vectorgl_dash_tests dash_tests.cpp)
target_include_directories(vectorgl_dash_tests PRIVATE ${VECTORGL_INTERNAL_INCLUDE_DIR})
set_tests_properties(vectorgl_dash_tests PROPERTIES TIMEOUT 15)
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
    if(TARGET vectorgl_palette)
        set(palette_runner)
        if(UNIX AND NOT APPLE)
            find_program(XVFB_RUN_EXECUTABLE NAMES xvfb-run)
            if(XVFB_RUN_EXECUTABLE)
                set(palette_runner ${XVFB_RUN_EXECUTABLE} -a)
            endif()
        endif()
        add_test(NAME vectorgl_palette_smoke COMMAND ${palette_runner} $<TARGET_FILE:vectorgl_palette>
            --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
        set_tests_properties(vectorgl_palette_smoke PROPERTIES TIMEOUT 20)
    endif()
    if(VECTORGL_BUILD_UI AND VECTORGL_BUILD_GLFW_ADAPTER)
        if(TARGET vectorgl_ui_list)
            set(list_runner)
            if(UNIX AND NOT APPLE)
                find_program(XVFB_RUN_EXECUTABLE NAMES xvfb-run)
                if(XVFB_RUN_EXECUTABLE)
                    set(list_runner ${XVFB_RUN_EXECUTABLE} -a)
                endif()
            endif()
            add_test(NAME vectorgl_ui_list_smoke COMMAND ${list_runner} $<TARGET_FILE:vectorgl_ui_list>
                --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
            set_tests_properties(vectorgl_ui_list_smoke PROPERTIES TIMEOUT 20)
        endif()
        add_executable(vectorgl_ui_paint_tests ui/painting_tests.cpp)
        target_link_libraries(vectorgl_ui_paint_tests PRIVATE vectorgl::glfw)
        target_compile_definitions(vectorgl_ui_paint_tests PRIVATE
            VECTORGL_TEST_FONT="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf")
        if(UNIX AND NOT APPLE)
            find_program(XVFB_RUN_EXECUTABLE NAMES xvfb-run)
        endif()
        if(XVFB_RUN_EXECUTABLE)
            add_test(NAME vectorgl_ui_paint_tests COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:vectorgl_ui_paint_tests>)
            if(TARGET vectorgl_ui_host)
                add_test(NAME vectorgl_ui_host_smoke COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:vectorgl_ui_host>
                    --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
            endif()
        else()
            add_test(NAME vectorgl_ui_paint_tests COMMAND vectorgl_ui_paint_tests)
            if(TARGET vectorgl_ui_host)
                add_test(NAME vectorgl_ui_host_smoke COMMAND vectorgl_ui_host
                    --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
            endif()
        endif()
        set_tests_properties(vectorgl_ui_paint_tests PROPERTIES TIMEOUT 20)
        if(TARGET vectorgl_ui_controls)
            set(controls_runner)
            if(XVFB_RUN_EXECUTABLE)
                set(controls_runner ${XVFB_RUN_EXECUTABLE} -a)
            endif()
            add_test(NAME vectorgl_ui_controls_smoke COMMAND ${controls_runner}
                $<TARGET_FILE:vectorgl_ui_controls> --smoke-test
                --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
            add_test(NAME vectorgl_ui_controls_narrow_smoke COMMAND ${controls_runner}
                $<TARGET_FILE:vectorgl_ui_controls> --smoke-test --width 800 --height 1150
                --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
            set_tests_properties(vectorgl_ui_controls_smoke vectorgl_ui_controls_narrow_smoke PROPERTIES TIMEOUT 20)
            foreach(overlay IN ITEMS dialog menu)
                add_test(NAME vectorgl_ui_controls_${overlay}_smoke COMMAND ${controls_runner}
                    $<TARGET_FILE:vectorgl_ui_controls> --smoke-test --show-${overlay}
                    --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
                set_tests_properties(vectorgl_ui_controls_${overlay}_smoke PROPERTIES TIMEOUT 20)
            endforeach()
        endif()
        if(TARGET vectorgl_ui_workspace)
            set(workspace_runner)
            if(XVFB_RUN_EXECUTABLE)
                set(workspace_runner ${XVFB_RUN_EXECUTABLE} -a)
            endif()
            add_test(NAME vectorgl_ui_workspace_metadata_smoke COMMAND ${workspace_runner}
                $<TARGET_FILE:vectorgl_ui_workspace> --smoke-test --metadata
                --width 1280 --height 780 --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
            set_tests_properties(vectorgl_ui_workspace_metadata_smoke PROPERTIES TIMEOUT 20)
            foreach(size IN ITEMS wide compact)
                if(size STREQUAL "wide")
                    set(workspace_width 1680)
                    set(workspace_height 980)
                else()
                    set(workspace_width 1280)
                    set(workspace_height 780)
                endif()
                add_test(NAME vectorgl_ui_workspace_${size}_smoke
                    COMMAND ${workspace_runner}
                        $<TARGET_FILE:vectorgl_ui_workspace> --smoke-test
                        --width ${workspace_width} --height ${workspace_height}
                        --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
                set_tests_properties(vectorgl_ui_workspace_${size}_smoke PROPERTIES TIMEOUT 20)
            endforeach()
        endif()
        if(TARGET vectorgl_ui_layout)
            if(XVFB_RUN_EXECUTABLE)
                add_test(NAME vectorgl_ui_layout_smoke COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:vectorgl_ui_layout>
                    --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
                add_test(NAME vectorgl_ui_layout_narrow_smoke COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:vectorgl_ui_layout>
                    --smoke-test --width 760 --height 980 --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
            else()
                add_test(NAME vectorgl_ui_layout_smoke COMMAND vectorgl_ui_layout
                    --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
                add_test(NAME vectorgl_ui_layout_narrow_smoke COMMAND vectorgl_ui_layout
                    --smoke-test --width 760 --height 980 --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
            endif()
            set_tests_properties(vectorgl_ui_layout_smoke vectorgl_ui_layout_narrow_smoke PROPERTIES TIMEOUT 20)
        endif()
        if(TARGET vectorgl_ui_host)
            set_tests_properties(vectorgl_ui_host_smoke PROPERTIES TIMEOUT 20)
        endif()
    endif()
    if(TARGET vectorgl_dashes)
        if(UNIX AND NOT APPLE)
            find_program(XVFB_RUN_EXECUTABLE NAMES xvfb-run)
        endif()
        if(XVFB_RUN_EXECUTABLE)
            add_test(NAME vectorgl_dashes_demo_smoke
                COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:vectorgl_dashes>
                    --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
        else()
            add_test(NAME vectorgl_dashes_demo_smoke COMMAND vectorgl_dashes
                --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
        endif()
        set_tests_properties(vectorgl_dashes_demo_smoke PROPERTIES TIMEOUT 15)
    endif()
    if(TARGET vectorgl_strokes)
        if(UNIX AND NOT APPLE)
            find_program(XVFB_RUN_EXECUTABLE NAMES xvfb-run)
        endif()
        if(XVFB_RUN_EXECUTABLE)
            add_test(NAME vectorgl_strokes_demo_smoke
                COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:vectorgl_strokes>
                    --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
        else()
            add_test(NAME vectorgl_strokes_demo_smoke COMMAND vectorgl_strokes
                --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
        endif()
        set_tests_properties(vectorgl_strokes_demo_smoke PROPERTIES TIMEOUT 15)
    endif()
    if(TARGET vectorgl_fill_rules)
        if(UNIX AND NOT APPLE)
            find_program(XVFB_RUN_EXECUTABLE NAMES xvfb-run)
        endif()
        if(XVFB_RUN_EXECUTABLE)
            add_test(NAME vectorgl_fill_rules_demo_smoke
                COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:vectorgl_fill_rules>
                    --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
        else()
            add_test(NAME vectorgl_fill_rules_demo_smoke COMMAND vectorgl_fill_rules
                --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
        endif()
        set_tests_properties(vectorgl_fill_rules_demo_smoke PROPERTIES TIMEOUT 15)
    endif()
    if(TARGET vectorgl_large_scene)
        if(UNIX AND NOT APPLE)
            find_program(XVFB_RUN_EXECUTABLE NAMES xvfb-run)
        endif()
        if(XVFB_RUN_EXECUTABLE)
            add_test(NAME vectorgl_large_scene_demo_smoke
                COMMAND ${XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:vectorgl_large_scene>
                    --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
        else()
            add_test(NAME vectorgl_large_scene_demo_smoke COMMAND vectorgl_large_scene
                --smoke-test --font ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/box.ttf)
        endif()
        set_tests_properties(vectorgl_large_scene_demo_smoke PROPERTIES TIMEOUT 30)
    endif()
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
