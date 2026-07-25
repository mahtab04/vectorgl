function(vectorgl_add_test TEST_NAME)
    add_executable(${TEST_NAME} ${ARGN})
    target_link_libraries(${TEST_NAME} PRIVATE vectorgl::vectorgl)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

vectorgl_add_test(vectorgl_core_tests core_tests.cpp)
vectorgl_add_test(vectorgl_path_tests path_tests.cpp)
vectorgl_add_test(vectorgl_scene_tests scene_tests.cpp)
vectorgl_add_test(vectorgl_animator_tests animator_tests.cpp)

if(VECTORGL_BUILD_GPU_TESTS)
    if(NOT TARGET glfw)
        message(FATAL_ERROR "VECTORGL_BUILD_GPU_TESTS requires the GLFW target")
    endif()

    add_executable(vectorgl_gpu_tests gpu_tests.cpp)
    target_link_libraries(vectorgl_gpu_tests PRIVATE vectorgl::vectorgl glfw)

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
endif()
