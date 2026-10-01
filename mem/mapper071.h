#pragma once
#include "banked_mapper.h"

// Camerica/Codemasters (mapper 71): UNROM-like - 16KB switchable PRG at
// $8000 via $C000-$FFFF, last bank fixed at $C000, 8KB CHR-RAM, no bus
// conflicts. Fire Hawk alone adds one-screen mirroring control at
// $8000-$9FFF (bit 4). NES 2.0 submapper 1 marks it; without that, follow
// FCEUX: keep the header's mirroring until $9000-$9FFF is written, since
// other games write junk to $8000 at startup.
class Mapper_071 : public BankedMapper {
public:
    Mapper_071(uint8_t prgBanks, uint8_t chrBanks, uint8_t submapper)
        : BankedMapper(prgBanks, chrBanks), fire_hawk(submapper == 1) {}

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        if (addr >= 0xC000) {
            SetPrg16k(0, data & 0x0F);
        } else if (addr < 0xA000 && (fire_hawk || addr >= 0x9000)) {
            mirror_mode = (data & 0x10) ? Mirror::ONESCREEN_HI : Mirror::ONESCREEN_LO;
        }
    }

private:
    bool fire_hawk;
};
