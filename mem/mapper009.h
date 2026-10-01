#pragma once
#include "banked_mapper.h"

// MMC2 (mapper 9, Punch-Out!!) and MMC4 (mapper 10, Fire Emblem, Famicom
// Wars). Each 4KB half of the pattern tables has two CHR banks, and the PPU
// itself picks between them: fetching the pattern of tile $FD or $FE flips
// that half's latch, so a game can switch banks mid-screen just by placing
// those tiles. The latch changes after the fetch, so $FD/$FE themselves
// still draw from the old bank.
//   $A000 PRG bank: MMC2 8KB at $8000 (last three 8KB fixed),
//                   MMC4 16KB at $8000 (last 16KB fixed)
//   $B000/$C000  4KB CHR bank for $0000 when latch 0 is $FD/$FE
//   $D000/$E000  4KB CHR bank for $1000 when latch 1 is $FD/$FE
//   $F000 bit 0  mirroring (0 = vertical)
// Latch 0 triggers at $0FD8/$0FE8 exactly on the MMC2 (so only on a tile's
// top row) but on all of $0FD8-$0FDF/$0FE8-$0FEF on the MMC4; latch 1 uses
// the full ranges on both.
class Mapper_009 : public BankedMapper {
public:
    Mapper_009(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId);

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override;

private:
    bool mmc4;
    uint8_t chr[4] = {0, 0, 0, 0}; // $B000, $C000, $D000, $E000
    uint8_t latch[2] = {0xFE, 0xFE};

    void UpdateChr();
};
