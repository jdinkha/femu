#pragma once
#include "banked_mapper.h"

// Irem 74HC161/32 (mapper 78): $8000-$FFFF register (bus conflicts) with a
// 16KB PRG bank at $8000 (bits 0-2, last bank fixed at $C000), an 8KB CHR
// bank (bits 4-7), and a mirroring bit whose meaning depends on the board:
// Holy Diver uses it as H/V (0 = horizontal), Uchuusen - Cosmo Carrier as
// one-screen A/B. NES 2.0 submapper 3 means Holy Diver and 1 Cosmo Carrier;
// iNES 1.0 dumps of Holy Diver set the four-screen header bit to say so.
class Mapper_078 : public BankedMapper {
public:
    Mapper_078(uint8_t prgBanks, uint8_t chrBanks, bool holyDiver)
        : BankedMapper(prgBanks, chrBanks), holy_diver(holyDiver) {
        mirror_mode = holy_diver ? Mirror::HORIZONTAL : Mirror::ONESCREEN_LO;
    }

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        data = BusConflict(addr, data);
        SetPrg16k(0, data & 0x07);
        SetChr8k(data >> 4);
        bool m = data & 0x08;
        if (holy_diver) mirror_mode = m ? Mirror::VERTICAL : Mirror::HORIZONTAL;
        else            mirror_mode = m ? Mirror::ONESCREEN_HI : Mirror::ONESCREEN_LO;
    }

private:
    bool holy_diver;
};
