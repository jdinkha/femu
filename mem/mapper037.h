#pragma once
#include "mapper004.h"

// MMC3 multicart (mapper 37): Super Mario Bros. + Tetris + Nintendo World
// Cup. In place of PRG-RAM, $6000-$7FFF holds an outer bank register
// (written only while the MMC3 has PRG-RAM enabled and writable). With its
// low three bits QBB:
//   PRG A16 = (BB == 3) | (Q & MMC3 A16), PRG A17 = CHR A17 = Q
// giving 64KB for SMB, 64KB for Tetris and 128KB for World Cup.
class Mapper_037 : public Mapper_004 {
public:
    Mapper_037(uint8_t prgBanks, uint8_t chrBanks) : Mapper_004(prgBanks, chrBanks) {
        prg_ram_size = 0;
        UpdateBanks();
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        if (addr < 0x6000 || addr > 0x7FFF) return false;
        if (prg_ram_enabled && prg_ram_writable) {
            outer = data & 0x07;
            UpdateBanks();
        }
        return true;
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    uint32_t PrgBank(uint8_t raw) const override {
        uint8_t a16 = ((outer & 0x03) == 0x03) | ((outer >> 2) & (raw >> 3) & 0x01);
        uint8_t q = (outer >> 2) & 0x01;
        return Mapper_004::PrgBank((uint8_t)((raw & 0x07) | (a16 << 3) | (q << 4)));
    }
    uint32_t ChrBank(uint8_t raw) const override {
        return Mapper_004::ChrBank((uint8_t)((raw & 0x7F) | ((outer & 0x04) << 5)));
    }

private:
    uint8_t outer = 0;
};
