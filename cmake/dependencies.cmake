# Shared dependency fetching for the vectorgl project.
# Include this once from the root CMakeLists.txt.

include_guard(GLOBAL)

include(FetchContent)

FetchContent_Declare(
	stb
	GIT_REPOSITORY https://github.com/nothings/stb.git
	GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
)

FetchContent_MakeAvailable(stb)

# Build a pinned, static FreeType with no optional codec/shaping dependencies.
# Function scope avoids changing the parent project's dependency preferences.
function(vectorgl_fetch_freetype)
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
if(VECTORGL_ENABLE_HINTED_TEXT)
    vectorgl_fetch_freetype()
endif()

# GLFW is only needed by example applications and opt-in GPU integration
# tests. Library-only builds and installed-package consumers do not need it.
if(VECTORGL_BUILD_EXAMPLES OR VECTORGL_BUILD_GPU_TESTS)
	find_package(glfw3 3.3 CONFIG QUIET)
	if(NOT TARGET glfw)
		FetchContent_Declare(
			glfw
			GIT_REPOSITORY https://github.com/glfw/glfw.git
			GIT_TAG 3.3.8
		)
		set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
		set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
		set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
		set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
		FetchContent_MakeAvailable(glfw)
	endif()
endif()

# GLAD is generated once and committed to the repository so consumers do not
# need Python, Jinja2, or network access to generate the OpenGL loader.
set(VECTORGL_GLAD_DIR "${PROJECT_SOURCE_DIR}/third_party/glad")

