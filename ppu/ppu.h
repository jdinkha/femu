#pragma once
#include <cstdint>
#include <array>
#include <memory>
#include "../mem/cartridge.h"

// 2C02 Picture Processing Unit.
//
// This first pass implements: register interface, VRAM/palette/OAM memory,
// full scanline/cycle timing (262 x 341), NMI generation at vblank, and
// background tile rendering (with scrolling - the loopy v/t/x registers
// handle that "for free" since that's how real hardware does it too).
//
// NOT yet implemented: sprite rendering (OAM evaluation, sprite shifters,
// sprite-0 hit, OAMDMA at $4014). That's the natural next milestone -
// everything here is structured so adding it doesn't require touching the
// background pipeline.
class PPU {
public:
    PPU();
    ~PPU() = default;

    void ConnectCartridge(const std::shared_ptr<Cartridge>& cart);
    void reset();
    void clock();

    // CPU-facing register interface. addr is already demirrored to 0-7 by Bus.
    uint8_t cpuRead(uint16_t addr, bool readOnly = false);
    void    cpuWrite(uint16_t addr, uint8_t data);

    bool frame_complete = false;
    bool nmi = false; // Bus polls this every system clock and fires cpu.nmi() when set

    // 256x240, 3 bytes/pixel (RGB), row-major - ready to hand straight to an
    // SDL streaming texture with SDL_PIXELFORMAT_RGB24.
    std::array<uint8_t, 256 * 240 * 3> framebuffer{};

private:
    std::shared_ptr<Cartridge> cart;

    // PPU-facing bus: pattern tables via the cartridge/mapper, nametables
    // and palette live here.
    uint8_t ppuRead(uint16_t addr);
    void    ppuWrite(uint16_t addr, uint8_t data);

    std::array<std::array<uint8_t, 1024>, 2> nameTable{};
    std::array<uint8_t, 32> paletteTable{};

    // ---- Registers ----
    union PPUCTRL {
        struct {
            uint8_t nametable_x : 1;
            uint8_t nametable_y : 1;
            uint8_t increment_mode : 1;
            uint8_t pattern_sprite : 1;
            uint8_t pattern_background : 1;
            uint8_t sprite_size : 1;
            uint8_t slave_mode : 1;
            uint8_t enable_nmi : 1;
        };
        uint8_t reg = 0x00;
    } control;

    union PPUMASK {
        struct {
            uint8_t grayscale : 1;
            uint8_t render_background_left : 1;
            uint8_t render_sprites_left : 1;
            uint8_t render_background : 1;
            uint8_t render_sprites : 1;
            uint8_t enhance_red : 1;
            uint8_t enhance_green : 1;
            uint8_t enhance_blue : 1;
        };
        uint8_t reg = 0x00;
    } mask;

    union PPUSTATUS {
        struct {
            uint8_t unused : 5;
            uint8_t sprite_overflow : 1;
            uint8_t sprite_zero_hit : 1;
            uint8_t vertical_blank : 1;
        };
        uint8_t reg = 0x00;
    } status;

    // "Loopy" scroll registers - the real 6502-adjacent scroll/address logic
    // Nintendo's engineers came up with. v is the current VRAM address (also
    // used for rendering), t is the "staging" address PPUSCROLL/PPUADDR
    // write into, x is fine-x scroll (3 bits), w is the shared write toggle.
    union LoopyRegister {
        struct {
            uint16_t coarse_x : 5;
            uint16_t coarse_y : 5;
            uint16_t nametable_x : 1;
            uint16_t nametable_y : 1;
            uint16_t fine_y : 3;
            uint16_t unused : 1;
        };
        uint16_t reg = 0x0000;
    };
    LoopyRegister vram_addr; // v
    LoopyRegister tram_addr; // t
    uint8_t fine_x = 0x00;   // x
    bool address_latch = false; // w

    uint8_t ppu_data_buffer = 0x00; // $2007 reads are delayed by one, except palette reads
    uint8_t oam_addr = 0x00;
    std::array<uint8_t, 256> OAM{}; // 64 sprites x 4 bytes (Y, tile, attribute, X)

public:
    // Called by Bus on a $4014 (OAMDMA) write. Copies one byte into OAM.
    // NOTE: real hardware stalls the CPU for ~513-514 cycles during the full
    // 256-byte transfer; Bus currently does the whole copy in one go rather
    // than modeling that stall. Fine for correctness, not cycle-accurate.
    void WriteOAMByte(uint8_t index, uint8_t value) { OAM[index] = value; }

private:
    // ---- Background rendering pipeline ----
    int16_t scanline = -1; // -1 = pre-render line
    int16_t cycle = 0;

    uint8_t bg_next_tile_id = 0x00;
    uint8_t bg_next_tile_attrib = 0x00;
    uint8_t bg_next_tile_lsb = 0x00;
    uint8_t bg_next_tile_msb = 0x00;

    uint16_t bg_shifter_pattern_lo = 0x0000;
    uint16_t bg_shifter_pattern_hi = 0x0000;
    uint16_t bg_shifter_attrib_lo = 0x0000;
    uint16_t bg_shifter_attrib_hi = 0x0000;

    void IncrementScrollX();
    void IncrementScrollY();
    void TransferAddressX();
    void TransferAddressY();
    void LoadBackgroundShifters();
    void UpdateShifters();

    // ---- Sprite (foreground) rendering pipeline ----
    struct SpriteEntry { uint8_t y = 0xFF, id = 0xFF, attribute = 0xFF, x = 0xFF; };
    std::array<SpriteEntry, 8> spriteScanline{}; // up to 8 sprites found for the current scanline
    uint8_t sprite_count = 0;

    std::array<uint8_t, 8> sprite_shifter_pattern_lo{};
    std::array<uint8_t, 8> sprite_shifter_pattern_hi{};

    bool bSpriteZeroHitPossible = false;
    bool bSpriteZeroBeingRendered = false;

    void EvaluateSpritesForScanline();
    void LoadSpriteShifters();

    uint8_t GetColorFromPaletteRam(uint8_t palette, uint8_t pixel);
    void    SetPixel(int x, int y, uint8_t colorIndex);

    // NES master palette: 64 entries, RGB. (Common "FCEUX-style" values -
    // swap in a different reference palette later if you want a different
    // color grade; it's a pure lookup table, nothing else depends on it.)
    static const std::array<uint8_t, 64 * 3> paletteRGB;
};
