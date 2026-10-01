#pragma once
#include "banked_mapper.h"

// HVC-UN1ROM (mapper 94, Senjou no Ookami): UNROM with the 16KB PRG bank
// number in bits 2-4. Bus conflicts.
class Mapper_094 : public BankedMapper {
public:
    Mapper_094(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {}

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        SetPrg16k(0, (BusConflict(addr, data) >> 2) & 0x07);
    }
};
