#include "Bus.h"

Bus::Bus() {
    cpu.ConnectBus(this);
    ram.fill(0x00);
}

void Bus::insertCartridge(const std::shared_ptr<Cartridge>& cart) {
    cartridge = cart;
}

void Bus::reset() {
    cpu.reset();
}

uint8_t Bus::cpuRead(uint16_t addr) {
    uint8_t data = 0x00;

    if (cartridge && cartridge->cpuRead(addr, data)) {
        return data;
    } else if (addr <= 0x1FFF) {
        return ram[addr & 0x07FF];
    } else if (addr >= 0x2000 && addr <= 0x3FFF) {
        // return ppu.cpuRead(addr & 0x0007);
        return 0x00; // stub
    } else if (addr >= 0x4000 && addr <= 0x4017) {
        return 0x00; // stub — APU/controllers
    }
    return data;
}

void Bus::cpuWrite(uint16_t addr, uint8_t data) {
    if (cartridge && cartridge->cpuWrite(addr, data)) {
        return;
    } else if (addr <= 0x1FFF) {
        ram[addr & 0x07FF] = data;
    } else if (addr >= 0x2000 && addr <= 0x3FFF) {
        // ppu.cpuWrite(addr & 0x0007, data);
    } else if (addr == 0x4014) {
        // OAM DMA trigger — later
    } else if (addr >= 0x4000 && addr <= 0x4017) {
        // APU/controller write
    }
}
