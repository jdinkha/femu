#pragma once
#include <cstdint>
#include <array>
#include <memory>
#include "cartridge.h"
#include "../cpu/cpu.h"
#include "../ppu/ppu.h"

class Bus {
public:
    Bus();

    CPU cpu;
    PPU ppu;

    void insertCartridge(const std::shared_ptr<Cartridge>& cart);
    void reset();
    void clock(); // master system clock: steps ppu 3x per 1x cpu, handles NMI

    uint8_t cpuRead(uint16_t addr);
    void cpuWrite(uint16_t addr, uint8_t data);

private:
    std::array<uint8_t, 2048> ram{};
    std::shared_ptr<Cartridge> cartridge;
    uint32_t system_clock_counter = 0;
    // APU apu;
};
