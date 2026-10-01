#pragma once
#include "banked_mapper.h"

// CPROM (mapper 13): 32KB fixed PRG, 16KB CHR-RAM - the first 4KB fixed at
// PPU $0000, any of the four 4KB pages switchable at $1000. Only used by
// Videomation. Has bus conflicts.
class Mapper_013 : public BankedMapper {
public:
    Mapper_013(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        chr_ram_size = 0x4000;
        SetPrg32k(0);
        SetChr4k(0, 0);
        SetChr4k(1, 0);
    }

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        SetChr4k(1, BusConflict(addr, data) & 0x03);
    }
};
