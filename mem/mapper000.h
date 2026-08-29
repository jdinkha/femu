#pragma once
#include "mapper.h"

// NROM: the simplest mapper, no bank switching at all. Either 16KB of PRG
// (mirrored across $8000-$FFFF) or a full 32KB. CHR is always a single
// fixed 8KB bank (RAM if the cartridge shipped with none).
class Mapper_000 : public Mapper {
public:
    Mapper_000(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {}
    ~Mapper_000() override = default;

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
};
