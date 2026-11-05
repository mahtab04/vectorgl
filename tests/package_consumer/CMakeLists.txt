cmake_minimum_required(VERSION 3.20)
project(vectorgl_package_consumer LANGUAGES CXX)

find_package(vectorgl 1.0 CONFIG REQUIRED)

option(VECTORGL_EXPECT_NO_UI "Verify optional UI is absent from this installed package" OFF)
if(VECTORGL_EXPECT_NO_UI)
    find_package(vectorgl CONFIG QUIET COMPONENTS ui)
    if(vectorgl_FOUND OR TARGET vectorgl::ui)
        message(FATAL_ERROR "Graphics-only package unexpectedly provides the UI component")
    endif()
endif()

add_executable(vectorgl_package_consumer main.cpp)
target_link_libraries(vectorgl_package_consumer PRIVATE vectorgl::vectorgl)
target_compile_features(vectorgl_package_consumer PRIVATE cxx_std_20)

enable_testing()
add_test(NAME vectorgl_package_consumer COMMAND vectorgl_package_consumer)
