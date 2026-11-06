install(TARGETS vectorgl
    EXPORT vectorglTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${VECTORGL_PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/vectorgl
)

# Match the build-tree public loader headers for standalone installed consumers.
install(DIRECTORY ${VECTORGL_GLAD_DIR}/include/glad ${VECTORGL_GLAD_DIR}/include/KHR
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")
install(FILES ${VECTORGL_GLAD_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/vectorgl/glad)

install(FILES ${VECTORGL_SHADER_FILES}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/vectorgl/shaders
)

install(EXPORT vectorglTargets
    FILE vectorglTargets.cmake
    NAMESPACE vectorgl::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vectorgl
)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/vectorglConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/vectorglConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vectorgl
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/vectorglConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/vectorglConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/vectorglConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vectorgl
)


install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/vectorgl)
configure_file(${PROJECT_SOURCE_DIR}/cmake/sdk-info.txt.in ${PROJECT_BINARY_DIR}/sdk-info.in @ONLY)
file(GENERATE OUTPUT ${PROJECT_BINARY_DIR}/sdk-info-$<CONFIG>.txt INPUT ${PROJECT_BINARY_DIR}/sdk-info.in)
install(FILES ${PROJECT_BINARY_DIR}/sdk-info-$<CONFIG>.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/vectorgl RENAME sdk-info.txt)
