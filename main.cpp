#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <cstdio>
#include <cfloat>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

#include "mem/bus.h"
#include "mem/savestate.h"
#include "ui/config.h"

namespace fs = std::filesystem;

static constexpr int NES_WIDTH = 256;
static constexpr int NES_HEIGHT = 240;
static constexpr const char* CONFIG_PATH = "femu_config.txt";
static constexpr double OVERLAY_HINT_SECONDS = 5.0;
static constexpr double STATUS_MESSAGE_SECONDS = 2.5; // transient "State saved" / error toast

enum class AppState { MENU, RUNNING };

struct DPadState {
    bool left_newer = false; // if both Left and Right are held, was Left pressed more recently?
    bool up_newer = false;
    bool prev_left = false, prev_right = false, prev_up = false, prev_down = false;

    void Update(bool left, bool right, bool up, bool down) {
        if (left && !prev_left)   left_newer = true;   // Left just pressed -> it's the newer one
        if (right && !prev_right) left_newer = false;  // Right just pressed -> it wins instead
        if (up && !prev_up)       up_newer = true;
        if (down && !prev_down)   up_newer = false;
        prev_left = left; prev_right = right; prev_up = up; prev_down = down;
    }
};

static uint8_t ReadController(const bool* keys, const KeyBindings& kb, DPadState& dpad) {
    bool left  = keys[kb.left];
    bool right = keys[kb.right];
    bool up    = keys[kb.up];
    bool down  = keys[kb.down];

    dpad.Update(left, right, up, down);

    // Resolve opposing pairs so at most one of each axis reaches the game
    if (left && right) {
        if (dpad.left_newer) right = false;
        else                 left = false;
    }
    if (up && down) {
        if (dpad.up_newer) down = false;
        else               up = false;
    }

    uint8_t c = 0;
    if (keys[kb.a])      c |= 0x80;
    if (keys[kb.b])      c |= 0x40;
    if (keys[kb.select]) c |= 0x20;
    if (keys[kb.start])  c |= 0x10;
    if (up)              c |= 0x08;
    if (down)            c |= 0x04;
    if (left)            c |= 0x02;
    if (right)           c |= 0x01;
    return c;
}

// Centered, aspect-ratio-correct destination rect for the NES framebuffer
// within an arbitrary (possibly resized, possibly fullscreen) window -
// letterboxes instead of stretching if the window isn't exactly 256:240.
static SDL_FRect ComputeDestRect(int window_w, int window_h) {
    float aspect = (float)NES_WIDTH / (float)NES_HEIGHT;
    float win_aspect = (float)window_w / (float)window_h;
    SDL_FRect r;
    if (win_aspect > aspect) {
        r.h = (float)window_h;
        r.w = r.h * aspect;
    } else {
        r.w = (float)window_w;
        r.h = r.w / aspect;
    }
    r.x = ((float)window_w - r.w) / 2.0f;
    r.y = ((float)window_h - r.h) / 2.0f;
    return r;
}

// Resize (if needed) and reposition `window` so it fits inside the primary
// display's usable area with its title bar on-screen.
//
// This matters most on Windows: when a window is taller than the usable
// desktop, SDL falls back to centering it on the *full* monitor bounds, which
// puts `y` at a negative value and pushes the whole title bar (close /
// minimize / maximize buttons) above the top of the screen where it can't be
// reached. femu sizes its window as a fixed multiple of 240px, so a 3x or 4x
// window easily overflows a laptop display once the taskbar and window frame
// are accounted for. Clamping the size and recentering keeps the caption
// visible on every platform.
static void FitWindowToDisplay(SDL_Window* window, int desired_w, int desired_h) {
    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (!display) display = SDL_GetPrimaryDisplay();

    SDL_Rect usable{};
    if (!display || !SDL_GetDisplayUsableBounds(display, &usable)) {
        SDL_SetWindowSize(window, desired_w, desired_h);
        return;
    }

    // Window-frame thickness (title bar + borders). Zero until the window has
    // been shown on some backends, so fall back to a generous estimate.
    int top = 0, left = 0, bottom = 0, right = 0;
    SDL_GetWindowBordersSize(window, &top, &left, &bottom, &right);
    if (top == 0 && bottom == 0) { top = 40; }
    const int frame_w = left + right;
    const int frame_h = top + bottom;

    int w = desired_w, h = desired_h;
    if (w + frame_w > usable.w) w = usable.w - frame_w;
    if (h + frame_h > usable.h) h = usable.h - frame_h;
    if (w < NES_WIDTH)  w = NES_WIDTH;   // never shrink below 1x
    if (h < NES_HEIGHT) h = NES_HEIGHT;
    SDL_SetWindowSize(window, w, h);

    // Center within the usable area, but never let the top of the frame go
    // above the usable region - a title bar you can see beats a perfectly
    // centered one you can't.
    int x = usable.x + (usable.w - w - frame_w) / 2 + left;
    int y = usable.y + (usable.h - h - frame_h) / 2 + top;
    if (x < usable.x + left) x = usable.x + left;
    if (y < usable.y + top)  y = usable.y + top;
    SDL_SetWindowPosition(window, x, y);
}

