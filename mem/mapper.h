#pragma once
#include <cstdint>

// Base class for cartridge mapper chips. A mapper's whole job is address
// translation: given a CPU or PPU address, decide whether this mapper
// handles it and, if so, what physical offset into PRG/CHR memory it
// corresponds to. Bank-switching state (for MMC1/MMC3 etc.) lives in the
// derived class.
class Mapper {
public:
    Mapper(uint8_t prgBanks, uint8_t chrBanks) : nPRGBanks(prgBanks), nCHRBanks(chrBanks) {}
    virtual ~Mapper() = default;

    // Each returns true if this mapper claims the address, and writes the
    // resulting physical offset into mapped_addr.
    virtual bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr)  = 0;
    virtual bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr)  = 0;
    virtual bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) = 0;

    virtual void reset() {}

protected:
    uint8_t nPRGBanks = 0;
    uint8_t nCHRBanks = 0;
};
