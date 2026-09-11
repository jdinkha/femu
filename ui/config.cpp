#include "config.h"
#include <fstream>
#include <sstream>

AppConfig AppConfig::Load(const std::string& path) {
    AppConfig cfg; // defaults if the file doesn't exist or a key is missing
    std::ifstream f(path);
    if (!f.is_open()) return cfg;

    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        if (value.empty()) continue;

        // Controller 1
        if      (key == "a")      cfg.keys.a      = (SDL_Scancode)std::stoi(value);
        else if (key == "b")      cfg.keys.b      = (SDL_Scancode)std::stoi(value);
        else if (key == "select") cfg.keys.select = (SDL_Scancode)std::stoi(value);
        else if (key == "start")  cfg.keys.start  = (SDL_Scancode)std::stoi(value);
        else if (key == "up")     cfg.keys.up     = (SDL_Scancode)std::stoi(value);
        else if (key == "down")   cfg.keys.down   = (SDL_Scancode)std::stoi(value);
        else if (key == "left")   cfg.keys.left   = (SDL_Scancode)std::stoi(value);
        else if (key == "right")  cfg.keys.right  = (SDL_Scancode)std::stoi(value);
        // Controller 2
        else if (key == "2_a")      cfg.keys2.a      = (SDL_Scancode)std::stoi(value);
        else if (key == "2_b")      cfg.keys2.b      = (SDL_Scancode)std::stoi(value);
        else if (key == "2_select") cfg.keys2.select = (SDL_Scancode)std::stoi(value);
        else if (key == "2_start")  cfg.keys2.start  = (SDL_Scancode)std::stoi(value);
        else if (key == "2_up")     cfg.keys2.up     = (SDL_Scancode)std::stoi(value);
        else if (key == "2_down")   cfg.keys2.down   = (SDL_Scancode)std::stoi(value);
        else if (key == "2_left")   cfg.keys2.left   = (SDL_Scancode)std::stoi(value);
        else if (key == "2_right")  cfg.keys2.right  = (SDL_Scancode)std::stoi(value);
        else if (key == "controller2_enabled") cfg.controller2_enabled = (std::stoi(value) != 0);
        // Hotkeys
        else if (key == "hotkey_fullscreen") cfg.hotkeys.fullscreen = (SDL_Scancode)std::stoi(value);
        else if (key == "hotkey_save_state") cfg.hotkeys.save_state = (SDL_Scancode)std::stoi(value);
        else if (key == "hotkey_load_state") cfg.hotkeys.load_state = (SDL_Scancode)std::stoi(value);
        // Misc
        else if (key == "window_scale") cfg.window_scale = std::stoi(value);
        else if (key == "window_width") cfg.window_width = std::stoi(value);
        else if (key == "window_height") cfg.window_height = std::stoi(value);
        else if (key == "last_rom_dir") cfg.last_rom_dir = value;
        // Audio
        else if (key == "master_volume") cfg.master_volume = std::stof(value);
        else if (key == "audio_device") cfg.audio_device = value;
    }
    return cfg;
}

void AppConfig::Save(const std::string& path) const {
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) return;

    f << "a="      << (int)keys.a      << "\n";
    f << "b="      << (int)keys.b      << "\n";
    f << "select=" << (int)keys.select << "\n";
    f << "start="  << (int)keys.start  << "\n";
    f << "up="     << (int)keys.up     << "\n";
    f << "down="   << (int)keys.down   << "\n";
    f << "left="   << (int)keys.left   << "\n";
    f << "right="  << (int)keys.right  << "\n";

    f << "2_a="      << (int)keys2.a      << "\n";
    f << "2_b="      << (int)keys2.b      << "\n";
    f << "2_select=" << (int)keys2.select << "\n";
    f << "2_start="  << (int)keys2.start  << "\n";
    f << "2_up="     << (int)keys2.up     << "\n";
    f << "2_down="   << (int)keys2.down   << "\n";
    f << "2_left="   << (int)keys2.left   << "\n";
    f << "2_right="  << (int)keys2.right  << "\n";
    f << "controller2_enabled=" << (controller2_enabled ? 1 : 0) << "\n";

    f << "hotkey_fullscreen=" << (int)hotkeys.fullscreen << "\n";
    f << "hotkey_save_state=" << (int)hotkeys.save_state << "\n";
    f << "hotkey_load_state=" << (int)hotkeys.load_state << "\n";

    f << "window_scale=" << window_scale << "\n";
    f << "window_width=" << window_width << "\n";
    f << "window_height=" << window_height << "\n";
    f << "last_rom_dir=" << last_rom_dir << "\n";

    f << "master_volume=" << master_volume << "\n";
    f << "audio_device=" << audio_device << "\n";
}
