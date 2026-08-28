#include "cartridge.h"
#include "mapper000.h"
#include <fstream>

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
    mirror   = (header.mapper1 & 0x01) ? Mirror::VERTICAL : Mirror::HORIZONTAL;

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
        default:
            return; // unsupported mapper - add Mapper_001/004 etc. here as you build them
    }

    valid = true;
}

bool Cartridge::cpuRead(uint16_t addr, uint8_t& data) {
    uint32_t mapped_addr = 0;
    if (mapper && mapper->cpuMapRead(addr, mapped_addr)) {
        data = prgMemory[mapped_addr];
        return true;
    }
    return false;
}

bool Cartridge::cpuWrite(uint16_t addr, uint8_t data) {
    uint32_t mapped_addr = 0;
    if (mapper && mapper->cpuMapWrite(addr, mapped_addr)) {
        // NROM's PRG-ROM ignores this in practice; mappers with PRG-RAM
        // (or bank-select registers, e.g. MMC1/MMC3) will use mapped_addr
        // or intercept the raw addr/data themselves once you add them.
        (void)data;
        return true;
    }
    return false;
}

bool Cartridge::ppuRead(uint16_t addr, uint8_t& data) {
    uint32_t mapped_addr = 0;
    if (mapper && mapper->ppuMapRead(addr, mapped_addr)) {
        data = chrMemory[mapped_addr];
        return true;
    }
    return false;
}

bool Cartridge::ppuWrite(uint16_t addr, uint8_t data) {
    uint32_t mapped_addr = 0;
    if (mapper && mapper->ppuMapWrite(addr, mapped_addr)) {
        chrMemory[mapped_addr] = data;
        return true;
    }
    return false;
}
