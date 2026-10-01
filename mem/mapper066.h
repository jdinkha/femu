#pragma once
#include "banked_mapper.h"

// GxROM (mapper 66): one register selecting a 32KB PRG bank (bits 4-5) and
// an 8KB CHR bank (bits 0-1). Super Mario Bros. + Duck Hunt, Dragon Power,
// Gumshoe.
class Mapper_066 : public BankedMapper {
public:
    Mapper_066(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        SetPrg32k(0);
    }

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        (void)addr;
        SetPrg32k((data >> 4) & 0x03);
        SetChr8k(data & 0x03);
    }
};
