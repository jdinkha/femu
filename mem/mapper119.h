#pragma once
#include "mapper004.h"

// TQROM (mapper 119): an MMC3 board with both 64KB CHR-ROM and 8KB CHR-RAM;
// bit 6 of each CHR bank number picks RAM over ROM. High Speed, Pin*Bot.
class Mapper_119 : public Mapper_004 {
public:
    Mapper_119(uint8_t prgBanks, uint8_t chrBanks) : Mapper_004(prgBanks, chrBanks) {
        chr_ram_size = 0x2000;
        UpdateBanks();
    }

protected:
    uint32_t ChrBank(uint8_t raw) const override {
        if (raw & 0x40) return (uint32_t)nCHRBanks * 0x2000 + (raw & 0x07) * 0x400u;
        return Mapper_004::ChrBank(raw & 0x3F);
    }
    bool ChrWritable(uint8_t raw) const override { return (raw & 0x40) != 0; }
};
