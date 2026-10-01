#pragma once
#include "banked_mapper.h"

// Taito TC0190 (mapper 33: Akira, Don Doko Don, Insector X) and TC0690
// (mapper 48: Bubble Bobble 2 (J), Don Doko Don 2, Flintstones (J)).
//   $8000      8KB PRG bank at $8000; on the TC0190 bit 6 is mirroring
//              (0 = V)
//   $8001      8KB PRG bank at $A000 ($C000/$E000 are the last two)
//   $8002/3    2KB CHR banks at $0000/$0800 (bank number in 2KB units)
//   $A000-$A003  1KB CHR banks at $1000-$1C00
// The TC0690 moves mirroring to bit 6 of $E000 and adds an MMC3-style
// scanline IRQ at $C000-$C003 (reload - written inverted - clear, enable,
// acknowledge), which trips a few CPU cycles later than the MMC3's.
class Mapper_033 : public BankedMapper {
public:
    Mapper_033(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId)
        : BankedMapper(prgBanks, chrBanks), tc0690(mapperId == 48) {
        SetPrg8k(0, 0);
        SetPrg8k(1, 0);
        SetPrg8k(2, -2);
        SetPrg8k(3, -1);
        mirror_mode = Mirror::VERTICAL;
    }

    bool irqState() const override { return irq_pending; }
    void ScanlineIRQ() override;
    void cpuClock() override {
        if (irq_delay && --irq_delay == 0) irq_pending = true;
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override;

private:
    bool tc0690;
    uint8_t irq_latch = 0, irq_counter = 0;
    bool irq_reload = false, irq_enabled = false, irq_pending = false;
    uint8_t irq_delay = 0; // CPU cycles until a tripped IRQ reaches the CPU
};
