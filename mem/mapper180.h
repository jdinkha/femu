#pragma once
#include "banked_mapper.h"

// UNROM wired with an AND gate (mapper 180, Crazy Climber): the FIRST 16KB
// is fixed at $8000 and $8000-$FFFF writes switch $C000 (bits 0-2).
class Mapper_180 : public BankedMapper {
public:
    Mapper_180(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        SetPrg16k(1, 0);
    }

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        (void)addr;
        SetPrg16k(1, data & 0x07);
    }
};
