#include <SDL3/SDL.h>
#include <cstdio>
#include <memory>
#include "mem/bus.h"

static constexpr int NES_WIDTH = 256;
static constexpr int NES_HEIGHT = 240;
static constexpr int SCALE = 3;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <rom.nes>\n", argv[0]);
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "femu",
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

    Bus bus;
    auto cart = std::make_shared<Cartridge>(argv[1]);
    if (!cart->imageValid()) {
        std::fprintf(stderr, "Failed to load ROM (bad file, or an unsupported mapper): %s\n", argv[1]);
        return 1;
    }
    bus.insertCartridge(cart);
    bus.reset();

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

        // Advance the system until the PPU finishes a full frame. This is
        // the entire "run one frame" loop - CPU, PPU (and eventually APU)
        // all fall out of just calling the master clock enough times.
        do {
            bus.clock();
        } while (!bus.ppu.frame_complete);
        bus.ppu.frame_complete = false;

        SDL_UpdateTexture(texture, nullptr, bus.ppu.framebuffer.data(), NES_WIDTH * 3);

        SDL_RenderClear(renderer);
        SDL_FRect dst{ 0.0f, 0.0f, (float)(NES_WIDTH * SCALE), (float)(NES_HEIGHT * SCALE) };
        SDL_RenderTexture(renderer, texture, nullptr, &dst);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
