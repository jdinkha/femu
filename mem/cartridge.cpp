#include "cartridge.h"
#include "mapper_factory.h"
#include "serialize.h"
#include <fstream>
#include <filesystem>

Cartridge::Cartridge(const std::string& path) {
    struct Header {
        char     name[4];
        uint8_t  prg_chunks; // 16KB units
        uint8_t  chr_chunks; // 8KB units
        uint8_t  mapper1;
        uint8_t  mapper2;
        uint8_t  prg_ram_size;
        uint8_t  tv_system1;
        uint8_t  tv_system2;
        char     unused[5];
    } header{};

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return;

    ifs.read((char*)&header, sizeof(Header));
    if (std::string(header.name, 4) != "NES\x1A") return; // bad magic bytes

    if (header.mapper1 & 0x04) {
        // 512-byte trainer present, skip it
        ifs.seekg(512, std::ios::cur);
    }

    // Old dumping tools left junk like "DiskDude!" in bytes 7-15 of iNES 1.0
    // headers, which would corrupt the mapper number's high nibble. A clean
    // iNES 1.0 header has bytes 12-15 zeroed (NES 2.0 uses them, so it's
    // exempt); when they aren't, trust only the low nibble.
    const bool nes2 = (header.mapper2 & 0x0C) == 0x08;
    const bool junk = !nes2 && (header.unused[1] | header.unused[2] | header.unused[3] | header.unused[4]) != 0;
    mapperID = (uint8_t)((junk ? 0 : (header.mapper2 >> 4) << 4) | (header.mapper1 >> 4));
    if (nes2 && (header.prg_ram_size & 0x0F)) return; // NES 2.0 mapper number above 255
    hw_mirror = (header.mapper1 & 0x01) ? Mirror::VERTICAL : Mirror::HORIZONTAL;
    battery_backed = (header.mapper1 & 0x02) != 0;

    nPRGBanks = header.prg_chunks;
    prgMemory.resize((size_t)nPRGBanks * 16384);
    ifs.read((char*)prgMemory.data(), (std::streamsize)prgMemory.size());

    nCHRBanks = header.chr_chunks;
    chrMemory.resize((size_t)nCHRBanks * 8192);
    if (nCHRBanks > 0)
        ifs.read((char*)chrMemory.data(), (std::streamsize)chrMemory.size());

    if (!ifs.good() && !ifs.eof()) return; // read failure partway through
    if (prgMemory.empty()) return;

    RomInfo info;
    info.mapper = mapperID;
    info.submapper = nes2 ? (uint8_t)(header.prg_ram_size >> 4) : 0; // NES 2.0 byte 8
    info.prgBanks = nPRGBanks;
    info.chrBanks = nCHRBanks;
    info.four_screen = (header.mapper1 & 0x08) != 0;
    info.battery = battery_backed;
    mapper = CreateMapper(info);
    if (!mapper) return; // unsupported mapper - see mapper_factory.cpp
    if (info.four_screen) four_screen_vram.resize(0x1000, 0x00); // board supplies all 4 nametables
    if (nes2) {
        // Bytes 10/11: PRG-RAM and CHR-RAM as shift counts (64 << n bytes,
        // 0 = none), volatile in the low nibble and battery-backed in the high.
        auto size = [](uint8_t shift) { return shift ? 64u << shift : 0u; };
        const uint8_t prg_ram_byte = header.tv_system2, chr_ram_byte = (uint8_t)header.unused[0];
        mapper->SetRamSizes(size(prg_ram_byte & 0x0F) + size(prg_ram_byte >> 4),
                            size(chr_ram_byte & 0x0F) + size(chr_ram_byte >> 4));
    }

    // CHR-RAM sits after any CHR-ROM (see Mapper::ppuMapRead). The board
    // decides how much; with neither ROM nor RAM, fall back to 8KB of RAM.
    chr_rom_size = chrMemory.size();
    chrMemory.resize(chr_rom_size + mapper->ChrRamSize(), 0x00);
    if (chrMemory.empty()) chrMemory.resize(8192, 0x00);

    // PRG-RAM is always allocated (some mappers use $6000-$7FFF as plain
    // work RAM even without a battery); only persisted to disk if the
    // header says this cartridge actually has a battery backing it.
    prg_ram.resize(mapper->PrgRamSize(), 0x00);
    mapper->AttachMemory(&prgMemory, &prg_ram);
    if (battery_backed) {
        sav_path = std::filesystem::path(path).replace_extension(".sav").string();
        std::ifstream sav(sav_path, std::ios::binary);
        if (sav.is_open()) {
            sav.read((char*)prg_ram.data(), (std::streamsize)prg_ram.size());
        }
    }

    // FNV-1a over the immutable ROM data; savestates use this to refuse
    // loading against the wrong game.
    rom_hash = 1469598103934665603ULL;
    auto hash_bytes = [this](const std::vector<uint8_t>& v) {
        for (uint8_t byte : v) {
            rom_hash ^= byte;
            rom_hash *= 1099511628211ULL;
        }
    };
    hash_bytes(prgMemory);
    hash_bytes(chrMemory);
    rom_hash ^= mapperID;
    rom_hash *= 1099511628211ULL;

    valid = true;
}

