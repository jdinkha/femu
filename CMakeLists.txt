cmake_minimum_required(VERSION 3.16)
project(femu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- SDL3 ---
# Point this at wherever the SDL3 source tree actually lives in your project.
# SDL3's own CMakeLists.txt exports an SDL3::SDL3 target, so add_subdirectory
# is all that's needed - no separate find_package step.
add_subdirectory(./SDL EXCLUDE_FROM_ALL)

add_executable(femu
    main.cpp
    cpu/cpu.cpp
    mem/bus.cpp
    mem/cartridge.cpp
    mem/mapper000.cpp
    ppu/ppu.cpp
)

target_link_libraries(femu PRIVATE SDL3::SDL3)
