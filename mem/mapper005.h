#pragma once
#include "mapper.h"
#include <array>

// Nintendo MMC5 (mapper 5): Castlevania III, Just Breed, Uncharted Waters,
// the Koei strategy games, Metal Slader Glory. Registers at $5000-$5FFF:
//   $5000-$5015  expansion audio: two pulse channels and 8-bit PCM
//   $5100 PRG mode (32/16+16/16+8+8/8x4 KB)  $5101 CHR mode (8/4/2/1 KB)
//   $5102/$5103  PRG-RAM write protection ($02 and $01 unlock it)
//   $5104 ExRAM mode: 0 nametable, 1 extended attributes, 2 RAM, 3 ROM
//   $5105 per-quadrant nametable: CIRAM A, CIRAM B, ExRAM, fill mode
//   $5106/$5107  fill-mode tile and palette
//   $5113-$5117  PRG banks ($5113 RAM at $6000; bit 7 picks ROM over RAM
//                for $5114-$5116; $5117 always ROM)
//   $5120-$512B  CHR banks: $5120-7 for sprites (and everything with 8x8
//                sprites), $5128-B for the background with 8x16 sprites
//   $5130 upper CHR bits   $5200-$5202 vertical split
//   $5203 IRQ scanline     $5204 IRQ enable / status
//   $5205/$5206  8x8 multiplier   $5C00-$5FFF ExRAM
// It also watches PPUCTRL/PPUMASK writes for 8x16 sprites and rendering.
//
// Scanline detection follows the documented behavior (the IRQ fires at
// dot 4 of the target scanline); femu gets it from the PPU's per-scanline
// hook rather than by watching nametable fetches. PRG-RAM defaults to the
// 64KB superset that runs every game.
class Mapper_005 : public Mapper {
public:
    Mapper_005(uint8_t prgBanks, uint8_t chrBanks);

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    bool cpuReadHook(uint16_t addr, uint8_t& data) override;
    bool cpuWriteHook(uint16_t addr, uint8_t data) override;
    bool prgRamMap(uint16_t addr, uint32_t& offset) override;

    bool ppuReadHook(uint16_t addr, uint8_t& data) override;
    bool ppuWriteHook(uint16_t addr, uint8_t data) override;
    int nametablePage(uint8_t quadrant) const override;
    void ppuScanline(int16_t scanline, bool rendering) override;
    void ppuSpriteFetch(bool active) override {
        sprite_fetch = active;
        if (!active) nt_fetches = 0; // the next background fetch is tile 2 of a new line
    }

    bool irqState() const override {
        return (irq_pending && irq_enabled) || (pcm_irq && (pcm_mode & 0x80));
    }
    void cpuClock() override { AudioClock(); }
    float audioSample() const override { return audio_out; }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

private:
    // ---- Banking ----
    uint8_t prg_mode = 3, chr_mode = 3;
    uint8_t ram_protect[2] = {1, 2};
    uint8_t exram_mode = 3;
    uint8_t nt_mapping = 0;
    uint8_t fill_tile = 0, fill_attr = 0;
    uint8_t prg_regs[5] = {0, 0, 0, 0, 0xFF};   // $5113-$5117
    uint16_t chr_regs[12] = {0};                 // $5120-$512B, as the chip's 10-bit registers
    uint8_t chr_upper = 0;                       // $5130
    bool last_set_b = false;                     // last CHR register written was $5128-$512B
    std::array<uint8_t, 0x400> exram{};

    // ---- What the PPU is doing ----
    bool sprite_8x16 = false;
    bool substitutions = false;                  // PPUMASK shows rendering on
    bool sprite_fetch = false;
    int16_t scanline = 0;
    uint8_t exattr = 0;                          // ExRAM byte for the tile being fetched
    int nt_fetches = 0;                          // background tile fetches since the sprite fetch
    bool in_split = false;                       // the tile being fetched is in the split region
    uint8_t split_column = 0, split_y = 0;       // its column and split-region line

    // ---- Split screen ----
    uint8_t split_control = 0, split_scroll = 0, split_bank = 0;

    // ---- IRQ, multiplier ----
    uint8_t irq_compare = 0;
    bool irq_enabled = false, irq_pending = false, in_frame = false;
    uint8_t irq_counter = 0;
    uint8_t mult_a = 0xFF, mult_b = 0xFF;

    // ---- Audio ----
    struct Pulse {
        uint8_t control = 0;        // DDLC VVVV
        uint16_t period = 0, timer = 0;
        uint8_t step = 0;
        uint8_t length = 0;
        bool enabled = false;
        bool env_start = false;
        uint8_t env_divider = 0, env_decay = 0;
    } pulse[2];
    uint8_t pcm_mode = 0, pcm_value = 0;
    bool pcm_irq = false;
    uint16_t frame_timer = 0;
    bool apu_phase = false;
    float audio_out = 0.0f;

    bool PrgTarget(uint16_t addr, bool& is_ram, uint32_t& offset) const;
    bool Rendering() const { return in_frame && substitutions; }
    bool ExtendedAttributes() const { return exram_mode == 1 && substitutions; }
    uint32_t ChrOffset(uint16_t addr) const;
    void WriteChr(int index, uint8_t data);
    void AudioWrite(uint16_t addr, uint8_t data);
    void AudioClock();
};
