#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <memory>
#include "cartridge.h"
#include "../cpu/cpu.h"
#include "../ppu/ppu.h"
#include "../apu/apu.h"

struct StateWriter;
struct StateReader;

class Bus {
public:
    Bus();

    CPU cpu;
    PPU ppu;
    APU apu;

    void insertCartridge(const std::shared_ptr<Cartridge>& cart);
    void reset();
    void clock(); // master system clock: steps ppu 3x per 1x cpu, handles NMI

    // Savestate support. SerializeState/DeserializeState round-trip the entire
    // machine: internal RAM, the system clock divider, latched controller
    // shift registers, and every subcomponent (CPU, PPU, APU, cartridge
    // PRG/CHR-RAM + mapper). The ROM and the frontend-supplied `controller`
    // inputs are not machine state and are left to the caller.
    bool HasCartridge() const { return cartridge != nullptr; }
    uint64_t RomHash() const { return cartridge ? cartridge->RomHash() : 0; }
    void SerializeState(StateWriter& w) const;
    void DeserializeState(StateReader& r);

    uint8_t cpuRead(uint16_t addr);
    void cpuWrite(uint16_t addr, uint8_t data);

    // Controller state, set by the frontend (main.cpp) once per frame before
    // stepping the emulator. Bit layout (matches real hardware's shift-out
    // order, A first): bit7=A bit6=B bit5=Select bit4=Start
    //                   bit3=Up bit2=Down bit1=Left bit0=Right
    uint8_t controller[2] = { 0x00, 0x00 };

    // Resampled audio, ready to hand to an SDL audio stream. clock() appends
    // to this at ~44.1kHz (downsampled from the APU's per-CPU-cycle rate);
    // the frontend drains and clears it once per frame.
    std::vector<float> audio_samples;

private:
    std::array<uint8_t, 2048> ram{};
    std::shared_ptr<Cartridge> cartridge;
    uint32_t system_clock_counter = 0;

    // Latched snapshot of `controller[]`, taken on a $4016 write and shifted
    // out one bit per $4016/$4017 read afterward - this is the actual
    // shift-register behavior real games rely on.
    uint8_t controller_state[2] = { 0x00, 0x00 };

    double audio_time_accumulator = 0.0;
    static constexpr double CPU_CLOCK_HZ = 1789773.0; // NTSC
    static constexpr double AUDIO_SAMPLE_RATE = 44100.0;
};
