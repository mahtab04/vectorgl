# Shared dependency fetching for the vectorgl project.
# Include this once from the root CMakeLists.txt.

include_guard(GLOBAL)

include(FetchContent)

function(vectorgl_require_fetch name)
    string(TOUPPER "${name}" key)
    if(NOT VECTORGL_FETCH_DEPENDENCIES AND NOT FETCHCONTENT_SOURCE_DIR_${key})
        message(FATAL_ERROR "Missing ${name}. Downloads disabled: provide FETCHCONTENT_SOURCE_DIR_${key}, an installed dependency, or enable VECTORGL_FETCH_DEPENDENCIES")
    endif()
endfunction()


if(VECTORGL_STB_INCLUDE_DIR)
    if(NOT EXISTS "${VECTORGL_STB_INCLUDE_DIR}/stb_truetype.h")
        message(FATAL_ERROR "VECTORGL_STB_INCLUDE_DIR must contain stb_truetype.h")
    endif()
    set(stb_SOURCE_DIR "${VECTORGL_STB_INCLUDE_DIR}")
else()
    vectorgl_require_fetch(stb)
    FetchContent_Declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4)
    FetchContent_MakeAvailable(stb)
endif()
if(EXISTS "${stb_SOURCE_DIR}/LICENSE")
    install(FILES "${stb_SOURCE_DIR}/LICENSE" DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/vectorgl/stb)
endif()

# Pin the tessellator source and verify the archive before compiling it into
# vectorgl. No separate library or dependency is exported to package consumers.
function(vectorgl_fetch_libtess2)
    vectorgl_require_fetch(vectorgl_libtess2)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(vectorgl_libtess2
        URL https://codeload.github.com/memononen/libtess2/tar.gz/8dbd6483e920311a58c9af10a10beb278efebc36
        URL_HASH SHA256=083f507dd3b27eba01fa540b9efe3bbeed36d5bd4ea0e8869897d21f52c5f98b
    )
    FetchContent_MakeAvailable(vectorgl_libtess2)
    set(VECTORGL_LIBTESS2_DIR "${vectorgl_libtess2_SOURCE_DIR}" PARENT_SCOPE)
    install(FILES "${vectorgl_libtess2_SOURCE_DIR}/LICENSE.txt"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/vectorgl/libtess2)
endfunction()
vectorgl_fetch_libtess2()

# Build a pinned, static FreeType with no optional codec/shaping dependencies.
# Function scope avoids changing the parent project's dependency preferences.
function(vectorgl_fetch_freetype)
    vectorgl_require_fetch(vectorgl_freetype)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    set(BUILD_SHARED_LIBS OFF)
    set(FT_DISABLE_ZLIB ON)
    set(FT_DISABLE_BZIP2 ON)
    set(FT_DISABLE_PNG ON)
    set(FT_DISABLE_HARFBUZZ ON)
    set(FT_DISABLE_BROTLI ON)
    FetchContent_Declare(vectorgl_freetype
        URL https://codeload.github.com/freetype/freetype/tar.gz/refs/tags/VER-2-14-3
        URL_HASH SHA256=dc49de6b01a266eef4876a4dd34d9842c475d3e28ff2eff63bd2fb760ab56261
    )
    FetchContent_MakeAvailable(vectorgl_freetype)
    install(FILES
        ${vectorgl_freetype_SOURCE_DIR}/LICENSE.TXT
        ${vectorgl_freetype_SOURCE_DIR}/docs/FTL.TXT
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/vectorgl/FreeType
    )
endfunction()
set(VECTORGL_FREETYPE_RESOLVED OFF)
if(VECTORGL_ENABLE_HINTED_TEXT)
    if(NOT VECTORGL_FREETYPE_PROVIDER STREQUAL "BUNDLED")
        find_package(Freetype QUIET)
    endif()
    if(TARGET Freetype::Freetype)
        set(VECTORGL_FREETYPE_TARGET Freetype::Freetype)
        set(VECTORGL_FREETYPE_RESOLVED SYSTEM)
    elseif(VECTORGL_FREETYPE_PROVIDER STREQUAL "SYSTEM")
        message(FATAL_ERROR "SYSTEM FreeType requested but Freetype::Freetype was not found")
    else()
        vectorgl_fetch_freetype()
        set(VECTORGL_FREETYPE_TARGET freetype)
        set(VECTORGL_FREETYPE_RESOLVED BUNDLED)
    endif()
endif()

# The default host adapter needs GLFW, as do enabled graphical apps/examples/tests.
function(vectorgl_resolve_glfw)
    if(NOT TARGET glfw)
        find_package(glfw3 3.3 CONFIG QUIET)
    endif()
    if(NOT TARGET glfw)
        vectorgl_require_fetch(glfw)
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        option(GLFW_INSTALL "Install bundled GLFW" ${VECTORGL_BUILD_GLFW_ADAPTER})
        option(GLFW_BUILD_DOCS "Build GLFW docs" OFF)
        option(GLFW_BUILD_TESTS "Build GLFW tests" OFF)
        option(GLFW_BUILD_EXAMPLES "Build GLFW examples" OFF)
        if(VECTORGL_BUILD_GLFW_ADAPTER AND NOT GLFW_INSTALL)
            message(FATAL_ERROR
                "The bundled GLFW adapter SDK requires GLFW_INSTALL=ON, or an installed glfw3 package. "
                "A previous build may have cached GLFW_INSTALL=OFF. Reconfigure with "
                "-DGLFW_INSTALL=ON, or use a VectorGL library/platform preset.")
        endif()
        FetchContent_Declare(glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.3.8)
        FetchContent_MakeAvailable(glfw)
        if(VECTORGL_BUILD_GLFW_ADAPTER)
            install(FILES "${glfw_SOURCE_DIR}/LICENSE.md"
                DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/vectorgl/GLFW)
        endif()
    endif()
endfunction()
if(VECTORGL_BUILD_EXAMPLES OR VECTORGL_BUILD_APPS OR VECTORGL_BUILD_GPU_TESTS OR VECTORGL_BUILD_GLFW_ADAPTER)
    vectorgl_resolve_glfw()
endif()

# GLAD is generated once and committed to the repository so consumers do not
# need Python, Jinja2, or network access to generate the OpenGL loader.
set(VECTORGL_GLAD_DIR "${PROJECT_SOURCE_DIR}/third_party/glad")

