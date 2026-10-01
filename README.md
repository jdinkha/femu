# femu
An NES emulator written from scratch in C++17, by Jacob Dinkha and Frank Zombre

## Prerequisites

You'll need:

- **Git**
- **CMake 3.16+**
- **A C++17 compiler** - GCC, Clang, or MSVC
- **SDL3's own build dependencies**, which vary by OS since SDL3 is built from source as a submodule rather than installed as a system package.

## Building

Clone with submodules (or update them afterward if you forgot):

```bash
git clone --recurse-submodules github.com/jdinkha/femu.git femu
cd femu
# If you already cloned without --recurse-submodules:
git submodule update --init --recursive
```

### Linux / macOS
 
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

`-DCMAKE_BUILD_TYPE=Release` matters here - Makefile/Ninja-based builds default to no optimization otherwise, and this is a real-time emulator that needs the headroom.

### Windows (Visual Studio)

Either open the project folder directly in Visual Studio (it detects `CMakeLists.txt` and configures automatically via its built-in CMake support), or from the command line:

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

Visual Studio's generator is multi-config, so `--config Release` is required at build time (unlike the single-config Makefile/Ninja case above).

## Running

```bash
./femu                  # opens to the GUI menu - browse for a ROM from there
./femu path/to/game.nes # launches straight into that ROM
```

### FPS test

```bash
./femu --fps-test [seconds] path/to/game.nes
```

Runs the game normally (you can play it) while timing every frame, then after `seconds` (default 60), or when you close the window, prints a report and exits with 0 on pass and 1 on fail. The report covers average fps against a real NTSC NES (60.0988 fps), 1% and 0.1% low fps, slow frames, how much of each frame's 16.6 ms budget went to emulation and rendering, and the slowest frames with their likely cause. It passes if the average is within 0.1% of a real NES and the 1% low is at least 95% of it. Use a Release build, or the numbers mean little.

On first run, use the **Games** tab to either browse for a single `.nes` file or point at a folder to list all ROMs in it.

