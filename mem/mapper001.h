#pragma once
#include "mapper.h"

// MMC1 (mapper 1): a serial-shift-register mapper. Every CPU write to
// $8000-$FFFF shifts one bit (the write's LSB) into a 5-bit register; on
// the 5th write, the accumulated value is committed to one of four internal
// registers, chosen by which address range the completing write landed in.
// A write with bit 7 set is a reset, not a shift.
//
// Covers Zelda, Metroid, Mega Man 2, Final Fantasy, and a large chunk of
// the mid-generation NES library.
class Mapper_001 : public Mapper {
public:
    Mapper_001(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
    ~Mapper_001() override = default;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;

    void reset() override;
    Mirror mirror() const override { return mirror_mode; }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

private:
    uint8_t load_register = 0x00;
    uint8_t load_register_count = 0;

    uint8_t control_register = 0x1C; // power-on default: PRG mode 3 (16KB, fix last bank), CHR mode 0
    uint8_t chr_bank_select_4lo = 0;
    uint8_t chr_bank_select_4hi = 0;
    uint8_t chr_bank_select_8 = 0;
    uint8_t prg_bank_select_16 = 0; // whichever window is switchable in modes 2/3 (fixed side computed directly, not stored)
    uint8_t prg_bank_select_32 = 0;

    Mirror mirror_mode = Mirror::HORIZONTAL;
};
