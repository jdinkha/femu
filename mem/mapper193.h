#pragma once
#include "banked_mapper.h"

// NTDEC TC-112 (mapper 193, Fighting Hero): registers at $6000-$7FFF
// (mask $E007, so no PRG-RAM). $6000 picks a 4KB CHR bank for $0000,
// $6001/$6002 2KB banks for $1000/$1800 (all in 1KB units with the low bits
// ignored), $6003 an 8KB PRG bank for $8000 (the last three are fixed), and
// $6004 bit 0 the mirroring (0 = vertical).
class Mapper_193 : public BankedMapper {
public:
    Mapper_193(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        prg_ram_size = 0;
        SetPrg8k(0, 0);
        SetPrg8k(1, -3);
        SetPrg8k(2, -2);
        SetPrg8k(3, -1);
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        if (addr < 0x6000 || addr > 0x7FFF) return false;
        switch (addr & 0x0007) {
            case 0: SetChr4k(0, data >> 2); break;
            case 1: SetChr2k(2, data >> 1); break;
            case 2: SetChr2k(3, data >> 1); break;
            case 3: SetPrg8k(0, data & 0x0F); break;
            case 4: mirror_mode = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL; break;
            default: break;
        }
        return true;
    }
};
