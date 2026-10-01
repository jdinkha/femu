#pragma once
#include "banked_mapper.h"

// Sunsoft-4 (mapper 68): After Burner, Maharaja, Nantettatte!! Baseball.
//   $8000/$9000/$A000/$B000  2KB CHR banks
//   $C000/$D000  1KB CHR-ROM pages for the low/high nametable when ROM
//                nametables are on (bit 7 forced, so the last 128KB)
//   $E000  bits 0-1 mirroring (V, H, one-screen low, high); bit 4 serves
//          the nametables from CHR-ROM instead of CIRAM
//   $F000  16KB PRG bank at $8000 (last bank fixed), bit 4 PRG-RAM enable
// Nantettatte!! Baseball's add-on ROM cartridge and its license timer
// aren't emulated.
class Mapper_068 : public BankedMapper {
public:
    Mapper_068(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        mirror_mode = Mirror::VERTICAL;
    }

    bool prgRamEnabled() const override { return ram_enabled; }

    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr >= 0x2000 && addr <= 0x3EFF && rom_nametables) {
            mapped_addr = (uint32_t)(nt[Page((addr >> 10) & 3)] | 0x80) * 0x400 + (addr & 0x3FF);
            return true;
        }
        return BankedMapper::ppuMapRead(addr, mapped_addr);
    }
    bool ppuWriteHook(uint16_t addr, uint8_t data) override {
        (void)data;
        return addr >= 0x2000 && addr <= 0x3EFF && rom_nametables; // ROM: ignore
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        switch (addr & 0xF000) {
            case 0x8000: case 0x9000: case 0xA000: case 0xB000:
                SetChr2k((addr >> 12) & 0x3, data);
                break;
            case 0xC000: nt[0] = data & 0x7F; break;
            case 0xD000: nt[1] = data & 0x7F; break;
            case 0xE000: {
                static const Mirror modes[4] = {Mirror::VERTICAL, Mirror::HORIZONTAL,
                                                Mirror::ONESCREEN_LO, Mirror::ONESCREEN_HI};
                mirror_mode = modes[data & 0x03];
                rom_nametables = data & 0x10;
                break;
            }
            case 0xF000:
                SetPrg16k(0, data & 0x0F);
                ram_enabled = data & 0x10;
                break;
        }
    }

private:
    uint8_t nt[2] = {0, 0};
    bool rom_nametables = false;
    bool ram_enabled = false;

    // Low (0) or high (1) nametable for a quadrant, per the mirroring.
    int Page(int quadrant) const {
        switch (mirror_mode) {
            case Mirror::VERTICAL:     return quadrant & 1;
            case Mirror::HORIZONTAL:   return quadrant >> 1;
            case Mirror::ONESCREEN_HI: return 1;
            default:                   return 0;
        }
    }
};
