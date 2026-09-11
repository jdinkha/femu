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
    SDL_Scancode save_state = SDL_SCANCODE_F5; // quick-save to <rom>.state
    SDL_Scancode load_state = SDL_SCANCODE_F7; // quick-load from <rom>.state
};

struct AppConfig {
    KeyBindings keys;                // controller 1
    KeyBindings keys2;                // controller 2 (only read if controller2_enabled)
    bool controller2_enabled = false;
    HotkeyBindings hotkeys;

    // Windowed size. window_scale drives the 1x-4x buttons. window_width /
    // window_height, when both > 0, are an explicit pixel size chosen from the
    // resolution dropdown and take precedence over the scale; the scale
    // buttons clear them back to 0.
    int window_scale = 3;
    int window_width = 0;
    int window_height = 0;

    // Empty means the user hasn't picked a ROM folder yet (e.g. a fresh
    // install) - the Games tab shows a prompt instead of a folder path in
    // that case, rather than silently pointing at a "roms" directory that
    // doesn't exist.
    std::string last_rom_dir = "";

    // Audio. master_volume is a linear 0..1 gain applied to the SDL audio
    // stream. audio_device is the SDL playback device *name* (not ID, which
    // isn't stable across runs); empty means "follow the system default".
    float master_volume = 1.0f;
    std::string audio_device = "";

    // Simple line-based "key=value" text format - not INI/JSON, just enough
    // to round-trip these settings without pulling in a parsing library.
    static AppConfig Load(const std::string& path);
    void Save(const std::string& path) const;
};
