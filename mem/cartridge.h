#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "Mapper.h"

enum class Mirror { HORIZONTAL, VERTICAL, ONESCREEN_LO, ONESCREEN_HI };

class Cartridge {
public:
    explicit Cartridge(const std::string& path);
    bool imageValid() const { return valid; }

    bool cpuRead(uint16_t addr, uint8_t& data);
    bool cpuWrite(uint16_t addr, uint8_t data);
    bool ppuRead(uint16_t addr, uint8_t& data);
    bool ppuWrite(uint16_t addr, uint8_t data);

    Mirror mirror = Mirror::HORIZONTAL;

private:
    bool valid = false;
    uint8_t mapperID = 0;
    uint8_t nPRGBanks = 0;
    uint8_t nCHRBanks = 0;

    std::vector<uint8_t> prgMemory;
    std::vector<uint8_t> chrMemory;
    std::unique_ptr<Mapper> mapper;
}; 