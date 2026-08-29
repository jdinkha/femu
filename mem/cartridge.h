#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "mapper.h"

class Cartridge {
public:
    explicit Cartridge(const std::string& path);
    ~Cartridge(); // auto-saves battery-backed RAM if present

    bool imageValid() const { return valid; }

    bool cpuRead(uint16_t addr, uint8_t& data);
    bool cpuWrite(uint16_t addr, uint8_t data);
    bool ppuRead(uint16_t addr, uint8_t& data);
    bool ppuWrite(uint16_t addr, uint8_t data);

    Mirror mirror() const;

    // MMC3's IRQ; no-ops on mappers that don't have one.
    bool irqState() const { return mapper ? mapper->irqState() : false; }
    void irqClear()        { if (mapper) mapper->irqClear(); }
    void ScanlineIRQ()     { if (mapper) mapper->ScanlineIRQ(); }

    bool hasBattery() const { return battery_backed; }
    void SaveRAM() const; // writes prg_ram to sav_path if battery_backed

private:
    bool valid = false;
    uint8_t mapperID = 0;
    uint8_t nPRGBanks = 0;
    uint8_t nCHRBanks = 0;
    std::vector<uint8_t> prgMemory;
    std::vector<uint8_t> chrMemory;
    std::unique_ptr<Mapper> mapper;

    Mirror hw_mirror = Mirror::HORIZONTAL; // from the iNES header, used when mapper->mirror() reports HARDWARE

    // PRG-RAM at $6000-$7FFF. Always allocated (8KB - the near-universal
    // size; NES 2.0 headers can specify otherwise but we don't parse those)
    // since some mappers use this region as plain work RAM even without a
    // battery. Only persisted to disk if the iNES header's battery flag is set.
    std::vector<uint8_t> prg_ram;
    bool battery_backed = false;
    std::string sav_path;
};
