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

        if      (key == "a")      cfg.keys.a      = (SDL_Scancode)std::stoi(value);
        else if (key == "b")      cfg.keys.b      = (SDL_Scancode)std::stoi(value);
        else if (key == "select") cfg.keys.select = (SDL_Scancode)std::stoi(value);
        else if (key == "start")  cfg.keys.start  = (SDL_Scancode)std::stoi(value);
        else if (key == "up")     cfg.keys.up     = (SDL_Scancode)std::stoi(value);
        else if (key == "down")   cfg.keys.down   = (SDL_Scancode)std::stoi(value);
        else if (key == "left")   cfg.keys.left   = (SDL_Scancode)std::stoi(value);
        else if (key == "right")  cfg.keys.right  = (SDL_Scancode)std::stoi(value);
        else if (key == "window_scale") cfg.window_scale = std::stoi(value);
        else if (key == "last_rom_dir") cfg.last_rom_dir = value;
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
    f << "window_scale=" << window_scale << "\n";
    f << "last_rom_dir=" << last_rom_dir << "\n";
}
