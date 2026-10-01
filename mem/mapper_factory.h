#pragma once
#include <cstdint>
#include <memory>
#include "mapper.h"

// What the iNES / NES 2.0 header says about the board.
struct RomInfo {
    uint8_t mapper = 0;
    uint8_t submapper = 0;  // NES 2.0 only; 0 otherwise
    uint8_t prgBanks = 0;   // 16KB units
    uint8_t chrBanks = 0;   // 8KB units, 0 = CHR-RAM
    bool four_screen = false;
    bool battery = false;
};

// Builds the mapper for a ROM, or returns null if femu doesn't support it.
// A board that gives the header's four-screen bit another meaning (mapper
// 78's Holy Diver flag) consumes it by clearing info.four_screen.
std::unique_ptr<Mapper> CreateMapper(RomInfo& info);
