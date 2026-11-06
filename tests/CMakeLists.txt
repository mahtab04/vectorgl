function(vectorgl_add_test TEST_NAME)
    add_executable(${TEST_NAME} ${ARGN})
    target_link_libraries(${TEST_NAME} PRIVATE vectorgl::vectorgl)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

vectorgl_add_test(vectorgl_core_tests core_tests.cpp)
vectorgl_add_test(vectorgl_color_tests color_tests.cpp)
add_test(NAME vectorgl_cmake_policy_tests COMMAND ${CMAKE_COMMAND}
    "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}" "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/policies"
    "-DGENERATOR=${CMAKE_GENERATOR}" "-DSTB_DIR=${stb_SOURCE_DIR}" "-DTESS_DIR=${VECTORGL_LIBTESS2_DIR}"
    "-DC_COMPILER=${CMAKE_C_COMPILER}" "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/policy-tests.cmake)
set_tests_properties(vectorgl_cmake_policy_tests PROPERTIES TIMEOUT 90)
target_include_directories(vectorgl_color_tests PRIVATE ${VECTORGL_INTERNAL_INCLUDE_DIR})
if(VECTORGL_BUILD_UI)
    add_subdirectory(ui)
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

include(${PROJECT_SOURCE_DIR}/cmake/testing.cmake)
include(gpu.cmake)
vectorgl_label_tests(${CMAKE_CURRENT_SOURCE_DIR})
