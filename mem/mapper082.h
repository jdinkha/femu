#pragma once
#include "banked_mapper.h"

// Taito X1-017 (mapper 82): SD Keiji: Blader, Kyuukyoku Harikiri Koushien.
// Registers at $7EF0-$7EFC:
//   $7EF0/1  2KB CHR banks, $7EF2-5 1KB CHR banks (swapped halves when
//            $7EF6 bit 1 is set)
//   $7EF6    bit 0 mirroring (0 = horizontal), bit 1 CHR A12 inversion
//   $7EF7/8/9  unlock the chip's 5KB of RAM: $CA for $6000-$67FF, $69
//            for $6800-$6FFF, $84 for $7000-$73FF
//   $7EFA/B/C  8KB PRG banks at $8000/$A000/$C000, bank number in bits 2-5
// The RAM is battery-backed on some boards, so it is this board's PRG-RAM.
// The chip's IRQ timer was never used by a game and isn't emulated.
class Mapper_082 : public BankedMapper {
public:
    Mapper_082(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        prg_ram_size = 0x1400;
        SetPrg8k(0, 0);
        SetPrg8k(1, 0);
        SetPrg8k(2, 0);
        SetPrg8k(3, -1);
        Update();
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override;
    bool prgRamMap(uint16_t addr, uint32_t& offset) override {
        if (addr < 0x6000 || addr > 0x73FF) return false;
        int region = addr < 0x6800 ? 0 : addr < 0x7000 ? 1 : 2;
        if (!unlocked[region]) return false;
        offset = addr - 0x6000u;
        return true;
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

private:
    uint8_t chr[6] = {0, 2, 4, 5, 6, 7};
    bool chr_invert = false;
    bool unlocked[3] = {false, false, false};

    void Update();
};
