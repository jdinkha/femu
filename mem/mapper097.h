#pragma once
#include "banked_mapper.h"

// Irem TAM-S1 (mapper 97, Kaiketsu Yanchamaru): the last 16KB is fixed at
// $8000 and $8000-$BFFF writes switch $C000 (bits 0-4) and mirroring
// (bit 7: 0 = horizontal, 1 = vertical). No bus conflicts.
class Mapper_097 : public BankedMapper {
public:
    Mapper_097(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        SetPrg16k(0, -1);
        SetPrg16k(1, 0);
    }

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        if (addr >= 0xC000) return;
        SetPrg16k(1, data & 0x1F);
        mirror_mode = (data & 0x80) ? Mirror::VERTICAL : Mirror::HORIZONTAL;
    }
};
