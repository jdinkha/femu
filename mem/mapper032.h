#pragma once
#include "banked_mapper.h"

// Irem G-101 (mapper 32): Image Fight, Major League, Kaiketsu Yanchamaru 2.
//   $8000  8KB PRG bank at $8000, or at $C000 in PRG mode 1
//   $9000  bit 0 mirroring (0 = V), bit 1 PRG mode (second-last bank goes
//          to whichever of $8000/$C000 the $8000 register isn't using)
//   $A000  8KB PRG bank at $A000 ($E000 is the last bank)
//   $B000-$B007  1KB CHR banks
// Major League (NES 2.0 submapper 1) has one-screen mirroring hardwired to
// page B and no $9000 register at all.
class Mapper_032 : public BankedMapper {
public:
    Mapper_032(uint8_t prgBanks, uint8_t chrBanks, uint8_t submapper)
        : BankedMapper(prgBanks, chrBanks), major_league(submapper == 1) {
        if (major_league) mirror_mode = Mirror::ONESCREEN_HI;
        Update();
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        switch (addr & 0xF000) {
            case 0x8000: prg[0] = data & 0x1F; Update(); break;
            case 0x9000:
                if (major_league) break;
                mirror_mode = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
                prg_mode = data & 0x02;
                Update();
                break;
            case 0xA000: prg[1] = data & 0x1F; Update(); break;
            case 0xB000: SetChr1k(addr & 0x07, data); break;
        }
    }

private:
    bool major_league;
    uint8_t prg[2] = {0, 0};
    bool prg_mode = false;

    void Update() {
        SetPrg8k(prg_mode ? 2 : 0, prg[0]);
        SetPrg8k(prg_mode ? 0 : 2, -2);
        SetPrg8k(1, prg[1]);
        SetPrg8k(3, -1);
    }
};
