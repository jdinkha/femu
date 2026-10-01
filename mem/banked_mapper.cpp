#include "banked_mapper.h"
#include "serialize.h"

namespace {
// Wraps a (possibly negative) bank number into [0, count).
uint32_t Wrap(int bank, uint32_t count) {
    if (count == 0) return 0;
    int64_t b = bank % (int64_t)count;
    return (uint32_t)(b < 0 ? b + count : b);
}
} // namespace

BankedMapper::BankedMapper(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    // Power-on layout most boards share: first 16KB at $8000, last at $C000,
    // CHR identity-mapped.
    SetPrg16k(0, 0);
    SetPrg16k(1, -1);
    SetChr8k(0);
}

void BankedMapper::SetPrg8k(int slot, int bank) {
    prg_map[slot & 3] = Wrap(bank, Prg8kCount()) * 0x2000;
}

void BankedMapper::SetPrg16k(int slot, int bank) {
    uint32_t b = Wrap(bank, Prg8kCount() / 2 ? Prg8kCount() / 2 : 1);
    prg_map[(slot & 1) * 2]     = b * 0x4000;
    prg_map[(slot & 1) * 2 + 1] = b * 0x4000 + 0x2000;
}

void BankedMapper::SetPrg32k(int bank) {
    uint32_t count = Prg8kCount() / 4;
    uint32_t b = Wrap(bank, count ? count : 1);
    for (int i = 0; i < 4; i++) prg_map[i] = b * 0x8000 + i * 0x2000;
}

void BankedMapper::SetPrg6000(int bank) {
    prg6000 = Wrap(bank, Prg8kCount()) * 0x2000;
    prg6000_rom = true;
}

void BankedMapper::SetChr1k(int slot, int bank) {
    chr_map[slot & 7] = Wrap(bank, Chr1kCount()) * 0x400;
    chr_writable[slot & 7] = nCHRBanks == 0; // no CHR-ROM: these banks are RAM
}

void BankedMapper::SetChr2k(int slot, int bank) {
    uint32_t count = Chr1kCount() / 2;
    uint32_t b = Wrap(bank, count ? count : 1);
    for (int i = 0; i < 2; i++) {
        chr_map[(slot & 3) * 2 + i] = b * 0x800 + i * 0x400;
        chr_writable[(slot & 3) * 2 + i] = nCHRBanks == 0;
    }
}

void BankedMapper::SetChr4k(int slot, int bank) {
    uint32_t count = Chr1kCount() / 4;
    uint32_t b = Wrap(bank, count ? count : 1);
    for (int i = 0; i < 4; i++) {
        chr_map[(slot & 1) * 4 + i] = b * 0x1000 + i * 0x400;
        chr_writable[(slot & 1) * 4 + i] = nCHRBanks == 0;
    }
}

void BankedMapper::SetChr8k(int bank) {
    uint32_t count = Chr1kCount() / 8;
    uint32_t b = Wrap(bank, count ? count : 1);
    for (int i = 0; i < 8; i++) {
        chr_map[i] = b * 0x2000 + i * 0x400;
        chr_writable[i] = nCHRBanks == 0;
    }
}

void BankedMapper::SetChrRam1k(int slot, int bank) {
    chr_map[slot & 7] = (uint32_t)nCHRBanks * 0x2000 + Wrap(bank, ChrRam1kCount()) * 0x400;
    chr_writable[slot & 7] = true;
}

uint8_t BankedMapper::BusConflict(uint16_t addr, uint8_t data) const {
    if (addr < 0x8000) return data;
    return Mapper::BusConflict(prg_map[(addr - 0x8000) >> 13] + (addr & 0x1FFF), data);
}

bool BankedMapper::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x8000) {
        mapped_addr = prg_map[(addr - 0x8000) >> 13] + (addr & 0x1FFF);
        return true;
    }
    if (addr >= 0x6000 && prg6000_rom) {
        mapped_addr = prg6000 + (addr & 0x1FFF);
        return true;
    }
    return false;
}

bool BankedMapper::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    (void)mapped_addr; // PRG-ROM itself is never written
    if (addr < 0x8000) return false;
    WriteRegister(addr, data);
    return true;
}

bool BankedMapper::prgRamMap(uint16_t addr, uint32_t& offset) {
    if (prg6000_rom) return false; // ROM is in the $6000 window instead
    return Mapper::prgRamMap(addr, offset);
}

bool BankedMapper::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr > 0x1FFF) return false;
    mapped_addr = chr_map[addr >> 10] + (addr & 0x3FF);
    return true;
}

bool BankedMapper::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr > 0x1FFF || !chr_writable[addr >> 10]) return false;
    mapped_addr = chr_map[addr >> 10] + (addr & 0x3FF);
    return true;
}

void BankedMapper::SerializeState(StateWriter& w) const {
    w.writeBytes(prg_map.data(), sizeof(prg_map));
    w.write(prg6000);
    w.write(prg6000_rom);
    w.writeBytes(chr_map.data(), sizeof(chr_map));
    w.writeBytes(chr_writable.data(), sizeof(chr_writable));
    w.write(mirror_mode);
}

void BankedMapper::DeserializeState(StateReader& r) {
    r.readBytes(prg_map.data(), sizeof(prg_map));
    r.read(prg6000);
    r.read(prg6000_rom);
    r.readBytes(chr_map.data(), sizeof(chr_map));
    r.readBytes(chr_writable.data(), sizeof(chr_writable));
    r.read(mirror_mode);
}
