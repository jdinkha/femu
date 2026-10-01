#pragma once
#include "banked_mapper.h"

// Camerica Quattro (mapper 232): Quattro Adventure/Sports/Arcade. A 64KB
// outer block chosen by $8000-$BFFF (bits 3-4) and a 16KB page within it by
// $C000-$FFFF (bits 0-1); $C000-$FFFF shows the block's last page.
class Mapper_232 : public BankedMapper {
public:
    Mapper_232(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        Update();
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        if (addr < 0xC000) block = (data >> 3) & 0x03;
        else               page = data & 0x03;
        Update();
    }

private:
    uint8_t block = 0, page = 0;

    void Update() {
        SetPrg16k(0, block * 4 + page);
        SetPrg16k(1, block * 4 + 3);
    }
};
