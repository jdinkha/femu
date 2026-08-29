#include "mapper000.h"

bool Mapper_000::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x8000) return false;
    // NROM-128 (1 bank): mirror $8000-$BFFF into $C000-$FFFF.
    // NROM-256 (2 banks): full 32KB mapped directly.
    mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
    return true;
}

bool Mapper_000::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    (void)data; // NROM's PRG-ROM is physically read-only; nothing to do with the value
    if (addr < 0x8000) return false;
    mapped_addr = addr & (nPRGBanks > 1 ? 0x7FFF : 0x3FFF);
    return true;
}

bool Mapper_000::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr > 0x1FFF) return false;
    mapped_addr = addr;
    return true;
}

bool Mapper_000::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr > 0x1FFF) return false;
    if (nCHRBanks != 0) return false; // real CHR-ROM is not writable
    mapped_addr = addr; // CHR-RAM case
    return true;
}
