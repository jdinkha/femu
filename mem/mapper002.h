#pragma once
#include "banked_mapper.h"

// UxROM (mapper 2): 16KB switchable PRG at $8000, last 16KB fixed at $C000,
// 8KB CHR-RAM. Mega Man, Castlevania, Contra, DuckTales. Emulated without
// bus conflicts unless the NES 2.0 submapper (2) asks for them.
class Mapper_002 : public BankedMapper {
public:
    Mapper_002(uint8_t prgBanks, uint8_t chrBanks, uint8_t submapper)
        : BankedMapper(prgBanks, chrBanks), bus_conflicts(submapper == 2) {}

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        if (bus_conflicts) data = BusConflict(addr, data);
        SetPrg16k(0, data);
    }

private:
    bool bus_conflicts;
};
