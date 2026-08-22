#pragma once
#include <cstdint>
#include <array>
#include <memory>
#include "cpu.h"
#include "tests/cartridge.h"

// NES CPU memory map (mapper 0 / NROM scope):
//   $0000-$1FFF  2KB internal RAM, mirrored every $0800
//   $2000-$3FFF  PPU registers, mirrored every 8 bytes (stubbed - no PPU yet)
//   $4000-$4017  APU / controller I/O registers (stubbed)
//   $4020-$7FFF  unmapped on NROM
//   $8000-$FFFF  cartridge PRG-ROM
class Bus {
public:
    Bus() { cpu.ConnectBus(this); ram.fill(0x00); }

    CPU cpu;
    std::array<uint8_t, 2048> ram{};
    std::shared_ptr<Cartridge> cart;

    void InsertCartridge(const std::shared_ptr<Cartridge>& c) { cart = c; }

    void write(uint16_t addr, uint8_t data) {
        if (addr <= 0x1FFF) {
            ram[addr & 0x07FF] = data;
        } else if (addr <= 0x3FFF) {
            // PPU registers - stub
        } else if (addr <= 0x4017) {
            // APU / controller I/O - stub
        } else if (addr >= 0x8000 && cart) {
            cart->cpuWrite(addr, data);
        }
    }

    uint8_t read(uint16_t addr) {
        if (addr <= 0x1FFF) {
            return ram[addr & 0x07FF];
        } else if (addr <= 0x3FFF) {
            return 0x00;
        } else if (addr <= 0x4017) {
            return 0x00;
        } else if (addr >= 0x8000 && cart) {
            return cart->cpuRead(addr);
        }
        return 0x00;
    }
};
