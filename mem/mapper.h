#pragma once
#include <cstdint>

// HARDWARE means "this mapper doesn't control mirroring itself - use
// whatever the iNES header said." NROM relies on this; MMC1 and MMC3 both
// override mirror() to report their own dynamically-selected mode instead.
enum class Mirror { HORIZONTAL, VERTICAL, ONESCREEN_LO, ONESCREEN_HI, HARDWARE };

// Base class for cartridge mapper chips. A mapper's whole job is address
// translation: given a CPU or PPU address, decide whether this mapper
// handles it and, if so, what physical offset into PRG/CHR memory it
// corresponds to. Bank-switching state (for MMC1/MMC3 etc.) lives in the
// derived class.
class Mapper {
public:
    Mapper(uint8_t prgBanks, uint8_t chrBanks) : nPRGBanks(prgBanks), nCHRBanks(chrBanks) {}
    virtual ~Mapper() = default;

    // Each returns true if this mapper claims the address, and writes the
    // resulting physical offset into mapped_addr. cpuMapWrite takes the
    // written byte itself (not just the address) because bank-switching
    // mappers treat writes to $8000+ as register writes, not memory writes -
    // NROM ignores the value, MMC1/MMC3 depend entirely on it.
    virtual bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) = 0;
    virtual bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) = 0;

    virtual void reset() {}

    // Most mappers don't touch mirroring or PRG-RAM gating; MMC1/MMC3 override these.
    virtual Mirror mirror() const { return Mirror::HARDWARE; }
    virtual bool prgRamEnabled() const { return true; }

    // MMC3's scanline-counting IRQ. ScanlineIRQ() is called once per visible
    // scanline by the PPU (approximating real hardware's PPU-A12-toggle-based
    // counting, which is far more precise but requires tracking every VRAM
    // address the PPU touches mid-scanline - this "once per scanline"
    // approximation is what most hobbyist emulators use and is correct for
    // the vast majority of MMC3 games' split-screen/status-bar effects).
    virtual bool irqState() const { return false; }
    virtual void irqClear() {}
    virtual void ScanlineIRQ() {}

protected:
    uint8_t nPRGBanks = 0;
    uint8_t nCHRBanks = 0;
};
