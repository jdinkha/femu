#pragma once
#include "mapper.h"
#include <array>

// MMC3 (mapper 4): 8KB PRG windows, 1-2KB CHR windows, and a scanline-
// counting IRQ used for split-screen effects and status bars. Covers Mega
// Man 3-6, Kirby's Adventure, Super Mario Bros. 3, and a huge fraction of
// the later NES library.
//
// Boards that wire the MMC3's bank outputs differently (multicarts with an
// outer bank, TQROM's CHR-RAM select, TxSROM's nametable select...) derive
// from this and override PrgBank/ChrBank and the other hooks.
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

protected:
    uint8_t target_register = 0;
    bool prg_bank_mode = false; // bank_select bit 6
    bool chr_a12_invert = false; // bank_select bit 7

    std::array<uint8_t, 8> reg{}; // R0-R7, raw values as written to $8001

    // What the MMC3 itself puts on its bank outputs for each window -
    // register values, or $FE/$FF for the fixed second-last/last PRG banks -
    // before the board's wiring turns them into real banks.
    uint8_t prg_raw[4] = {0};
    uint8_t chr_raw[8] = {0};

    Mirror mirror_mode = Mirror::HORIZONTAL;
    bool prg_ram_enabled = true;
    bool prg_ram_writable = true; // $A001 bit 6 clear

    // Turn an MMC3 bank output into a byte offset into PRG / CHR (CHR-ROM,
    // then CHR-RAM). Defaults: plain TxROM wiring. ChrWritable says whether
    // a CHR bank is RAM.
    virtual uint32_t PrgBank(uint8_t raw) const;
    virtual uint32_t ChrBank(uint8_t raw) const;
    virtual bool ChrWritable(uint8_t raw) const { (void)raw; return nCHRBanks == 0; }

    void UpdateBanks();

private:
    uint32_t chr_offset[8] = {0}; // resolved byte offsets for all 8 CHR windows
    uint32_t prg_offset[4] = {0}; // resolved byte offsets for all 4 PRG windows
    bool chr_writable[8] = {false};

    bool irq_enabled = false;
    bool irq_reload = false;
    uint8_t irq_latch = 0;
    uint8_t irq_counter = 0;
    bool irq_pending = false;
};
