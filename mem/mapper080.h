#pragma once
#include "banked_mapper.h"

// Taito X1-005 (mapper 80: Kyonshiizu 2, Minelvaton Saga, Taito Grand Prix)
// and its single-screen wiring (mapper 207: Fudou Myouou Den). Registers
// at $7EF0-$7EFF (A7 ignored, so also $7E70-$7E7F):
//   $7EF0/1  2KB CHR banks at $0000/$0800 (low bit ignored)
//   $7EF2-5  1KB CHR banks at $1000-$1C00
//   $7EF6    80 only: bit 0 mirroring (0 = horizontal)
//   $7EF8/9  $A3 unlocks the chip's 128 bytes of RAM at $7F00-$7FFF
//   $7EFA/B, $7EFC/D, $7EFE/F  8KB PRG banks at $8000/$A000/$C000
// On 207, bit 7 of $7EF0 / $7EF1 instead picks the CIRAM page for the top
// / bottom pair of nametables. The internal RAM is battery-backed on some
// boards, so it is this board's PRG-RAM.
class Mapper_080 : public BankedMapper {
public:
    Mapper_080(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId)
        : BankedMapper(prgBanks, chrBanks), single_screen_bits(mapperId == 207) {
        prg_ram_size = 128;
        SetPrg8k(0, 0);
        SetPrg8k(1, 0);
        SetPrg8k(2, 0);
        SetPrg8k(3, -1);
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override;
    bool prgRamMap(uint16_t addr, uint32_t& offset) override {
        if (addr < 0x7F00 || addr > 0x7FFF || !ram_unlocked) return false;
        offset = addr & 0x7F;
        return true;
    }
    int nametablePage(uint8_t quadrant) const override {
        return single_screen_bits ? chr_regs[quadrant >> 1] >> 7 : -1;
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

private:
    bool single_screen_bits;
    uint8_t chr_regs[2] = {0, 0};
    bool ram_unlocked = false;
};
