#pragma once
#include "banked_mapper.h"

// Mapper 34 is two unrelated boards:
// - BNROM (Deadly Towers): one 32KB PRG bank register at $8000-$FFFF, 8KB
//   CHR-RAM.
// - NINA-001 (Impossible Mission II): registers at $7FFD (32KB PRG bank),
//   $7FFE and $7FFF (4KB CHR banks), which sit on top of PRG-RAM - writes go
//   to both.
// NES 2.0 submapper 1 means NINA-001, 2 means BNROM; otherwise more than 8KB
// of CHR-ROM means NINA-001.
class Mapper_034 : public BankedMapper {
public:
    Mapper_034(uint8_t prgBanks, uint8_t chrBanks, uint8_t submapper)
        : BankedMapper(prgBanks, chrBanks),
          nina(submapper == 1 || (submapper != 2 && chrBanks > 1)) {
        SetPrg32k(0);
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        if (!nina || addr < 0x7FFD || addr > 0x7FFF) return false;
        if (addr == 0x7FFD)      SetPrg32k(data & 0x01);
        else if (addr == 0x7FFE) SetChr4k(0, data & 0x0F);
        else                     SetChr4k(1, data & 0x0F);
        return false; // also lands in PRG-RAM underneath
    }

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        (void)addr;
        if (!nina) SetPrg32k(data);
    }

private:
    bool nina;
};
