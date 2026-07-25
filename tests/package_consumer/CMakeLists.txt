cmake_minimum_required(VERSION 3.20)
project(vectorgl_package_consumer LANGUAGES CXX)

find_package(vectorgl 1.0 CONFIG REQUIRED)

add_executable(vectorgl_package_consumer main.cpp)
target_link_libraries(vectorgl_package_consumer PRIVATE vectorgl::vectorgl)
target_compile_features(vectorgl_package_consumer PRIVATE cxx_std_20)

enable_testing()
add_test(NAME vectorgl_package_consumer COMMAND vectorgl_package_consumer)
