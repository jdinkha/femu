#pragma once
#include "banked_mapper.h"

// Caltron 6-in-1 (mapper 41), a discrete multicart. The outer register is
// written through the *address* of a write to $6000-$67FF: A0-A2 pick a
// 32KB PRG bank, A3-A4 a 32KB outer CHR bank, A5 the mirroring (0 = V,
// 1 = H). The inner CHR register at $8000-$FFFF (bits 0-1, an 8KB bank
// inside the outer one, bus conflicts) only takes writes while the PRG bank
// is 4-7.
class Mapper_041 : public BankedMapper {
public:
    Mapper_041(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        mirror_mode = Mirror::VERTICAL;
        Update();
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        (void)data;
        if (addr < 0x6000 || addr > 0x67FF) return false;
        outer = (uint8_t)(addr & 0x3F);
        Update();
        return true;
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        if (!(outer & 0x04)) return;
        inner = BusConflict(addr, data) & 0x03;
        Update();
    }

private:
    uint8_t outer = 0, inner = 0;

    void Update() {
        SetPrg32k(outer & 0x07);
        SetChr8k(((outer >> 3) & 0x03) << 2 | inner);
        mirror_mode = (outer & 0x20) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
    }
};
