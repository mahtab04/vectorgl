include_guard(GLOBAL)

option(ENABLE_CLANG_TIDY "Run clang-tidy during build" ON)
option(ENABLE_FORMAT_CHECK "Run clang-format check during build" ON)
option(ENABLE_COMPILER_WARNINGS "Enable recommended compiler warnings" ON)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

find_program(CLANG_TIDY_EXE NAMES clang-tidy
    HINTS
        "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/Llvm/x64/bin"
        "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin"
        "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/Llvm/x64/bin"
        "C:/Program Files/LLVM/bin"
)

find_program(CLANG_FORMAT_EXE NAMES clang-format
    HINTS
        "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/Llvm/x64/bin"
        "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin"
        "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/Llvm/x64/bin"
        "C:/Program Files/LLVM/bin"
)

if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
else()
    message(WARNING "clang-tidy not found")
endif()

if(CLANG_FORMAT_EXE)
    message(STATUS "clang-format found: ${CLANG_FORMAT_EXE}")
else()
    message(WARNING "clang-format not found")
endif()

function(_vectorgl_escape_regex INPUT OUTPUT)
    set(_value "${INPUT}")
    string(REPLACE "\\" "/" _value "${_value}")
    string(REGEX REPLACE "([][+.*^$(){}|?\\])" "\\\\\\1" _value "${_value}")
    set(${OUTPUT} "${_value}" PARENT_SCOPE)
endfunction()

function(setup_clang_tooling TARGET_NAME)
    if(NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "setup_clang_tooling called for unknown target '${TARGET_NAME}'")
    endif()

    if(ENABLE_COMPILER_WARNINGS)
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

    if(ENABLE_SANITIZERS)
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
                "ENABLE_SANITIZERS requires a GCC or Clang toolchain"
            )
        endif()
    endif()

    file(REAL_PATH "${PROJECT_SOURCE_DIR}" _project_source_dir)
    string(REPLACE "\\" "/" _project_source_dir "${_project_source_dir}")
    _vectorgl_escape_regex("${_project_source_dir}" _project_source_regex)
    file(REAL_PATH "${CMAKE_BINARY_DIR}" _binary_dir)
    string(REPLACE "\\" "/" _binary_dir "${_binary_dir}")
    _vectorgl_escape_regex("${_binary_dir}" _binary_dir_regex)

    if(ENABLE_CLANG_TIDY AND CLANG_TIDY_EXE)
        set_target_properties(${TARGET_NAME} PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--header-filter=^${_project_source_regex}/(src|include)/"
        )
    endif()

    if(NOT CLANG_FORMAT_EXE)
        return()
    endif()

    get_target_property(_target_sources ${TARGET_NAME} SOURCES)
    get_target_property(_target_source_dir ${TARGET_NAME} SOURCE_DIR)

    if(NOT _target_sources)
        return()
    endif()

    set(_format_sources "")
    foreach(_source IN LISTS _target_sources)
        if(_source MATCHES "^\\$<")
            continue()
        endif()

        if(NOT IS_ABSOLUTE "${_source}")
            set(_source "${_target_source_dir}/${_source}")
        endif()

        file(REAL_PATH "${_source}" _source_real)
        string(REPLACE "\\" "/" _source_real "${_source_real}")

        if(_source_real MATCHES "^${_binary_dir_regex}(/|$)")
            continue()
        endif()

        if(NOT _source_real MATCHES "^${_project_source_regex}(/|$)")
            continue()
        endif()

        if(_source_real MATCHES "^${_project_source_regex}/third_party(/|$)")
            continue()
        endif()

        if(_source_real MATCHES "\\.(c|cc|cpp|cxx|h|hh|hpp|hxx)$")
            list(APPEND _format_sources "${_source_real}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES _format_sources)
    if(NOT _format_sources)
        return()
    endif()

    add_custom_target(${TARGET_NAME}-format
        COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${_format_sources}
        COMMENT "[${TARGET_NAME}] Running clang-format"
        VERBATIM
    )

    add_custom_target(${TARGET_NAME}-format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror -style=file ${_format_sources}
        COMMENT "[${TARGET_NAME}] Checking clang-format compliance"
        VERBATIM
    )

    if(ENABLE_FORMAT_CHECK)
        add_dependencies(${TARGET_NAME} ${TARGET_NAME}-format-check)
    endif()
endfunction()
