#pragma once
#include "mapper.h"
#include <array>

// Shared plumbing for the many boards that boil down to "some switchable PRG
// and CHR windows plus a mirroring bit". It keeps an 8KB-granular map of
// $6000-$FFFF and a 1KB-granular map of $0000-$1FFF; a derived mapper only
// decodes its registers (WriteRegister for $8000-$FFFF, cpuWriteHook for
// anything lower) and calls the Set* helpers. Bank numbers wrap around the
// memory that's actually there, and negative numbers count back from the
// end (-1 = last bank).
class BankedMapper : public Mapper {
public:
    BankedMapper(uint8_t prgBanks, uint8_t chrBanks);

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override;
    bool prgRamMap(uint16_t addr, uint32_t& offset) override;

    Mirror mirror() const override { return mirror_mode; }

    // Persists the bank maps and mirroring; a derived class serializes its
    // own registers and calls these first.
    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    // A CPU write to $8000-$FFFF.
    virtual void WriteRegister(uint16_t addr, uint8_t data) { (void)addr; (void)data; }

    // PRG windows: slot 0-3 = $8000/$A000/$C000/$E000 for 8KB; 0-1 for 16KB.
    void SetPrg8k(int slot, int bank);
    void SetPrg16k(int slot, int bank);
    void SetPrg32k(int bank);
    // Puts 8KB of PRG-ROM at $6000-$7FFF; ClearPrg6000 gives it back to PRG-RAM.
    void SetPrg6000(int bank);
    void ClearPrg6000() { prg6000_rom = false; }

    // CHR windows, counted in their own size: slot 0-7 for 1KB, 0-3 for 2KB,
    // 0-1 for 4KB. Banks index CHR-ROM, or CHR-RAM on a board without ROM.
    void SetChr1k(int slot, int bank);
    void SetChr2k(int slot, int bank);
    void SetChr4k(int slot, int bank);
    void SetChr8k(int bank);
    // Maps a 1KB window to CHR-RAM explicitly, for boards with both ROM and RAM.
    void SetChrRam1k(int slot, int bank);

    // The ROM byte a write to `addr` collides with, ANDed into `data`.
    uint8_t BusConflict(uint16_t addr, uint8_t data) const;

    uint32_t Prg8kCount() const { return (uint32_t)nPRGBanks * 2; }
    uint32_t Chr1kCount() const { return nCHRBanks ? (uint32_t)nCHRBanks * 8 : chr_ram_size / 0x400; }
    uint32_t ChrRam1kCount() const { return chr_ram_size / 0x400; }

    Mirror mirror_mode = Mirror::HARDWARE;

private:
    std::array<uint32_t, 4> prg_map{};  // byte offsets into PRG-ROM for $8000-$FFFF
    uint32_t prg6000 = 0;
    bool prg6000_rom = false;
    std::array<uint32_t, 8> chr_map{};  // byte offsets into CHR (ROM then RAM)
    std::array<bool, 8> chr_writable{};
};
