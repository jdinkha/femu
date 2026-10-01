#pragma once
#include "banked_mapper.h"

// Sunsoft-2 on the Sunsoft-3 board (mapper 89, Tenka no Goikenban: Mito
// Koumon): $8000-$FFFF register (bus conflicts) with a 16KB PRG bank at
// $8000 (bits 4-6, last bank fixed), an 8KB CHR bank (bit 7 high, bits 0-2
// low) and one-screen mirroring (bit 3).
class Mapper_089 : public BankedMapper {
public:
    Mapper_089(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        mirror_mode = Mirror::ONESCREEN_LO;
    }

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        data = BusConflict(addr, data);
        SetPrg16k(0, (data >> 4) & 0x07);
        SetChr8k(((data >> 4) & 0x08) | (data & 0x07));
        mirror_mode = (data & 0x08) ? Mirror::ONESCREEN_HI : Mirror::ONESCREEN_LO;
    }
};
