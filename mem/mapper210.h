#pragma once
#include "banked_mapper.h"

// Namco 175 and 340 (mapper 210): the N163's simpler siblings - no audio,
// IRQ or nametable banking. Famista '91/'92/'93, Wagan Land 2/3, Splatterhouse.
//   $8000-$B800  1KB CHR banks (one register per $800)
//   $C000        175 only: bit 0 enables PRG-RAM
//   $E000        8KB PRG at $8000; on the 340, bits 6-7 pick the mirroring
//                (one-screen A, vertical, one-screen B, horizontal)
//   $E800 / $F000  8KB PRG at $A000 / $C000 ($E000 is the last bank)
// NES 2.0 submapper 1 is the 175 (hardwired mirroring, PRG-RAM) and 2 the
// 340. Without one, both are emulated at once: the commercial 175 games
// write $E000's top bits to match their wiring, so honoring them is safe.
class Mapper_210 : public BankedMapper {
public:
    Mapper_210(uint8_t prgBanks, uint8_t chrBanks, uint8_t submapper)
        : BankedMapper(prgBanks, chrBanks), n175(submapper != 2), n340(submapper != 1) {
        if (!n175) prg_ram_size = 0;
        SetPrg8k(0, 0);
        SetPrg8k(1, 0);
        SetPrg8k(2, 0);
        SetPrg8k(3, -1);
    }

    bool prgRamEnabled() const override { return ram_enabled; }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        const int reg = (addr - 0x8000) >> 11;
        if (reg < 8) {
            SetChr1k(reg, data);
        } else if (reg == 8 && n175) {
            ram_enabled = data & 0x01;
        } else if (reg == 12) {
            SetPrg8k(0, data & 0x3F);
            if (n340) {
                static const Mirror modes[4] = {Mirror::ONESCREEN_LO, Mirror::VERTICAL,
                                                Mirror::ONESCREEN_HI, Mirror::HORIZONTAL};
                mirror_mode = modes[data >> 6];
            }
        } else if (reg == 13) {
            SetPrg8k(1, data & 0x3F);
        } else if (reg == 14) {
            SetPrg8k(2, data & 0x3F);
        }
    }

private:
    bool n175, n340;
    bool ram_enabled = false;
};
