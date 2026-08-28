#include <SDL3/SDL.h>
#include <cstdio>
#include <memory>
#include "mem/bus.h"

static constexpr int NES_WIDTH = 256;
static constexpr int NES_HEIGHT = 240;
static constexpr int SCALE = 3;

// Hardcoded key bindings (no rebinding/config system yet - these are the
// only place the mapping lives, change them here if you want different keys).
// Bit layout matches Bus::controller[]: bit7=A bit6=B bit5=Select bit4=Start
//                                        bit3=Up bit2=Down bit1=Left bit0=Right
static uint8_t ReadController(const bool* keys) {
    uint8_t c = 0;
    if (keys[SDL_SCANCODE_Z])      c |= 0x80; // A
    if (keys[SDL_SCANCODE_X])      c |= 0x40; // B
    if (keys[SDL_SCANCODE_RSHIFT]) c |= 0x20; // Select
    if (keys[SDL_SCANCODE_RETURN]) c |= 0x10; // Start
    if (keys[SDL_SCANCODE_UP])     c |= 0x08;
    if (keys[SDL_SCANCODE_DOWN])   c |= 0x04;
    if (keys[SDL_SCANCODE_LEFT])   c |= 0x02;
    if (keys[SDL_SCANCODE_RIGHT])  c |= 0x01;
    return c;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <rom.nes>\n", argv[0]);
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "NES Emulator",
        NES_WIDTH * SCALE, NES_HEIGHT * SCALE,
        0
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
    SDL_SetRenderVSync(renderer, 0); // we pace frames ourselves below; vsync would fight with that

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        NES_WIDTH, NES_HEIGHT
    );
    if (!texture) {
        std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // --- Audio: mono float, 44.1kHz, matching Bus's resampling target ---
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

    Bus bus;
    auto cart = std::make_shared<Cartridge>(argv[1]);
    if (!cart->imageValid()) {
        std::fprintf(stderr, "Failed to load ROM (bad file, or an unsupported mapper): %s\n", argv[1]);
        return 1;
    }
    bus.insertCartridge(cart);
    bus.reset();

    // Frame pacing: nothing above this point limits how fast we can churn
    // through emulated frames, so without this the loop runs as fast as the
    // CPU allows (hundreds of fps) - audio doesn't have this problem because
    // the audio *device* only drains samples at a fixed 44.1kHz regardless
    // of how fast we push them, but video has no equivalent hardware limit
    // on our end. We deliberately time against a real clock rather than
    // relying on vsync, since vsync ties to the display's refresh rate
    // (60Hz, 144Hz, whatever) rather than the NES's specific ~60.0988Hz.
    constexpr double NES_FPS = 60.0988;
    constexpr double TARGET_FRAME_SECONDS = 1.0 / NES_FPS;
    const Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 frame_start = SDL_GetPerformanceCounter();

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                running = false;
            }
        }

        // Poll the whole keyboard state once per frame rather than tracking
        // individual key up/down events - simpler, and correct for "is this
        // held right now" which is all a D-pad/buttons need.
        const bool* keys = SDL_GetKeyboardState(nullptr);
        bus.controller[0] = ReadController(keys);

        // Advance the system until the PPU finishes a full frame. CPU, PPU,
        // and now APU sample generation all fall out of just calling the
        // master clock enough times.
        do {
            bus.clock();
        } while (!bus.ppu.frame_complete);
        bus.ppu.frame_complete = false;

        SDL_UpdateTexture(texture, nullptr, bus.ppu.framebuffer.data(), NES_WIDTH * 3);

        SDL_RenderClear(renderer);
        SDL_FRect dst{ 0.0f, 0.0f, (float)(NES_WIDTH * SCALE), (float)(NES_HEIGHT * SCALE) };
        SDL_RenderTexture(renderer, texture, nullptr, &dst);
        SDL_RenderPresent(renderer);

        if (!bus.audio_samples.empty()) {
            SDL_PutAudioStreamData(audio_stream, bus.audio_samples.data(),
                                   (int)(bus.audio_samples.size() * sizeof(float)));
            bus.audio_samples.clear();
        }

        // Wait out whatever's left of this frame's time budget. SDL_Delay
        // has coarse (~1ms) granularity, so we sleep for most of the
        // remaining time and busy-spin the last sliver for accuracy -
        // standard technique to avoid oversleeping past the target.
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

    SDL_DestroyAudioStream(audio_stream);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
