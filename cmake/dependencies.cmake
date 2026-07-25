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
	stb
	GIT_REPOSITORY https://github.com/nothings/stb.git
	GIT_TAG 31c1ad37456438565541f4919958214b6e762fb4
)

FetchContent_MakeAvailable(glfw stb)

# GLAD is generated once and committed to the repository so consumers do not
# need Python, Jinja2, or network access to generate the OpenGL loader.
set(VECTORGL_GLAD_DIR "${PROJECT_SOURCE_DIR}/third_party/glad")

