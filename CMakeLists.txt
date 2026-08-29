cmake_minimum_required(VERSION 3.16)
project(femu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(./SDL EXCLUDE_FROM_ALL)

set(IMGUI_DIR ./imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

add_executable(femu
    main.cpp
    ui/config.cpp
    cpu/cpu.cpp
    mem/bus.cpp
    mem/cartridge.cpp
    mem/mapper000.cpp
    mem/mapper001.cpp
    mem/mapper004.cpp
    ppu/ppu.cpp
    apu/apu.cpp
)

target_link_libraries(femu PRIVATE SDL3::SDL3 imgui)
