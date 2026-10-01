#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "mapper.h"

struct StateWriter;
struct StateReader;

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
    // Which 1KB CIRAM page (0/1) nametable quadrant 0-3 lives in.
    uint8_t NametablePage(uint8_t quadrant) const;

    // Mapper IRQs and timing hooks; no-ops on mappers that don't use them.
    bool irqState() const { return mapper ? mapper->irqState() : false; }
    void irqClear()        { if (mapper) mapper->irqClear(); }
    void ScanlineIRQ()     { if (mapper) mapper->ScanlineIRQ(); }
    void CpuClock()        { if (mapper) mapper->cpuClock(); }
    void PpuScanline(int16_t scanline, bool rendering) { if (mapper) mapper->ppuScanline(scanline, rendering); }
    void PpuSpriteFetch(bool active) { if (mapper) mapper->ppuSpriteFetch(active); }

    // Expansion audio (VRC6, N163, MMC5...), mixed in by the bus.
    float AudioSample() const { return mapper ? mapper->audioSample() : 0.0f; }

    bool hasBattery() const { return battery_backed; }
    void SaveRAM() const; // writes prg_ram to sav_path if battery_backed

    // Stable fingerprint of the ROM's PRG+CHR contents (plus mapper ID),
    // computed once at load. Savestates embed this so a state can only be
    // reloaded against the game it was made from.
    uint64_t RomHash() const { return rom_hash; }

    // Savestate hooks: the mutable parts of the cartridge - PRG-RAM, CHR-RAM
    // (only when the board has no CHR-ROM), and the mapper's registers. PRG-ROM
    // and CHR-ROM are immutable and covered by RomHash() instead.
    void SerializeState(StateWriter& w) const;
    void DeserializeState(StateReader& r);

private:
    bool valid = false;
    uint8_t mapperID = 0;
    uint8_t nPRGBanks = 0;
    uint8_t nCHRBanks = 0;
    std::vector<uint8_t> prgMemory;
    std::vector<uint8_t> chrMemory; // CHR-ROM, then whatever CHR-RAM the board has
    size_t chr_rom_size = 0;
    std::unique_ptr<Mapper> mapper;

    // Extra nametable RAM on boards wired for four-screen mirroring (iNES
    // header flag); empty otherwise.
    std::vector<uint8_t> four_screen_vram;

    Mirror hw_mirror = Mirror::HORIZONTAL; // from the iNES header, used when mapper->mirror() reports HARDWARE

    // PRG-RAM, normally at $6000-$7FFF. Always allocated - 8KB unless the
    // mapper asks for a different size (NES 2.0 headers can specify it, but
    // we don't parse those) - since some mappers use this region as plain
    // work RAM even without a battery. Only persisted to disk if the iNES
    // header's battery flag is set.
    std::vector<uint8_t> prg_ram;
    bool battery_backed = false;
    std::string sav_path;

    uint64_t rom_hash = 0;
};
