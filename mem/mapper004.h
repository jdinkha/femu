#pragma once
#include "mapper.h"
#include <array>

// MMC3 (mapper 4): 8KB PRG windows, 1-2KB CHR windows, and a scanline-
// counting IRQ used for split-screen effects and status bars. Covers Mega
// Man 3-6, Kirby's Adventure, Super Mario Bros. 3, and a huge fraction of
// the later NES library.
class Mapper_004 : public Mapper {
public:
    Mapper_004(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
    ~Mapper_004() override = default;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;
    Mirror mirror() const override { return mirror_mode; }
    bool prgRamEnabled() const override { return prg_ram_enabled; }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

    bool irqState() const override { return irq_pending; }
    void irqClear() override { irq_pending = false; }
    void ScanlineIRQ() override;

private:
    uint8_t target_register = 0;
    bool prg_bank_mode = false; // bank_select bit 6
    bool chr_a12_invert = false; // bank_select bit 7

    std::array<uint8_t, 8> reg{}; // R0-R7, raw values as written to $8001

    uint8_t chr_bank_1k[8] = {0}; // resolved 1KB CHR bank indices for all 8 windows
    uint8_t prg_bank_8k[4] = {0}; // resolved 8KB PRG bank indices for all 4 windows

    Mirror mirror_mode = Mirror::HORIZONTAL;
    bool prg_ram_enabled = true;

    bool irq_enabled = false;
    bool irq_reload = false;
    uint8_t irq_latch = 0;
    uint8_t irq_counter = 0;
    bool irq_pending = false;

    void UpdateBanks();
};
