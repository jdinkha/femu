#pragma once
#include <SDL3/SDL.h>
#include <string>

// Remappable D-pad/button bindings. Defaults match what main.cpp used to
// hardcode (Z/X for B/A... wait, A/B, arrows for D-pad, Enter/RShift for
// Start/Select) - now living here instead so the GUI's keybind panel and
// ReadController() share one source of truth.
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

struct AppConfig {
    KeyBindings keys;
    int window_scale = 3;
    std::string last_rom_dir = "roms";

    // Simple line-based "key=value" text format - not INI/JSON, just enough
    // to round-trip these settings without pulling in a parsing library.
    static AppConfig Load(const std::string& path);
    void Save(const std::string& path) const;
};
