#pragma once
#include "banked_mapper.h"

// AxROM (mapper 7): 32KB switchable PRG, 8KB CHR-RAM, and a register bit
// that picks which single nametable page all four quadrants show.
// Battletoads, Wizards & Warriors, Marble Madness. No bus conflicts unless
// the NES 2.0 submapper (2) says the board has them.
class Mapper_007 : public BankedMapper {
public:
    Mapper_007(uint8_t prgBanks, uint8_t chrBanks, uint8_t submapper)
        : BankedMapper(prgBanks, chrBanks), bus_conflicts(submapper == 2) {
        SetPrg32k(0);
        mirror_mode = Mirror::ONESCREEN_LO;
    }

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        if (bus_conflicts) data = BusConflict(addr, data);
        SetPrg32k(data & 0x0F);
        mirror_mode = (data & 0x10) ? Mirror::ONESCREEN_HI : Mirror::ONESCREEN_LO;
    }

private:
    bool bus_conflicts;
};
