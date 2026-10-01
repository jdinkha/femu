#pragma once
#include "banked_mapper.h"

// AVE NINA-03/NINA-06 (mapper 79) and Sachen 3015/SA-016 (mapper 146): one
// register at $4100-$5FFF (decoded where A8 is set) selecting a 32KB PRG
// bank (bit 3) and an 8KB CHR bank (bits 0-2). Deathbots, Impossible
// Mission II (US), Krazy Kreatures, Pyramid.
class Mapper_079 : public BankedMapper {
public:
    Mapper_079(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        SetPrg32k(0);
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        if ((addr & 0xE100) != 0x4100) return false;
        SetPrg32k((data >> 3) & 0x01);
        SetChr8k(data & 0x07);
        return true;
    }
};
