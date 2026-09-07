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
    mem/savestate.cpp
    mem/cartridge.cpp
    mem/mapper000.cpp
    mem/mapper001.cpp
    mem/mapper004.cpp
    ppu/ppu.cpp
    apu/apu.cpp
)

target_link_libraries(femu PRIVATE SDL3::SDL3 imgui)

# SDL3 builds as a shared library (SDL3.dll) by default on Windows, and the
# .exe needs that DLL sitting in the same folder to launch at all - CMake
# doesn't do this automatically, it's on you to place it there. This copies
# it next to femu.exe after every build, using generator expressions so it
# resolves correctly whether you're building Debug or Release.
if (WIN32)
    add_custom_command(TARGET femu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:femu>
        COMMENT "Copying SDL3.dll next to femu.exe"
    )
endif()
