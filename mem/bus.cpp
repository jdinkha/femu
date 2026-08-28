#include "bus.h"

// NES CPU memory map:
//   $0000-$1FFF  2KB internal RAM, mirrored every $0800
//   $2000-$3FFF  PPU registers (8 of them), mirrored every 8 bytes
//   $4000-$4017  APU / controller I/O registers (stubbed - APU/input not built yet)
//   $4020-$7FFF  unmapped on NROM (some mappers use this for PRG-RAM)
//   $8000-$FFFF  cartridge PRG-ROM, via the mapper

Bus::Bus() {
    cpu.ConnectBus(this);
    ram.fill(0x00);
}

void Bus::insertCartridge(const std::shared_ptr<Cartridge>& cart) {
    cartridge = cart;
    ppu.ConnectCartridge(cart);
}

void Bus::reset() {
    cpu.reset();
    ppu.reset();
    system_clock_counter = 0;
}

void Bus::clock() {
    // PPU runs 3x the CPU's rate - this is the actual master clock of the
    // system; the CPU is downstream of it, not the other way around.
    ppu.clock();
    if (system_clock_counter % 3 == 0) {
        cpu.clock();
    }

    if (ppu.nmi) {
        ppu.nmi = false;
        cpu.nmi();
    }

    system_clock_counter++;
}

void Bus::cpuWrite(uint16_t addr, uint8_t data) {
    if (cartridge && cartridge->cpuWrite(addr, data)) {
        // handled by cartridge/mapper (PRG-RAM, bank-select registers, etc.)
    } else if (addr <= 0x1FFF) {
        ram[addr & 0x07FF] = data;
    } else if (addr <= 0x3FFF) {
        ppu.cpuWrite(addr & 0x0007, data);
    } else if (addr <= 0x4017) {
        // APU / controller I/O - stub
    }
}

uint8_t Bus::cpuRead(uint16_t addr) {
    uint8_t data = 0x00;
    if (cartridge && cartridge->cpuRead(addr, data)) {
        return data;
    } else if (addr <= 0x1FFF) {
        return ram[addr & 0x07FF];
    } else if (addr <= 0x3FFF) {
        return ppu.cpuRead(addr & 0x0007);
    } else if (addr <= 0x4017) {
        return 0x00;
    }
    return 0x00;
}