void Cartridge::SerializeState(StateWriter& w) const {
    w.writeBytes(prg_ram.data(), prg_ram.size());
    // CHR-RAM: mutable, must be saved (CHR-ROM never changes)
    w.writeBytes(chrMemory.data() + chr_rom_size, chrMemory.size() - chr_rom_size);
    w.writeBytes(four_screen_vram.data(), four_screen_vram.size());
    if (mapper) mapper->SerializeState(w);
}

void Cartridge::DeserializeState(StateReader& r) {
    r.readBytes(prg_ram.data(), prg_ram.size());
    r.readBytes(chrMemory.data() + chr_rom_size, chrMemory.size() - chr_rom_size);
    r.readBytes(four_screen_vram.data(), four_screen_vram.size());
    if (mapper) mapper->DeserializeState(r);
}

Cartridge::~Cartridge() {
    SaveRAM();
}

void Cartridge::SaveRAM() const {
    if (!battery_backed || sav_path.empty()) return;
    std::ofstream sav(sav_path, std::ios::binary | std::ios::trunc);
    if (!sav.is_open()) return;
    sav.write((const char*)prg_ram.data(), (std::streamsize)prg_ram.size());
}

Mirror Cartridge::mirror() const {
    Mirror m = mapper ? mapper->mirror() : Mirror::HARDWARE;
    return (m == Mirror::HARDWARE) ? hw_mirror : m;
}

bool Cartridge::cpuRead(uint16_t addr, uint8_t& data) {
    if (!mapper) return false;
    if (mapper->cpuReadHook(addr, data)) return true;

    uint32_t mapped_addr = 0;
    if (addr >= 0x4020 && !prg_ram.empty() && mapper->prgRamMap(addr, mapped_addr)) {
        data = mapper->prgRamEnabled() ? prg_ram[mapped_addr % prg_ram.size()] : 0x00;
        return true;
    }

    if (mapper->cpuMapRead(addr, mapped_addr)) {
        data = prgMemory[mapped_addr % prgMemory.size()];
        return true;
    }
    return false;
}

bool Cartridge::cpuWrite(uint16_t addr, uint8_t data) {
    if (!mapper) return false;
    if (mapper->cpuWriteHook(addr, data)) return true;

    uint32_t mapped_addr = 0;
    if (addr >= 0x4020 && !prg_ram.empty() && mapper->prgRamMap(addr, mapped_addr)) {
        // claimed even while disabled - the write is just ignored
        if (mapper->prgRamEnabled()) prg_ram[mapped_addr % prg_ram.size()] = data;
        return true;
    }

    if (mapper->cpuMapWrite(addr, mapped_addr, data)) {
        // Bank-switching mappers (MMC1/MMC3) fully handle the write
        // themselves via mapped_addr/data inside cpuMapWrite; PRG-ROM itself
        // is physically read-only so there's nothing further to do here.
        return true;
    }
    return false;
}

bool Cartridge::ppuRead(uint16_t addr, uint8_t& data) {
    if (!mapper) return false;
    if (mapper->ppuReadHook(addr, data)) return true;
    if (addr >= 0x2000 && addr <= 0x3EFF && !four_screen_vram.empty()) {
        data = four_screen_vram[addr & 0x0FFF];
        return true;
    }
    uint32_t mapped_addr = 0;
    if (mapper->ppuMapRead(addr, mapped_addr)) {
        data = chrMemory[mapped_addr % chrMemory.size()];
        return true;
    }
    return false;
}

bool Cartridge::ppuWrite(uint16_t addr, uint8_t data) {
    if (!mapper) return false;
    if (mapper->ppuWriteHook(addr, data)) return true;
    if (addr >= 0x2000 && addr <= 0x3EFF && !four_screen_vram.empty()) {
        four_screen_vram[addr & 0x0FFF] = data;
        return true;
    }
    uint32_t mapped_addr = 0;
    if (mapper->ppuMapWrite(addr, mapped_addr)) {
        chrMemory[mapped_addr % chrMemory.size()] = data;
        return true;
    }
    return false;
}

uint8_t Cartridge::NametablePage(uint8_t quadrant) const {
    if (mapper) {
        int page = mapper->nametablePage(quadrant);
        if (page >= 0) return (uint8_t)(page & 1);
    }
    switch (mirror()) {
        case Mirror::VERTICAL:     return quadrant & 1;
        case Mirror::HORIZONTAL:   return (quadrant >> 1) & 1;
        case Mirror::ONESCREEN_HI: return 1;
        case Mirror::ONESCREEN_LO:
        default:                   return 0;
    }
}
