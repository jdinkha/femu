#pragma once
#include "banked_mapper.h"

// Jaleco/Konami J87 (mapper 87): fixed PRG, and an 8KB CHR bank register at
// $6000-$7FFF whose two bits are wired swapped (bit 0 is the high bank
// bit). Argus, City Connection, The Goonies (J).
class Mapper_087 : public BankedMapper {
public:
    Mapper_087(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        prg_ram_size = 0;
        SetPrg32k(0);
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        if (addr < 0x6000 || addr > 0x7FFF) return false;
        SetChr8k(((data & 0x01) << 1) | ((data >> 1) & 0x01));
        return true;
    }
};
