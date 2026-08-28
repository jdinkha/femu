cmake_minimum_required(VERSION 3.16)
project(femu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(./SDL EXCLUDE_FROM_ALL)

add_executable(femu
    main.cpp
    cpu/cpu.cpp
    mem/bus.cpp
    mem/cartridge.cpp
    mem/mapper000.cpp
    ppu/ppu.cpp
    apu/apu.cpp
)

target_link_libraries(femu PRIVATE SDL3::SDL3)
