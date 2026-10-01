#pragma once
#include "banked_mapper.h"

// AVE Maxi 15 (mapper 234), a multicart of CNROM and NINA-03 games. Its
// registers at $FF80-$FFF7 latch the value the CPU *reads* there (writes
// work too, with bus conflicts), so games switch banks by reading a table
// in ROM:
//   $FF80-$FF9F outer: bit 7 mirroring (0 = V), bit 6 mode (0 = CNROM,
//     1 = NINA-03), bit 5 second ROM pair, bits 0-3 block. Locked once any
//     of bits 0-5 is set.
//   $FFE8-$FFF7 inner: bits 4-6 CHR page, bit 0 PRG page.
// CNROM mode: PRG = block, CHR = block:CC. NINA-03 mode: PRG = block
// bits 1-3 : P, CHR = block bits 1-3 : cCC.
class Mapper_234 : public BankedMapper {
public:
    Mapper_234(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        prg_ram_size = 0;
        Update();
    }

    bool cpuReadHook(uint16_t addr, uint8_t& data) override {
        if (addr < 0xFF80 || addr > 0xFFF7) return false;
        uint32_t offset = 0;
        BankedMapper::cpuMapRead(addr, offset);
        data = (*prg_rom)[offset % prg_rom->size()];
        Latch(addr, data);
        return true;
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        if (addr >= 0xFF80 && addr <= 0xFFF7) Latch(addr, BusConflict(addr, data));
    }

private:
    uint8_t outer = 0, inner = 0;

    void Latch(uint16_t addr, uint8_t value) {
        if (addr <= 0xFF9F) {
            if (outer & 0x3F) return; // locked
            outer = value;
        } else if (addr >= 0xFFE8) {
            inner = value & 0x71;
        } else {
            return; // $FFC0-$FFDF lockout defeat, nothing to emulate
        }
        Update();
    }

    void Update() {
        int block = outer & 0x0F;
        int prg, chr;
        if (outer & 0x40) { // NINA-03
            prg = (block & 0x0E) | (inner & 0x01);
            chr = ((block & 0x0E) << 2) | ((inner >> 4) & 0x07);
        } else {            // CNROM
            prg = block;
            chr = (block << 2) | ((inner >> 4) & 0x03);
        }
        if (outer & 0x20) { prg += 16; chr += 64; } // ROMs 3+4: the second 512KB of each
        SetPrg32k(prg);
        SetChr8k(chr);
        mirror_mode = (outer & 0x80) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
    }
};
