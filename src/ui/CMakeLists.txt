add_library(vectorgl_ui STATIC context.cpp painting.cpp layout.cpp geometry.hpp
    ${PROJECT_SOURCE_DIR}/include/PublicHeaders/vectorgl/ui/layout.hpp
    ${PROJECT_SOURCE_DIR}/include/PublicHeaders/vectorgl/ui/painting.hpp
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
    FILES_MATCHING PATTERN "*.hpp" PATTERN "glfw_adapter.hpp" EXCLUDE)
install(EXPORT vectorglUiTargets FILE vectorglUiTargets.cmake NAMESPACE vectorgl::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vectorgl)

if(VECTORGL_BUILD_GLFW_ADAPTER)
    add_library(vectorgl_glfw STATIC glfw_adapter.cpp
        ${PROJECT_SOURCE_DIR}/include/PublicHeaders/vectorgl/ui/glfw_adapter.hpp)
    add_library(vectorgl::glfw ALIAS vectorgl_glfw)
    set_target_properties(vectorgl_glfw PROPERTIES EXPORT_NAME glfw)
    target_link_libraries(vectorgl_glfw PUBLIC vectorgl::ui glfw)
    target_include_directories(vectorgl_glfw PRIVATE ${VECTORGL_INTERNAL_INCLUDE_DIR})
    setup_clang_tooling(vectorgl_glfw)
    install(TARGETS vectorgl_glfw EXPORT vectorglGlfwTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES ${PROJECT_SOURCE_DIR}/include/PublicHeaders/vectorgl/ui/glfw_adapter.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/vectorgl/ui)
    install(EXPORT vectorglGlfwTargets FILE vectorglGlfwTargets.cmake NAMESPACE vectorgl::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vectorgl)

endif()
