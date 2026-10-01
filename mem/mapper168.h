#pragma once
#include "banked_mapper.h"

// RacerMate (mapper 168, RacerMate Challenge II): 16KB switchable PRG at
// $8000 (bits 6-7 of $8000-$BFFF, last bank fixed) and 64KB of CHR-RAM in
// 4KB pages - the first fixed at $0000, bits 0-3 picking the one at $1000.
// $C000-$FFFF bit 2 holds an M2 counter at 0 (and acknowledges its IRQ)
// while set; while clear the counter runs and /IRQ is asserted whenever
// its 1024s bit is. Pages 8-15 are battery-backed and protected until bit
// 2 goes 1 -> 0. The game also needs the RacerMate exercise bike, which
// femu doesn't emulate, and the CHR-RAM's battery backing isn't saved.
class Mapper_168 : public BankedMapper {
public:
    Mapper_168(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        prg_ram_size = 0;
        chr_ram_size = 0x10000;
        SetChr4k(0, 0);
        SetChr4k(1, 0);
    }

    bool irqState() const override { return (counter & 0x400) != 0; }
    void cpuClock() override { if (!irq_hold) counter++; }

    bool ppuReadHook(uint16_t addr, uint8_t& data) override {
        if (!Protected(addr)) return false;
        data = 0xFF;
        return true;
    }
    bool ppuWriteHook(uint16_t addr, uint8_t data) override { (void)data; return Protected(addr); }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        if (addr < 0xC000) {
            SetPrg16k(0, data >> 6);
            chr_page = data & 0x0F;
            SetChr4k(1, chr_page);
            return;
        }
        bool hold = data & 0x04;
        if (irq_hold && !hold) ram_protected = false;
        irq_hold = hold;
        if (irq_hold) counter = 0;
    }

private:
    uint8_t chr_page = 0;
    bool irq_hold = false;
    bool ram_protected = true;
    uint32_t counter = 0;

    bool Protected(uint16_t addr) const {
        return ram_protected && addr >= 0x1000 && addr <= 0x1FFF && chr_page >= 8;
    }
};
