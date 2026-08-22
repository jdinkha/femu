#pragma once
#include <cstdint>
#include <array>
#include <memory>
#include "Cartridge.h"
#include "../cpu/cpu.h"

class Bus {
public:
    Bus();

    CPU cpu;

    void insertCartridge(const std::shared_ptr<Cartridge>& cart);
    void reset();

    uint8_t cpuRead(uint16_t addr);
    void cpuWrite(uint16_t addr, uint8_t data);

private:
    std::array<uint8_t, 2048> ram{};
    std::shared_ptr<Cartridge> cartridge;

    // PPU ppu;
    // APU apu;
};
