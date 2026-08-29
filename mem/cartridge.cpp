#include "cartridge.h"
#include "mapper000.h"
#include "mapper001.h"
#include "mapper004.h"
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

    mapperID = (uint8_t)(((header.mapper2 >> 4) << 4) | (header.mapper1 >> 4));
    hw_mirror = (header.mapper1 & 0x01) ? Mirror::VERTICAL : Mirror::HORIZONTAL;
    battery_backed = (header.mapper1 & 0x02) != 0;

    nPRGBanks = header.prg_chunks;
    prgMemory.resize((size_t)nPRGBanks * 16384);
    ifs.read((char*)prgMemory.data(), (std::streamsize)prgMemory.size());

    nCHRBanks = header.chr_chunks;
    chrMemory.resize((size_t)(nCHRBanks == 0 ? 1 : nCHRBanks) * 8192); // banks==0 means CHR-RAM
    if (nCHRBanks > 0)
        ifs.read((char*)chrMemory.data(), (std::streamsize)chrMemory.size());

    if (!ifs.good() && !ifs.eof()) return; // read failure partway through

    switch (mapperID) {
        case 0:
            mapper = std::make_unique<Mapper_000>(nPRGBanks, nCHRBanks);
            break;
        case 1:
            mapper = std::make_unique<Mapper_001>(nPRGBanks, nCHRBanks);
            break;
        case 4:
            mapper = std::make_unique<Mapper_004>(nPRGBanks, nCHRBanks);
            break;
        default:
            return; // unsupported mapper - add more Mapper_XXX classes as needed
    }

    // PRG-RAM is always allocated (some mappers use $6000-$7FFF as plain
    // work RAM even without a battery); only persisted to disk if the
    // header says this cartridge actually has a battery backing it.
    prg_ram.resize(8192, 0x00);
    if (battery_backed) {
        sav_path = std::filesystem::path(path).replace_extension(".sav").string();
        std::ifstream sav(sav_path, std::ios::binary);
        if (sav.is_open()) {
            sav.read((char*)prg_ram.data(), (std::streamsize)prg_ram.size());
        }
    }

    valid = true;
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
    if (addr >= 0x6000 && addr <= 0x7FFF && !prg_ram.empty()) {
        if (mapper && !mapper->prgRamEnabled()) { data = 0x00; return true; }
        data = prg_ram[addr - 0x6000];
        return true;
    }

    uint32_t mapped_addr = 0;
    if (mapper && mapper->cpuMapRead(addr, mapped_addr)) {
        data = prgMemory[mapped_addr % prgMemory.size()];
        return true;
    }
    return false;
}

bool Cartridge::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr >= 0x6000 && addr <= 0x7FFF && !prg_ram.empty()) {
        if (mapper && !mapper->prgRamEnabled()) return true; // claimed, but ignored while disabled
        prg_ram[addr - 0x6000] = data;
        return true;
    }

    uint32_t mapped_addr = 0;
    if (mapper && mapper->cpuMapWrite(addr, mapped_addr, data)) {
        // Bank-switching mappers (MMC1/MMC3) fully handle the write
        // themselves via mapped_addr/data inside cpuMapWrite; PRG-ROM itself
        // is physically read-only so there's nothing further to do here.
        return true;
    }
    return false;
}

bool Cartridge::ppuRead(uint16_t addr, uint8_t& data) {
    uint32_t mapped_addr = 0;
    if (mapper && mapper->ppuMapRead(addr, mapped_addr)) {
        data = chrMemory[mapped_addr % chrMemory.size()];
        return true;
    }
    return false;
}

bool Cartridge::ppuWrite(uint16_t addr, uint8_t data) {
    uint32_t mapped_addr = 0;
    if (mapper && mapper->ppuMapWrite(addr, mapped_addr)) {
        chrMemory[mapped_addr % chrMemory.size()] = data;
        return true;
    }
    return false;
}
