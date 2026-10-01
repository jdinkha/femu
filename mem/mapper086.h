#pragma once
#include "banked_mapper.h"

// Jaleco JF-13 (mapper 86, Moero!! Pro Yakyuu): no PRG-RAM - $6000-$6FFF
// is a register with a 32KB PRG bank (bits 4-5) and an 8KB CHR bank (bit 6
// high, bits 0-1 low). $7000-$7FFF drives a uPD7756C speech chip whose
// samples aren't in the dump, so it isn't emulated.
class Mapper_086 : public BankedMapper {
public:
    Mapper_086(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        prg_ram_size = 0;
        SetPrg32k(0);
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        if (addr < 0x6000 || addr > 0x7FFF) return false;
        if (addr < 0x7000) {
            SetPrg32k((data >> 4) & 0x03);
            SetChr8k(((data >> 4) & 0x04) | (data & 0x03));
        }
        return true;
    }
};