// Reflects the currently loaded ROM in the window's title bar, e.g.
// "femu - Super Mario Bros" while Super Mario Bros.nes is loaded, falling
// back to plain "femu" when no game is loaded.
static void UpdateWindowTitle(SDL_Window* window, const std::string& rom_path) {
    if (rom_path.empty()) {
        SDL_SetWindowTitle(window, "femu");
    } else {
        std::string title = "femu - " + fs::path(rom_path).stem().string();
        SDL_SetWindowTitle(window, title.c_str());
    }
}

// One enumerated SDL playback device. `id` is only valid for this run;
// `name` is what we persist in the config so a chosen device can be
// re-selected on the next launch.
struct AudioDevice {
    SDL_AudioDeviceID id;
    std::string name;
};

static std::vector<AudioDevice> EnumerateAudioDevices() {
    std::vector<AudioDevice> out;
    int count = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&count);
    if (!ids) return out;
    for (int i = 0; i < count; i++) {
        const char* n = SDL_GetAudioDeviceName(ids[i]);
        out.push_back({ ids[i], n ? n : "(unknown device)" });
    }
    SDL_free(ids);
    return out;
}

// Resolve a persisted device name back to a live ID. Empty name, or a name
// that no longer matches any connected device, falls back to the default.
static SDL_AudioDeviceID DeviceIdForName(const std::vector<AudioDevice>& devices,
                                        const std::string& name) {
    if (name.empty()) return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    for (auto& d : devices) {
        if (d.name == name) return d.id;
    }
    return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
}

// Open a mono F32 44.1kHz playback stream on the given device and start it.
// Returns nullptr on failure (caller decides whether that's fatal).
static SDL_AudioStream* OpenAudioStream(SDL_AudioDeviceID device) {
    SDL_AudioSpec spec{};
    spec.freq = 44100;
    spec.format = SDL_AUDIO_F32;
    spec.channels = 1;
    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(device, &spec, nullptr, nullptr);
    if (stream) SDL_ResumeAudioStreamDevice(stream);
    return stream;
}

static void RefreshRomList(std::vector<std::string>& rom_files, const std::string& dir) {
    rom_files.clear();
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return;
    for (auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".nes") {
            rom_files.push_back(entry.path().string());
        }
    }
}

// Bundles pointers to the main()-local state the async SDL dialog callbacks
// need to touch. SDL_ShowOpenFileDialog/SDL_ShowOpenFolderDialog take a
// plain C function pointer (no captures allowed), so this userdata struct
// is how they reach back into main()'s state instead.
struct DialogContext {
    Bus* bus;
    std::shared_ptr<Cartridge>* cart;
    AppState* state;
    AppConfig* config;
    std::vector<std::string>* rom_files;
    SDL_Window* window;
    std::string* current_rom_path;
};

static void SDLCALL OnRomFileChosen(void* userdata, const char* const* filelist, int filter) {
    (void)filter;
    auto* ctx = (DialogContext*)userdata;
    if (!filelist || !filelist[0]) return; // cancelled, or an error SDL already logged

    auto new_cart = std::make_shared<Cartridge>(filelist[0]);
    if (new_cart->imageValid()) {
        *ctx->cart = new_cart;
        ctx->bus->insertCartridge(*ctx->cart);
        ctx->bus->reset();
        *ctx->state = AppState::RUNNING;
        *ctx->current_rom_path = filelist[0];
        UpdateWindowTitle(ctx->window, *ctx->current_rom_path);
    }
}

