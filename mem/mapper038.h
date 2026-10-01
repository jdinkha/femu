#pragma once
#include "banked_mapper.h"

// Bit Corp. PCI556 (mapper 38, Crime Busters): GxROM-style banking with the
// register moved to $7000-$7FFF - bits 0-1 pick a 32KB PRG bank, bits 2-3
// an 8KB CHR bank.
class Mapper_038 : public BankedMapper {
public:
    Mapper_038(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        SetPrg32k(0);
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        if (addr < 0x7000 || addr > 0x7FFF) return false;
        SetPrg32k(data & 0x03);
        SetChr8k((data >> 2) & 0x03);
        return true;
    }
};
