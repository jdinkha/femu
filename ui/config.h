#pragma once
#include <SDL3/SDL.h>
#include <string>

// Remappable D-pad/button bindings for one controller. Both controller 1
// and (optionally) controller 2 use this same struct.
struct KeyBindings {
    SDL_Scancode a      = SDL_SCANCODE_Z;
    SDL_Scancode b      = SDL_SCANCODE_X;
    SDL_Scancode select = SDL_SCANCODE_RSHIFT;
    SDL_Scancode start  = SDL_SCANCODE_RETURN;
    SDL_Scancode up     = SDL_SCANCODE_UP;
    SDL_Scancode down   = SDL_SCANCODE_DOWN;
    SDL_Scancode left   = SDL_SCANCODE_LEFT;
    SDL_Scancode right  = SDL_SCANCODE_RIGHT;
};

// System-level hotkeys (not tied to either controller). This is deliberately
// separate from KeyBindings and easy to extend - save state, load state, and
// volume up/down are the obvious next additions once those features exist;
// each is just one more field here plus one more row in the Hotkeys panel.
struct HotkeyBindings {
    SDL_Scancode fullscreen = SDL_SCANCODE_F11;
};

struct AppConfig {
    KeyBindings keys;                // controller 1
    KeyBindings keys2;                // controller 2 (only read if controller2_enabled)
    bool controller2_enabled = false;
    HotkeyBindings hotkeys;

    int window_scale = 3;
    std::string last_rom_dir = "roms";

    // Simple line-based "key=value" text format - not INI/JSON, just enough
    // to round-trip these settings without pulling in a parsing library.
    static AppConfig Load(const std::string& path);
    void Save(const std::string& path) const;
};
