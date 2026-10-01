#pragma once
#include <cstdint>
#include <vector>

struct StateWriter;
struct StateReader;

// HARDWARE means "this mapper doesn't control mirroring itself - use
// whatever the iNES header said." NROM relies on this; MMC1 and MMC3 both
// override mirror() to report their own dynamically-selected mode instead.
enum class Mirror { HORIZONTAL, VERTICAL, ONESCREEN_LO, ONESCREEN_HI, HARDWARE };

// Base class for cartridge mapper chips. A mapper's whole job is address
// translation: given a CPU or PPU address, decide whether this mapper
// handles it and, if so, what physical offset into PRG/CHR memory it
// corresponds to. Bank-switching state (for MMC1/MMC3 etc.) lives in the
// derived class.
//
// Beyond the four translation functions, everything below is an optional
// hook with a default that matches a plain board (8KB PRG-RAM at $6000,
// CIRAM nametables per mirror(), no IRQ, no audio), for the boards that need
// more - registers outside $8000-$FFFF, cycle-counting IRQs, PPU fetch
// snooping, expansion audio and so on.
class Mapper {
public:
    Mapper(uint8_t prgBanks, uint8_t chrBanks)
        : nPRGBanks(prgBanks), nCHRBanks(chrBanks), chr_ram_size(chrBanks == 0 ? 0x2000 : 0) {}
    virtual ~Mapper() = default;

    // Each returns true if this mapper claims the address, and writes the
    // resulting physical offset into mapped_addr. cpuMapWrite takes the
    // written byte itself (not just the address) because bank-switching
    // mappers treat writes to $8000+ as register writes, not memory writes -
    // NROM ignores the value, MMC1/MMC3 depend entirely on it.
    //
    // CHR offsets index CHR-ROM followed by CHR-RAM: a board with both maps
    // its RAM at offsets from nCHRBanks * 8KB up. A mapper may also claim
    // nametable addresses ($2000-$3EFF) here to serve them from CHR.
    virtual bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) = 0;
    virtual bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) = 0;

    virtual void reset() {}

    // Savestate hooks for mapper bank-switching / IRQ state. The default is a
    // no-op, which is exactly right for fixed mappers like NROM; MMC1 and MMC3
    // override these to persist their registers. nPRGBanks / nCHRBanks come
    // from the ROM and are never serialized.
    virtual void SerializeState(StateWriter&) const {}
    virtual void DeserializeState(StateReader&) {}

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

    // ---- Memory the cartridge allocates for this board ----

    // Read by Cartridge once, right after construction; a derived constructor
    // changes them if its board differs from 8KB PRG-RAM / 8KB CHR-RAM-if-no-
    // CHR-ROM. PRG-RAM is what gets battery-saved, so boards whose save
    // memory is internal to the mapper (EEPROM, on-chip RAM) put it here too.
    uint32_t PrgRamSize() const { return prg_ram_size; }
    uint32_t ChrRamSize() const { return chr_ram_size; }
    // NES 2.0 headers state the sizes outright; Cartridge applies them here.
    // (Virtual so a board that keeps chip-internal RAM inside PRG-RAM can
    // add it on top of whatever the header says.)
    virtual void SetRamSizes(uint32_t prg, uint32_t chr) { prg_ram_size = prg; chr_ram_size = chr; }

    // Called by Cartridge once its memory exists, for boards that need to
    // touch it directly (bus conflicts read PRG-ROM; EEPROMs live in PRG-RAM).
    virtual void AttachMemory(const std::vector<uint8_t>* prg, std::vector<uint8_t>* prgram) {
        prg_rom = prg;
        prg_ram = prgram;
    }

    // ---- CPU side ----

    // Every CPU access reaches these first, at any address. Return true to
    // claim it (a register, on-chip RAM, an audio port...); return false to
    // let it continue as normal - which also lets a board just watch, e.g.
    // MMC5 snooping writes to PPUCTRL.
    virtual bool cpuReadHook(uint16_t addr, uint8_t& data) { (void)addr; (void)data; return false; }
    virtual bool cpuWriteHook(uint16_t addr, uint8_t data) { (void)addr; (void)data; return false; }

    // Where a CPU address lands in PRG-RAM, consulted for $4020-$FFFF before
    // cpuMapRead/Write. Return false for "not RAM" - open bus, or PRG-ROM
    // that cpuMapRead serves. Access is still gated by prgRamEnabled().
    virtual bool prgRamMap(uint16_t addr, uint32_t& offset) {
        if (addr < 0x6000 || addr > 0x7FFF) return false;
        offset = addr - 0x6000u;
        return true;
    }

    // Called once per CPU cycle, for IRQ counters clocked by M2 and for
    // expansion audio.
    virtual void cpuClock() {}

    // ---- PPU side ----

    // Every PPU access reaches these first. Return true to serve it directly
    // (MMC5 ExRAM / fill-mode nametables, extended attributes...).
    virtual bool ppuReadHook(uint16_t addr, uint8_t& data) { (void)addr; (void)data; return false; }
    virtual bool ppuWriteHook(uint16_t addr, uint8_t data) { (void)addr; (void)data; return false; }

    // Which CIRAM page (0 or 1) nametable quadrant 0-3 ($2000/$2400/$2800/
    // $2C00) uses, for boards that pick it per quadrant. -1 means "follow
    // mirror()".
    virtual int nametablePage(uint8_t quadrant) const { (void)quadrant; return -1; }

    // Called by the PPU at dot 4 of every scanline (-1 to 260), when MMC5
    // would detect it; `rendering` is whether background or sprites are on.
    virtual void ppuScanline(int16_t scanline, bool rendering) { (void)scanline; (void)rendering; }

    // Brackets the PPU's sprite pattern fetches, for boards that bank sprite
    // and background CHR separately (MMC5 8x16 mode).
    virtual void ppuSpriteFetch(bool active) { (void)active; }

    // ---- Expansion audio ----

    // This board's audio output for the current CPU cycle, on the same scale
    // as APU::GetOutputSample() (roughly 0..1); the bus adds the two.
    virtual float audioSample() const { return 0.0f; }

protected:
    uint8_t nPRGBanks = 0;
    uint8_t nCHRBanks = 0;

    uint32_t prg_ram_size = 0x2000;
    uint32_t chr_ram_size;

    const std::vector<uint8_t>* prg_rom = nullptr;
    std::vector<uint8_t>* prg_ram = nullptr;

    // For boards with bus conflicts: on a register write the CPU and the ROM
    // byte at that address both drive the data bus, so the mapper sees the
    // AND of the two. `rom_offset` is where the written address maps in PRG.
    uint8_t BusConflict(uint32_t rom_offset, uint8_t data) const {
        if (!prg_rom || prg_rom->empty()) return data;
        return data & (*prg_rom)[rom_offset % prg_rom->size()];
    }
};
