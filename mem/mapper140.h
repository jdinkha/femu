#pragma once
#include "banked_mapper.h"

// Jaleco JF-11/JF-14 (mapper 140): GxROM-like, with the register at
// $6000-$7FFF (so no PRG-RAM) - a 32KB PRG bank in bits 4-5 and an 8KB CHR
// bank in bits 0-3. Bio Senshi Dan, Mississippi Satsujin Jiken.
class Mapper_140 : public BankedMapper {
public:
    Mapper_140(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        prg_ram_size = 0;
        SetPrg32k(0);
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        if (addr < 0x6000 || addr > 0x7FFF) return false;
        SetPrg32k((data >> 4) & 0x03);
        SetChr8k(data & 0x0F);
        return true;
    }
};
