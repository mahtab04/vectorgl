# Shared dependency fetching for the vectorgl project.
# Include this once from the root CMakeLists.txt.

include_guard(GLOBAL)

include(FetchContent)

FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG 3.3.8
)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
	glad
	GIT_REPOSITORY https://github.com/Dav1dde/glad.git
	GIT_TAG v2.0.8
	SOURCE_SUBDIR cmake
)

FetchContent_Declare(
	stb
	GIT_REPOSITORY https://github.com/nothings/stb.git
	GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
)

FetchContent_MakeAvailable(glfw glad stb)

set(VECTORGL_GLAD_DIR "${CMAKE_CURRENT_BINARY_DIR}/gladsources/vectorgl_glad")
if(NOT TARGET vectorgl_glad)
	glad_add_library(
		vectorgl_glad
		STATIC
		REPRODUCIBLE
		LOADER
		LOCATION "${VECTORGL_GLAD_DIR}"
		API gl:core=3.3
	)
endif()

