#include "Cartridge.h"
#include "Mapper000.h"
#include <fstream>

struct INesHeader {
    char name[4];
    uint8_t prgRomChunks;
    uint8_t chrRomChunks;
    uint8_t mapper1;
    uint8_t mapper2;
    uint8_t prgRamSize;
    uint8_t tvSystem1;
    uint8_t tvSystem2;
    char unused[5];
};

Cartridge::Cartridge(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return;

    INesHeader header{};
    ifs.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (std::string(header.name, 4) != "NES\x1A") return;

    if (header.mapper1 & 0x04) ifs.seekg(512, std::ios::cur); // trainer

    mapperID = ((header.mapper2 >> 4) << 4) | (header.mapper1 >> 4);
    mirror = (header.mapper1 & 0x01) ? Mirror::VERTICAL : Mirror::HORIZONTAL;

    nPRGBanks = header.prgRomChunks;
    prgMemory.resize(static_cast<size_t>(nPRGBanks) * 16384);
    ifs.read(reinterpret_cast<char*>(prgMemory.data()), prgMemory.size());

    nCHRBanks = header.chrRomChunks;
    if (nCHRBanks == 0) {
        chrMemory.resize(8192); // CHR-RAM
    } else {
        chrMemory.resize(static_cast<size_t>(nCHRBanks) * 8192);
        ifs.read(reinterpret_cast<char*>(chrMemory.data()), chrMemory.size());
    }

    switch (mapperID) {
        case 0:
            mapper = std::make_unique<Mapper000>(nPRGBanks, nCHRBanks);
            break;
        default:
            return; // add more mappers as you support them
    }

    valid = true;
}

bool Cartridge::cpuRead(uint16_t addr, uint8_t& data) {
    uint32_t mapped;
    if (mapper && mapper->cpuMapRead(addr, mapped)) {
        data = prgMemory[mapped];
        return true;
    }
    return false;
}

bool Cartridge::cpuWrite(uint16_t addr, uint8_t data) {
    uint32_t mapped;
    if (mapper && mapper->cpuMapWrite(addr, mapped)) {
        return true; // NROM: no-op; PRG-RAM mappers would write here
    }
    return false;
}

bool Cartridge::ppuRead(uint16_t addr, uint8_t& data) {
    uint32_t mapped;
    if (mapper && mapper->ppuMapRead(addr, mapped)) {
        data = chrMemory[mapped];
        return true;
    }
    return false;
}

bool Cartridge::ppuWrite(uint16_t addr, uint8_t data) {
    uint32_t mapped;
    if (mapper && mapper->ppuMapWrite(addr, mapped)) {
        chrMemory[mapped] = data;
        return true;
    }
    return false;
}