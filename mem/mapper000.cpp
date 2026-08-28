#pragma once
#include "mapper.h"

class Mapper000 : public Mapper {
public:
    Mapper000(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {}

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override {
        if (addr >= 0x8000 && addr <= 0xFFFF) {
            mappedAddr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
            return true;
        }
        return false;
    }

    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr) override {
        if (addr >= 0x8000 && addr <= 0xFFFF) {
            mappedAddr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
            return true; // ROM — write is normally a no-op at Cartridge level
        }
        return false;
    }

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override {
        if (addr <= 0x1FFF) {
            mappedAddr = addr;
            return true;
        }
        return false;
    }

    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr) override {
        if (addr <= 0x1FFF && nCHRBanks == 0) {
            mappedAddr = addr; // only writable when using CHR-RAM
            return true;
        }
        return false;
    }
};
