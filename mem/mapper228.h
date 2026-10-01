#pragma once
#include "banked_mapper.h"

// Active Enterprises (mapper 228): Action 52, Cheetahmen II. Writes to
// $8000-$FFFF carry most of the register in the address:
//   A13 mirroring (0 = V, 1 = H), A11-A12 which 512KB PRG chip, A6-A10 a
//   16KB page on it, A5 PRG mode (0 = 32KB, 1 = the same 16KB twice),
//   A0-A3 the high CHR bits; data bits 0-1 the low CHR bits (8KB bank).
// Action 52 has three PRG chips - 0, 1 and 3 - stored in that order in the
// file; selecting the missing chip 2 reads open bus.
class Mapper_228 : public BankedMapper {
public:
    Mapper_228(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        WriteRegister(0x8000, 0x00); // the games expect $00 at $8000 on power-up
    }

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr >= 0x8000 && chip_missing) return false;
        return BankedMapper::cpuMapRead(addr, mapped_addr);
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        uint8_t chip = (addr >> 11) & 0x03;
        chip_missing = chip == 2;
        if (chip == 3) chip = 2;
        int page = chip * 32 + ((addr >> 6) & 0x1F);
        if (addr & 0x20) {
            SetPrg16k(0, page);
            SetPrg16k(1, page);
        } else {
            SetPrg16k(0, page & ~1);
            SetPrg16k(1, page | 1);
        }
        SetChr8k(((addr & 0x0F) << 2) | (data & 0x03));
        mirror_mode = (addr & 0x2000) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
    }

private:
    bool chip_missing = false;
};
