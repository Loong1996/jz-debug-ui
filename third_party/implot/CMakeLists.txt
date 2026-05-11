cmake_minimum_required(VERSION 3.14)

add_library(implot STATIC
    implot.cpp
    implot_items.cpp
)

target_include_directories(implot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(implot PUBLIC imgui)
