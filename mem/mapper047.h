#pragma once
#include "mapper004.h"

// MMC3 multicart (mapper 47): Super Spike V'Ball + Nintendo World Cup. In
// place of PRG-RAM, bit 0 of a $6000-$7FFF register (written only while the
// MMC3 has PRG-RAM enabled and writable) picks a 128KB PRG + 128KB CHR block.
class Mapper_047 : public Mapper_004 {
public:
    Mapper_047(uint8_t prgBanks, uint8_t chrBanks) : Mapper_004(prgBanks, chrBanks) {
        prg_ram_size = 0;
        UpdateBanks();
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        if (addr < 0x6000 || addr > 0x7FFF) return false;
        if (prg_ram_enabled && prg_ram_writable) {
            block = data & 0x01;
            UpdateBanks();
        }
        return true;
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    uint32_t PrgBank(uint8_t raw) const override {
        return Mapper_004::PrgBank((uint8_t)((raw & 0x0F) | (block << 4)));
    }
    uint32_t ChrBank(uint8_t raw) const override {
        return Mapper_004::ChrBank((uint8_t)((raw & 0x7F) | (block << 7)));
    }

private:
    uint8_t block = 0;
};
