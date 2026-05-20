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

# ---- core library ----
add_library(dui STATIC
    src/dui_app.cpp
    src/dui_canvas.cpp
    src/dui_inspector.cpp
    src/dui_metrics.cpp
    src/dui_log.cpp
    src/dui_ext.cpp
    src/dui_commands.cpp
    src/dui_detail.cpp
    src/dui_draw_all.cpp
    src/dui_user_metrics.cpp
    src/dui_events.cpp
    src/dui_hotkeys.cpp
    src/dui_search.cpp
    src/dui_trails.cpp
    src/dui_pins.cpp
    src/dui_snapshot.cpp
    src/dui_profiler.cpp
    src/dui_menubar.cpp
    src/dui_time.cpp
    src/dui_entity_log.cpp
    src/dui_replay.cpp
    src/dui_minimap.cpp
    src/dui_select_group.cpp
    src/dui_layers.cpp
)
target_include_directories(dui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/implot
)
target_compile_options(dui PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_link_libraries(dui PUBLIC imgui implot d3d11 dxgi d3dcompiler)

# ---- demo exe ----
add_executable(dui_demo WIN32
    src/main.cpp
    src/dui_mock.cpp
    src/dui_demo_setup.cpp
)
target_compile_options(dui_demo PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_link_libraries(dui_demo PRIVATE dui)

# Copy assets/ next to the exe after each build so fonts are always available
add_custom_command(TARGET dui_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:dui_demo>/assets
)

# VS debugger: run from project root so relative paths (assets/) work
set_target_properties(dui_demo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
