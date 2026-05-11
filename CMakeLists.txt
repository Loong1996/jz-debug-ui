cmake_minimum_required(VERSION 3.14)
project(debug_ui_demo)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS)

add_subdirectory(third_party/imgui)
add_subdirectory(third_party/implot)

# ---- unit test (no GUI deps) ----
add_executable(test_ring_buffer tests/test_ring_buffer.cpp)
target_include_directories(test_ring_buffer PRIVATE src)

# ---- main demo ----
add_executable(debug_ui_demo WIN32
    src/main.cpp
    src/dui_app.cpp
    src/dui_canvas.cpp
    src/dui_inspector.cpp
    src/dui_metrics.cpp
)

target_include_directories(debug_ui_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/implot
)

target_link_libraries(debug_ui_demo PRIVATE
    imgui
    implot
    d3d11
    dxgi
    d3dcompiler
)

# Copy assets/ next to the exe after each build so fonts are always available
add_custom_command(TARGET debug_ui_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:debug_ui_demo>/assets
)

# VS debugger: run from project root so relative paths (assets/) work
set_target_properties(debug_ui_demo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
