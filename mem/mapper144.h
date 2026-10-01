#pragma once
#include "banked_mapper.h"

// AGCI 50282 (mapper 144, Death Race): a Color Dreams board with a
// deliberately weakened data line, so bit 0 of every register write comes
// from the ROM alone: effective = ROM[addr] & (written | 1).
class Mapper_144 : public BankedMapper {
public:
    Mapper_144(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        SetPrg32k(0);
    }

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        data = BusConflict(addr, data | 0x01);
        SetPrg32k(data & 0x03);
        SetChr8k(data >> 4);
    }
};
