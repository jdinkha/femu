#pragma once
#include "banked_mapper.h"

// Bandai 74161/7432 (mapper 70): 16KB switchable PRG at $8000 (bits 4-7),
// last bank fixed at $C000, 8KB switchable CHR (bits 0-3), bus conflicts.
// Family Trainer games, Kamen Rider Club. Mapper 152 is the same board with
// one-screen mirroring control.
class Mapper_070 : public BankedMapper {
public:
    Mapper_070(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId)
        : BankedMapper(prgBanks, chrBanks), one_screen(mapperId == 152) {
        if (one_screen) mirror_mode = Mirror::ONESCREEN_LO;
    }

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        data = BusConflict(addr, data);
        SetPrg16k(0, (data >> 4) & (one_screen ? 0x07 : 0x0F));
        SetChr8k(data & 0x0F);
        if (one_screen) mirror_mode = (data & 0x80) ? Mirror::ONESCREEN_HI : Mirror::ONESCREEN_LO;
    }

private:
    bool one_screen;
};