static void SDLCALL OnRomFolderChosen(void* userdata, const char* const* filelist, int filter) {
    (void)filter;
    auto* ctx = (DialogContext*)userdata;
    if (!filelist || !filelist[0]) return;

    ctx->config->last_rom_dir = filelist[0];
    ctx->config->Save(CONFIG_PATH);
    RefreshRomList(*ctx->rom_files, ctx->config->last_rom_dir);
}

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    AppConfig config = AppConfig::Load(CONFIG_PATH);

    SDL_Window* window = SDL_CreateWindow(
        "femu",
        NES_WIDTH * config.window_scale, NES_HEIGHT * config.window_scale,
        SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 0); // we pace frames ourselves below

    // Keep the window (and its title bar) fully on-screen - see FitWindowToDisplay.
    SDL_SyncWindow(window); // let the frame size settle so borders report correctly
    FitWindowToDisplay(window, NES_WIDTH * config.window_scale, NES_HEIGHT * config.window_scale);

    SDL_Texture* texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, NES_WIDTH, NES_HEIGHT);
    if (!texture) {
        std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::vector<AudioDevice> audio_devices = EnumerateAudioDevices();
    SDL_AudioStream* audio_stream =
        OpenAudioStream(DeviceIdForName(audio_devices, config.audio_device));
    if (!audio_stream && !config.audio_device.empty()) {
        // The saved device is gone or refused - don't fail startup over it,
        // just fall back to the system default.
        std::fprintf(stderr, "Audio device \"%s\" unavailable, using default: %s\n",
                     config.audio_device.c_str(), SDL_GetError());
        config.audio_device.clear();
        audio_stream = OpenAudioStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK);
    }
    if (!audio_stream) {
        std::fprintf(stderr, "SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetAudioStreamGain(audio_stream, config.master_volume);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    Bus bus;
    std::shared_ptr<Cartridge> cart;
    std::string current_rom_path; // path of the loaded ROM, drives the window title
    AppState state = AppState::MENU;
    bool is_fullscreen = false;
    double overlay_hint_timer = 0.0; // counts down from OVERLAY_HINT_SECONDS after a fresh boot
    double save_ram_timer = 0.0;     // counts up; periodic save protects against crashes/force-quits

    std::string status_message;      // transient toast (savestate feedback)
    double status_message_timer = 0.0;
    auto set_status = [&](const std::string& msg) {
        status_message = msg;
        status_message_timer = STATUS_MESSAGE_SECONDS;
    };

    // Quick-save / quick-load the current game's default savestate slot
    // (<rom>.state, next to the ROM). Both require a game to be loaded - a
    // savestate is meaningless without a live machine to capture, or a
    // matching game to restore into - but "loaded" includes a game that's
    // merely paused behind the menu.
    auto quick_save_state = [&]() {
        if (!cart) return;
        std::string err = savestate::Save(bus, savestate::DefaultPath(current_rom_path));
        set_status(err.empty() ? "State saved" : ("Save failed: " + err));
    };
    auto quick_load_state = [&]() {
        if (!cart) return;
        std::string err = savestate::Load(bus, savestate::DefaultPath(current_rom_path));
        set_status(err.empty() ? "State loaded" : ("Load failed: " + err));
    };

    // Per-controller D-pad state, tracking press order across frames so
    // opposing directions can be resolved last-pressed-wins.
    DPadState dpad1, dpad2;

    // Smoothed peak level of the audio being sent to the device, 0..1, for
    // the volume meter in the Audio settings tab. Bumped up by each frame's
    // samples and decayed every frame so it falls back to zero on silence.
    float audio_level = 0.0f;

    // Switch the audio output to `device`, keeping the current volume. Leaves
    // the existing stream untouched if the new device can't be opened.
    auto reopen_audio = [&](SDL_AudioDeviceID device) {
        SDL_AudioStream* fresh = OpenAudioStream(device);
        if (!fresh) {
            std::fprintf(stderr, "Failed to open audio device: %s\n", SDL_GetError());
            return;
        }
        SDL_DestroyAudioStream(audio_stream);
        audio_stream = fresh;
        SDL_SetAudioStreamGain(audio_stream, config.master_volume);
    };

    // Optional: still support launching straight into a ROM via argv.
    if (argc >= 2) {
        auto initial = std::make_shared<Cartridge>(argv[1]);
        if (initial->imageValid()) {
            cart = initial;
            bus.insertCartridge(cart);
            bus.reset();
            state = AppState::RUNNING;
            current_rom_path = argv[1];
            UpdateWindowTitle(window, current_rom_path);
            overlay_hint_timer = OVERLAY_HINT_SECONDS;
        } else {
            std::fprintf(stderr, "Failed to load ROM from argv, opening menu instead: %s\n", argv[1]);
        }
    }

    std::vector<std::string> rom_files;
    RefreshRomList(rom_files, config.last_rom_dir);

    DialogContext dialog_ctx{ &bus, &cart, &state, &config, &rom_files, window, &current_rom_path };

    SDL_Scancode* currently_rebinding = nullptr; // points at whichever field is being captured, or null
    const char* button_names[8] = { "A", "B", "Select", "Start", "Up", "Down", "Left", "Right" };

    constexpr double NES_FPS = 60.0988;
    constexpr double TARGET_FRAME_SECONDS = 1.0 / NES_FPS;
    const Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 frame_start = SDL_GetPerformanceCounter();

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (currently_rebinding) {
                    *currently_rebinding = event.key.scancode;
                    currently_rebinding = nullptr;
                    config.Save(CONFIG_PATH);
                } else if (!event.key.repeat && event.key.scancode == config.hotkeys.fullscreen) {
                    is_fullscreen = !is_fullscreen;
                    SDL_SetWindowFullscreen(window, is_fullscreen);
                } else if (!event.key.repeat && config.hotkeys.save_state != SDL_SCANCODE_UNKNOWN
                           && event.key.scancode == config.hotkeys.save_state) {
                    quick_save_state();
                } else if (!event.key.repeat && config.hotkeys.load_state != SDL_SCANCODE_UNKNOWN
                           && event.key.scancode == config.hotkeys.load_state) {
                    quick_load_state();
                } else if (event.key.key == SDLK_ESCAPE) {
                    // Toggle, not "always go to menu": ESC opens the menu
                    // from gameplay, and - since that's the intuitive
                    // expectation - takes you right back to the game from
                    // the menu instead of needing a separate Resume button.
                    if (state == AppState::RUNNING) {
                        state = AppState::MENU;
                    } else if (cart) {
                        state = AppState::RUNNING;
                    }
                    // If no ROM is loaded yet, ESC in the menu does nothing -
                    // there's no game to return to, and quitting on ESC would
                    // be an easy accidental keystroke to regret.
                }
            }
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Decay the volume meter every frame; the running emulator bumps it
        // back up below. ~0.82/frame gives a springy-but-readable falloff.
        audio_level *= 0.82f;

        // Transient savestate toast, shown over both the game and the menu.
        if (status_message_timer > 0.0) {
            status_message_timer -= TARGET_FRAME_SECONDS;
            ImGui::SetNextWindowBgAlpha(0.45f);
            ImGui::SetNextWindowPos(ImVec2(8, 30));
            ImGui::Begin("##statustoast", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
            ImGui::TextUnformatted(status_message.c_str());
            ImGui::End();
        }

        if (state == AppState::MENU) {
            ImGui::SetNextWindowSize(ImVec2(560, 460), ImGuiCond_FirstUseEver);
            ImGui::Begin("femu");

            if (ImGui::BeginTabBar("MainTabs")) {
                if (ImGui::BeginTabItem("Games")) {
                    if (ImGui::Button("Browse for ROM...")) {
                        static const SDL_DialogFileFilter filters[] = { { "NES ROMs", "nes" } };
                        SDL_ShowOpenFileDialog(OnRomFileChosen, &dialog_ctx, window,
                                                filters, 1, config.last_rom_dir.c_str(), false);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Choose ROMs Folder...")) {
                        SDL_ShowOpenFolderDialog(OnRomFolderChosen, &dialog_ctx, window,
                                                  config.last_rom_dir.c_str(), false);
                    }

                    ImGui::Text("ROM folder: %s", config.last_rom_dir.c_str());
                    ImGui::SameLine();
                    if (ImGui::Button("Refresh")) {
                        RefreshRomList(rom_files, config.last_rom_dir);
                    }
                    ImGui::Separator();

                    if (rom_files.empty()) {
                        ImGui::TextDisabled("No .nes files found in this folder.");
                    }
                    for (auto& path : rom_files) {
                        std::string label = fs::path(path).filename().string();
                        if (ImGui::Selectable(label.c_str())) {
                            auto new_cart = std::make_shared<Cartridge>(path);
                            if (new_cart->imageValid()) {
                                cart = new_cart;
                                bus.insertCartridge(cart);
                                bus.reset();
                                state = AppState::RUNNING;
                                current_rom_path = path;
                                UpdateWindowTitle(window, current_rom_path);
                                overlay_hint_timer = OVERLAY_HINT_SECONDS;
                            }
                        }
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Display")) {
                    ImGui::Text("Window size:");
                    static const int scales[] = { 1, 2, 3, 4 };
                    for (int s : scales) {
                        char label[8];
                        std::snprintf(label, sizeof(label), "%dx", s);
                        if (ImGui::Button(label)) {
                            config.window_scale = s;
                            if (!is_fullscreen) {
                                FitWindowToDisplay(window, NES_WIDTH * s, NES_HEIGHT * s);
                            }
                            config.Save(CONFIG_PATH);
                        }
                        ImGui::SameLine();
                    }
                    ImGui::NewLine();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Audio")) {
                    float volume_pct = config.master_volume * 100.0f;
                    if (ImGui::SliderFloat("Volume", &volume_pct, 0.0f, 100.0f, "%.0f%%",
                                           ImGuiSliderFlags_AlwaysClamp)) {
                        config.master_volume = volume_pct / 100.0f;
                        SDL_SetAudioStreamGain(audio_stream, config.master_volume);
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        config.Save(CONFIG_PATH);
                    }

                    ImGui::Text("Output level");
                    ImGui::ProgressBar(audio_level, ImVec2(-FLT_MIN, 0), "");

                    ImGui::Separator();

                    // Re-enumerate while this tab is visible so devices that
                    // get plugged in / removed show up without a restart.
                    audio_devices = EnumerateAudioDevices();
                    const char* preview = config.audio_device.empty()
                        ? "System Default" : config.audio_device.c_str();
                    ImGui::Text("Output device");
                    ImGui::SameLine(120);
                    ImGui::SetNextItemWidth(320);
                    if (ImGui::BeginCombo("##audiodevice", preview)) {
                        if (ImGui::Selectable("System Default", config.audio_device.empty())) {
                            if (!config.audio_device.empty()) {
                                config.audio_device.clear();
                                reopen_audio(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK);
                                config.Save(CONFIG_PATH);
                            }
                        }
                        for (auto& d : audio_devices) {
                            bool selected = (config.audio_device == d.name);
                            if (ImGui::Selectable(d.name.c_str(), selected) && !selected) {
                                config.audio_device = d.name;
                                reopen_audio(d.id);
                                config.Save(CONFIG_PATH);
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Controls")) {
                    ImGui::TextDisabled("Click a box to rebind it; middle-click to unbind it.");
                    ImGui::Spacing();
                    ImGui::Text("Controller 1");
                    for (int i = 0; i < 8; i++) {
                        SDL_Scancode* targets1[8] = {
                            &config.keys.a, &config.keys.b, &config.keys.select, &config.keys.start,
                            &config.keys.up, &config.keys.down, &config.keys.left, &config.keys.right
                        };
                        ImGui::PushID(targets1[i]); // field's own address = a free unique ID
                        ImGui::Text("%s", button_names[i]);
                        ImGui::SameLine(120);
                        std::string btn_label = (currently_rebinding == targets1[i]) ? "press a key..."
                            : (*targets1[i] == SDL_SCANCODE_UNKNOWN) ? "(unbound)"
                            : SDL_GetScancodeName(*targets1[i]);
                        if (ImGui::Button(btn_label.c_str(), ImVec2(160, 0))) {
                            currently_rebinding = targets1[i];
                        }
                        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                            *targets1[i] = SDL_SCANCODE_UNKNOWN;
                            if (currently_rebinding == targets1[i]) currently_rebinding = nullptr;
                            config.Save(CONFIG_PATH);
                        }
                        ImGui::PopID();
                    }

                    ImGui::Separator();
                    ImGui::Checkbox("Enable Controller 2", &config.controller2_enabled);
                    if (config.controller2_enabled) {
                        ImGui::Text("Controller 2");
                        for (int i = 0; i < 8; i++) {
                            SDL_Scancode* targets2[8] = {
                                &config.keys2.a, &config.keys2.b, &config.keys2.select, &config.keys2.start,
                                &config.keys2.up, &config.keys2.down, &config.keys2.left, &config.keys2.right
                            };
                            ImGui::PushID(targets2[i]);
                            ImGui::Text("%s", button_names[i]);
                            ImGui::SameLine(120);
                            std::string btn_label = (currently_rebinding == targets2[i]) ? "press a key..."
                                : (*targets2[i] == SDL_SCANCODE_UNKNOWN) ? "(unbound)"
                                : SDL_GetScancodeName(*targets2[i]);
                            if (ImGui::Button(btn_label.c_str(), ImVec2(160, 0))) {
                                currently_rebinding = targets2[i];
                            }
                            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                                *targets2[i] = SDL_SCANCODE_UNKNOWN;
                                if (currently_rebinding == targets2[i]) currently_rebinding = nullptr;
                                config.Save(CONFIG_PATH);
                            }
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Hotkeys")) {
                    ImGui::TextDisabled("Click a box to rebind it; middle-click to unbind it.");
                    ImGui::Spacing();

                    auto hotkey_row = [&](const char* label, SDL_Scancode* target) {
                        ImGui::PushID(target);
                        ImGui::Text("%s", label);
                        ImGui::SameLine(120);
                        std::string lbl = (currently_rebinding == target) ? "press a key..."
                            : (*target == SDL_SCANCODE_UNKNOWN) ? "(unbound)"
                            : SDL_GetScancodeName(*target);
                        if (ImGui::Button(lbl.c_str(), ImVec2(160, 0))) {
                            currently_rebinding = target;
                        }
                        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                            *target = SDL_SCANCODE_UNKNOWN;
                            if (currently_rebinding == target) currently_rebinding = nullptr;
                            config.Save(CONFIG_PATH);
                        }
                        ImGui::PopID();
                    };

                    hotkey_row("Fullscreen", &config.hotkeys.fullscreen);
                    hotkey_row("Save State", &config.hotkeys.save_state);
                    hotkey_row("Load State", &config.hotkeys.load_state);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Savestates")) {
                    if (!cart) {
                        ImGui::TextWrapped(
                            "Load a game to save or load its states.\n\n"
                            "Savestates are written next to the ROM with a \".state\" "
                            "extension and can only be loaded back into the exact game "
                            "they were created from.");
                    } else {
                        ImGui::Text("Game: %s",
                                    fs::path(current_rom_path).stem().string().c_str());
                        if (ImGui::Button("Save State")) quick_save_state();
                        ImGui::SameLine();
                        ImGui::TextDisabled("(%s saves, %s loads)",
                            SDL_GetScancodeName(config.hotkeys.save_state),
                            SDL_GetScancodeName(config.hotkeys.load_state));

                        ImGui::Separator();
                        ImGui::TextUnformatted("Savestates for this game's folder:");
                        ImGui::Spacing();

                        fs::path dir = fs::path(current_rom_path).parent_path();
                        if (dir.empty()) dir = ".";
                        uint64_t current_hash = bus.RomHash();
                        std::error_code ec;
                        bool any = false;
                        for (auto& entry : fs::directory_iterator(dir, ec)) {
                            if (!entry.is_regular_file() ||
                                entry.path().extension() != ".state") continue;
                            any = true;

                            std::string path = entry.path().string();
                            savestate::Info info = savestate::Peek(path);
                            bool loadable = info.valid && info.rom_hash == current_hash;

                            ImGui::PushID(path.c_str());
                            ImGui::BeginDisabled(!loadable);
                            if (ImGui::Button("Load")) {
                                std::string err = savestate::Load(bus, path);
                                set_status(err.empty() ? "State loaded"
                                                       : ("Load failed: " + err));
                                if (err.empty()) state = AppState::RUNNING;
                            }
                            ImGui::EndDisabled();
                            ImGui::SameLine();

                            std::string name = entry.path().filename().string();
                            if (!info.valid) {
                                ImGui::Text("%s  -  not a femu savestate", name.c_str());
                            } else if (!loadable) {
                                ImGui::Text("%s  -  different game", name.c_str());
                            } else {
                                char when[32] = "unknown time";
                                std::time_t t = (std::time_t)info.saved_unix;
                                if (std::tm* lt = std::localtime(&t))
                                    std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M", lt);
                                ImGui::Text("%s  -  %s", name.c_str(), when);
                            }
                            ImGui::PopID();
                        }
                        if (!any)
                            ImGui::TextDisabled("No .state files in %s", dir.string().c_str());
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::End();
        } else {
            const bool* keys = SDL_GetKeyboardState(nullptr);
            bus.controller[0] = ReadController(keys, config.keys, dpad1);
            bus.controller[1] = config.controller2_enabled
                ? ReadController(keys, config.keys2, dpad2) : 0x00;

            do {
                bus.clock();
            } while (!bus.ppu.frame_complete);
            bus.ppu.frame_complete = false;

            if (!bus.audio_samples.empty()) {
                // APU output is unipolar ~0..0.75; scale the peak up so a
                // normal-loudness game lights most of the meter, then let the
                // stream gain fold in the user's volume setting.
                float peak = 0.0f;
                for (float s : bus.audio_samples) peak = std::max(peak, std::fabs(s));
                float level = std::min(1.0f, peak * 1.8f * config.master_volume);
                audio_level = std::max(audio_level, level);

                SDL_PutAudioStreamData(audio_stream, bus.audio_samples.data(),
                                       (int)(bus.audio_samples.size() * sizeof(float)));
                bus.audio_samples.clear();
            }

            if (overlay_hint_timer > 0.0) {
                overlay_hint_timer -= TARGET_FRAME_SECONDS;
                ImGui::SetNextWindowBgAlpha(0.35f);
                ImGui::SetNextWindowPos(ImVec2(8, 8));
                ImGui::Begin("##overlay", nullptr,
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::Text("ESC: Menu");
                ImGui::End();
            }

            // Periodic save-RAM flush - the destructor already saves on clean
            // exit or ROM switch, but this covers crashes/force-quits too.
            // The cartridge destructor no-ops for non-battery games, so this
            // is cheap to call even when it has nothing to actually persist.
            save_ram_timer += TARGET_FRAME_SECONDS;
            if (save_ram_timer >= 5.0) {
                save_ram_timer = 0.0;
                if (cart) cart->SaveRAM();
            }
        }

        ImGui::Render();

        int win_w = 0, win_h = 0;
        SDL_GetWindowSize(window, &win_w, &win_h);

        SDL_RenderClear(renderer);
        if (state == AppState::RUNNING) {
            SDL_UpdateTexture(texture, nullptr, bus.ppu.framebuffer.data(), NES_WIDTH * 3);
            SDL_FRect dst = ComputeDestRect(win_w, win_h);
            SDL_RenderTexture(renderer, texture, nullptr, &dst);
        }
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);

        // Pace every iteration (menu included) so an idle menu doesn't peg a
        // CPU core at 100% redrawing as fast as possible.
        Uint64 now = SDL_GetPerformanceCounter();
        double elapsed = (double)(now - frame_start) / (double)perf_freq;
        double remaining = TARGET_FRAME_SECONDS - elapsed;
        if (remaining > 0.0) {
            if (remaining > 0.002) {
                SDL_Delay((Uint32)((remaining - 0.001) * 1000.0));
            }
            do {
                now = SDL_GetPerformanceCounter();
                elapsed = (double)(now - frame_start) / (double)perf_freq;
            } while (elapsed < TARGET_FRAME_SECONDS);
        }
        frame_start = SDL_GetPerformanceCounter();
    }

    config.Save(CONFIG_PATH);

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyAudioStream(audio_stream);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
