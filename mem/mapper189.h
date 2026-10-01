#pragma once
#include "mapper004.h"

// MMC3 clones with 32KB PRG banking (mapper 189): Thunder Warrior, Street
// Fighter II: The World Warrior. The MMC3's own PRG banking is unused; a
// write anywhere in $4020-$7FFF picks the 32KB bank from the OR of the
// data's two nibbles, which covers every known board variant at once.
class Mapper_189 : public Mapper_004 {
public:
    Mapper_189(uint8_t prgBanks, uint8_t chrBanks) : Mapper_004(prgBanks, chrBanks) {
        prg_ram_size = 0;
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        if (addr < 0x4020 || addr > 0x7FFF) return false;
        prg_bank = (data | (data >> 4)) & 0x07;
        return true;
    }

    bool cpuMapRead(uint16_t addr, uint32_t& mapped_addr) override {
        if (addr < 0x8000) return false;
        mapped_addr = (uint32_t)prg_bank * 0x8000 + (addr & 0x7FFF);
        return true;
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

private:
    uint8_t prg_bank = 0;
};
