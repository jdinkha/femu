#pragma once
#include "banked_mapper.h"

// Sunsoft-1 (mapper 184): Atlantis no Nazo, The Wing of Madoola. Fixed PRG
// and a register at $6000-$7FFF (so no PRG-RAM) selecting two 4KB CHR
// banks - bits 0-2 for $0000, bits 4-5 for $1000, where the chip forces the
// bank into the upper half (4-7).
class Mapper_184 : public BankedMapper {
public:
    Mapper_184(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        prg_ram_size = 0;
        SetPrg32k(0);
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        if (addr < 0x6000 || addr > 0x7FFF) return false;
        SetChr4k(0, data & 0x07);
        SetChr4k(1, 0x04 | ((data >> 4) & 0x03));
        return true;
    }
};
