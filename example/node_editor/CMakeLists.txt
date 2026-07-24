set(VECTORGL_NODE_EDITOR_SOURCES
    node_editor_demo.cpp
    src/editor_app.cpp
    src/editor_state.cpp
    src/graph_model.cpp
    src/graph_renderer.cpp
    src/input_controller.cpp
    src/theme.cpp
)

set(VECTORGL_NODE_EDITOR_HEADERS
    include/editor_app.hpp
    include/editor_state.hpp
    include/graph_model.hpp
    include/graph_renderer.hpp
    include/input_controller.hpp
    include/theme.hpp
)

add_executable(vectorgl_node_editor
    ${VECTORGL_NODE_EDITOR_SOURCES}
    ${VECTORGL_NODE_EDITOR_HEADERS}
)

target_include_directories(vectorgl_node_editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vectorgl_node_editor PRIVATE vectorgl glfw glad)
