# femu
An NES emulator written from scratch in C++17

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

On first run, use the **Games** tab to either browse for a single `.nes` file or point at a folder to list all ROMs in it.

