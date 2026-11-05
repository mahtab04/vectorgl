add_library(vectorgl_ui STATIC context.cpp
    ${PROJECT_SOURCE_DIR}/include/PublicHeaders/vectorgl/ui/context.hpp
    ${PROJECT_SOURCE_DIR}/include/PublicHeaders/vectorgl/ui/events.hpp
    ${PROJECT_SOURCE_DIR}/include/PublicHeaders/vectorgl/ui/widget.hpp
    ${PROJECT_SOURCE_DIR}/include/PublicHeaders/vectorgl/ui/services.hpp)
add_library(vectorgl::ui ALIAS vectorgl_ui)
set_target_properties(vectorgl_ui PROPERTIES EXPORT_NAME ui)
target_link_libraries(vectorgl_ui PUBLIC vectorgl::vectorgl)
target_compile_features(vectorgl_ui PUBLIC cxx_std_20)
setup_clang_tooling(vectorgl_ui)

install(TARGETS vectorgl_ui EXPORT vectorglUiTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/PublicHeaders/vectorgl/ui/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/vectorgl/ui
    FILES_MATCHING PATTERN "*.hpp")
install(EXPORT vectorglUiTargets FILE vectorglUiTargets.cmake NAMESPACE vectorgl::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vectorgl)

# This example demonstrates event contracts without a window/context or GLFW.
if(VECTORGL_BUILD_UI_EXAMPLES)
    add_executable(vectorgl_ui_routing ${PROJECT_SOURCE_DIR}/example/ui_routing_demo.cpp)
    target_link_libraries(vectorgl_ui_routing PRIVATE vectorgl::ui)
endif()
