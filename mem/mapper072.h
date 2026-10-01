#pragma once
#include "banked_mapper.h"

// Jaleco JF-17 (mapper 72) and JF-19 (mapper 92): one register at
// $8000-$FFFF (bus conflicts) whose low nibble is loaded into the PRG bank
// when bit 7 goes from 0 to 1, and into the 8KB CHR bank when bit 6 does.
// Mapper 72 switches 16KB at $8000 with the last bank fixed at $C000;
// mapper 92 fixes the first bank at $8000 and switches $C000.
// Bits 4-5 drive a uPD7756C speech chip whose samples live in the chip's
// own mask ROM rather than the cartridge dump, so it isn't emulated.
class Mapper_072 : public BankedMapper {
public:
    Mapper_072(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId)
        : BankedMapper(prgBanks, chrBanks), jf19(mapperId == 92) {
        if (jf19) { SetPrg16k(0, 0); SetPrg16k(1, 0); }
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        data = BusConflict(addr, data);
        if ((data & 0x80) && !(last & 0x80)) SetPrg16k(jf19 ? 1 : 0, data & 0x0F);
        if ((data & 0x40) && !(last & 0x40)) SetChr8k(data & 0x0F);
        last = data;
    }

private:
    bool jf19;
    uint8_t last = 0;
};
