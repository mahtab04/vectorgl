include_guard(GLOBAL)

# Quiet discovery; tools are required only for explicitly requested checks.
if(PROJECT_IS_TOP_LEVEL OR VECTORGL_ENABLE_CLANG_TIDY OR VECTORGL_ENABLE_FORMAT_CHECK)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy-18 clang-tidy)
    find_program(CLANG_FORMAT_EXE NAMES clang-format-18 clang-format HINTS "C:/Program Files/LLVM/bin")
endif()
if(VECTORGL_ENABLE_CLANG_TIDY AND NOT CLANG_TIDY_EXE)
    message(FATAL_ERROR "clang-tidy requested but not found; set CLANG_TIDY_EXE or disable VECTORGL_ENABLE_CLANG_TIDY")
endif()
if(VECTORGL_ENABLE_FORMAT_CHECK AND NOT CLANG_FORMAT_EXE)
    message(FATAL_ERROR "clang-format requested but not found; set CLANG_FORMAT_EXE or disable VECTORGL_ENABLE_FORMAT_CHECK")
endif()
if(VECTORGL_ENABLE_FORMAT_CHECK)
    execute_process(COMMAND "${CLANG_FORMAT_EXE}" --version OUTPUT_VARIABLE formatter_version
        RESULT_VARIABLE formatter_result)
    if(NOT formatter_result EQUAL 0 OR NOT formatter_version MATCHES "version 18\\.")
        message(FATAL_ERROR "The format check requires clang-format 18; set CLANG_FORMAT_EXE")
    endif()
endif()

function(setup_clang_tooling TARGET_NAME)
    if(NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "setup_clang_tooling called for unknown target '${TARGET_NAME}'")
    endif()

    get_target_property(configured ${TARGET_NAME} VECTORGL_TOOLING_CONFIGURED)
    if(configured)
        return()
    endif()
    set_property(TARGET ${TARGET_NAME} PROPERTY VECTORGL_TOOLING_CONFIGURED TRUE)
    if(VECTORGL_ENABLE_COMPILER_WARNINGS)
        if(MSVC)
            target_compile_options(${TARGET_NAME} PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:/W4>
                $<$<COMPILE_LANGUAGE:CXX>:/permissive->
            )
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${TARGET_NAME} PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:-Wall>
                $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
                $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
            )
        endif()
    endif()

    if(VECTORGL_ENABLE_SANITIZERS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${TARGET_NAME} PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
            )
            # PUBLIC ensures executables linking the static library also link
            # the sanitizer runtimes, including installed-package consumers.
            target_link_options(${TARGET_NAME} PUBLIC
                -fsanitize=address,undefined
            )
        else()
            message(FATAL_ERROR
                "VECTORGL_ENABLE_SANITIZERS requires a GCC or Clang toolchain"
            )
        endif()
    endif()


    if(VECTORGL_ENABLE_CLANG_TIDY)
        set_property(TARGET ${TARGET_NAME} PROPERTY CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    endif()
endfunction()

function(vectorgl_setup_directory directory)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        if(type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|EXECUTABLE)$")
            setup_clang_tooling(${target})
        endif()
    endforeach()
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        if(child MATCHES "^${PROJECT_SOURCE_DIR}/(src|tests|example|apps)(/|$)")
            vectorgl_setup_directory("${child}")
        endif()
    endforeach()
endfunction()

function(vectorgl_add_format_targets)
    vectorgl_setup_directory("${PROJECT_SOURCE_DIR}")
    if(NOT CLANG_FORMAT_EXE)
        return()
    endif()
    foreach(mode IN ITEMS format format-check)
        set(write OFF)
        if(mode STREQUAL "format")
            set(write ON)
        endif()
        add_custom_target(vectorgl-${mode}
            COMMAND ${CMAKE_COMMAND}
                "-DVECTORGL_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
                "-DVECTORGL_FORMATTER=${CLANG_FORMAT_EXE}"
                "-DVECTORGL_FORMAT_WRITE=${write}"
                -P "${PROJECT_SOURCE_DIR}/cmake/format.cmake"
            VERBATIM)
    endforeach()
    if(VECTORGL_ENABLE_FORMAT_CHECK)
        add_custom_target(vectorgl-quality ALL DEPENDS vectorgl-format-check)
    endif()
endfunction()
