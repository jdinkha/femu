#pragma once
#include "banked_mapper.h"

// Jaleco SS88006 (mapper 18): Pizza Pop!, Ninja Jajamaru: Ginga Daisakusen,
// Plasma Ball. Registers decode A0-A1 and A12-A15 (mask $F003); each bank
// number is written a nibble at a time:
//   $8000-$9001  8KB PRG banks at $8000/$A000/$C000, low 4 + high 2 bits
//   $9002        PRG-RAM: bit 0 enable, bit 1 allow writes
//   $A000-$D003  1KB CHR banks, low 4 + high 4 bits
//   $E000-$E003  IRQ reload value, 4 bits per register, low nibble first
//   $F000        reload the counter (and acknowledge)
//   $F001        bit 0 count enable; bits 1-3 shrink the counter to 12/8/4
//                bits (acknowledges)
//   $F002        mirroring: H, V, one-screen A, one-screen B
// The counter counts down every CPU cycle; the IRQ fires when its active
// low bits borrow past zero. $F003 drives a uPD7755C/7756C speech chip
// whose samples aren't in the dump, so it isn't emulated.
class Mapper_018 : public BankedMapper {
public:
    Mapper_018(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        SetPrg8k(0, 0);
        SetPrg8k(1, 0);
        SetPrg8k(2, 0);
        SetPrg8k(3, -1);
    }

    bool prgRamEnabled() const override { return ram_control & 0x01; }
    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        (void)data;
        // Writes need bit 1 too; while it's clear, swallow them.
        return addr >= 0x6000 && addr <= 0x7FFF && (ram_control & 0x03) == 0x01;
    }

    bool irqState() const override { return irq_pending; }
    void cpuClock() override {
        if (!(irq_control & 0x01)) return;
        uint16_t mask = (irq_control & 0x08) ? 0x000F : (irq_control & 0x04) ? 0x00FF
                      : (irq_control & 0x02) ? 0x0FFF : 0xFFFF;
        uint16_t low = irq_counter & mask;
        if (low == 0) irq_pending = true;
        irq_counter = (uint16_t)((irq_counter & ~mask) | ((low - 1) & mask));
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override;

private:
    uint8_t prg[3] = {0, 0, 0};
    uint8_t chr[8] = {0};
    uint8_t ram_control = 0;
    uint16_t irq_reload = 0, irq_counter = 0;
    uint8_t irq_control = 0;
    bool irq_pending = false;
};
