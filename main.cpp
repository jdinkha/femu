#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

#include "mem/bus.h"
#include "ui/config.h"

namespace fs = std::filesystem;

static constexpr int NES_WIDTH = 256;
static constexpr int NES_HEIGHT = 240;
static constexpr const char* CONFIG_PATH = "femu_config.txt";
static constexpr double OVERLAY_HINT_SECONDS = 5.0;

enum class AppState { MENU, RUNNING };

static uint8_t ReadController(const bool* keys, const KeyBindings& kb) {
    uint8_t c = 0;
    if (keys[kb.a])      c |= 0x80;
    if (keys[kb.b])      c |= 0x40;
    if (keys[kb.select]) c |= 0x20;
    if (keys[kb.start])  c |= 0x10;
    if (keys[kb.up])     c |= 0x08;
    if (keys[kb.down])   c |= 0x04;
    if (keys[kb.left])   c |= 0x02;
    if (keys[kb.right])  c |= 0x01;
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

    SDL_Texture* texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, NES_WIDTH, NES_HEIGHT);
    if (!texture) {
        std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_AudioSpec audio_spec{};
    audio_spec.freq = 44100;
    audio_spec.format = SDL_AUDIO_F32;
    audio_spec.channels = 1;
    SDL_AudioStream* audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec, nullptr, nullptr);
    if (!audio_stream) {
        std::fprintf(stderr, "SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_ResumeAudioStreamDevice(audio_stream);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    Bus bus;
    std::shared_ptr<Cartridge> cart;
    AppState state = AppState::MENU;
    bool is_fullscreen = false;
    double overlay_hint_timer = 0.0; // counts down from OVERLAY_HINT_SECONDS after a fresh boot

    // Optional: still support launching straight into a ROM via argv.
    if (argc >= 2) {
        auto initial = std::make_shared<Cartridge>(argv[1]);
        if (initial->imageValid()) {
            cart = initial;
            bus.insertCartridge(cart);
            bus.reset();
            state = AppState::RUNNING;
            overlay_hint_timer = OVERLAY_HINT_SECONDS;
        } else {
            std::fprintf(stderr, "Failed to load ROM from argv, opening menu instead: %s\n", argv[1]);
        }
    }

    std::vector<std::string> rom_files;
    RefreshRomList(rom_files, config.last_rom_dir);

    DialogContext dialog_ctx{ &bus, &cart, &state, &config, &rom_files };

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
                                SDL_SetWindowSize(window, NES_WIDTH * s, NES_HEIGHT * s);
                            }
                            config.Save(CONFIG_PATH);
                        }
                        ImGui::SameLine();
                    }
                    ImGui::NewLine();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Controls")) {
                    ImGui::Text("Controller 1");
                    for (int i = 0; i < 8; i++) {
                        SDL_Scancode* targets1[8] = {
                            &config.keys.a, &config.keys.b, &config.keys.select, &config.keys.start,
                            &config.keys.up, &config.keys.down, &config.keys.left, &config.keys.right
                        };
                        ImGui::PushID(targets1[i]); // field's own address = a free unique ID
                        ImGui::Text("%s", button_names[i]);
                        ImGui::SameLine(120);
                        std::string btn_label = (currently_rebinding == targets1[i])
                            ? "press a key..." : SDL_GetScancodeName(*targets1[i]);
                        if (ImGui::Button(btn_label.c_str(), ImVec2(160, 0))) {
                            currently_rebinding = targets1[i];
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
                            std::string btn_label = (currently_rebinding == targets2[i])
                                ? "press a key..." : SDL_GetScancodeName(*targets2[i]);
                            if (ImGui::Button(btn_label.c_str(), ImVec2(160, 0))) {
                                currently_rebinding = targets2[i];
                            }
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Hotkeys")) {
                    // Save state / load state / volume are natural next
                    // additions here once those features exist - each is
                    // just one more field in HotkeyBindings plus one more
                    // row below, same pattern as Fullscreen.
                    ImGui::PushID(&config.hotkeys.fullscreen);
                    ImGui::Text("Fullscreen");
                    ImGui::SameLine(120);
                    std::string fs_label = (currently_rebinding == &config.hotkeys.fullscreen)
                        ? "press a key..." : SDL_GetScancodeName(config.hotkeys.fullscreen);
                    if (ImGui::Button(fs_label.c_str(), ImVec2(160, 0))) {
                        currently_rebinding = &config.hotkeys.fullscreen;
                    }
                    ImGui::PopID();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::End();
        } else {
            const bool* keys = SDL_GetKeyboardState(nullptr);
            bus.controller[0] = ReadController(keys, config.keys);
            bus.controller[1] = config.controller2_enabled ? ReadController(keys, config.keys2) : 0x00;

            do {
                bus.clock();
            } while (!bus.ppu.frame_complete);
            bus.ppu.frame_complete = false;

            if (!bus.audio_samples.empty()) {
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
