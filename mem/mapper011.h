#pragma once
#include "banked_mapper.h"

// Color Dreams (mapper 11): one register selecting a 32KB PRG bank (bits
// 0-1) and an 8KB CHR bank (bits 4-7); bits 2-3 drive the lockout-defeat
// charge pump and do nothing an emulator can see. Crystal Mines, Bible
// Adventures, Menace Beach. Some boards lack bus conflicts, so none here.
class Mapper_011 : public BankedMapper {
public:
    Mapper_011(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        SetPrg32k(0);
    }

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        (void)addr;
        SetPrg32k(data & 0x03);
        SetChr8k(data >> 4);
    }
};
