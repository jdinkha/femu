#pragma once
#include "banked_mapper.h"

// Tengen RAMBO-1 (mapper 64): Klax, Skull & Crossbones, Rolling Thunder;
// and its TLSROM-style variant (mapper 158, Alien Syndrome). An extended
// MMC3: $8000 (even) selects a register and holds the mode bits
//   bit 7 CHR A12 inversion, bit 6 PRG mode, bit 5 K (1KB CHR everywhere)
// $8001 (odd) writes it: R0/R1 2KB (or with K, 1KB alongside R8/R9) CHR,
// R2-R5 1KB CHR, R6/R7/RF 8KB PRG. $A000 bit 0 is mirroring (64 only);
// on 158, bit 7 of the CHR bank for $0000-$0FFF picks each nametable's
// CIRAM page instead, as on TxSROM.
// IRQ: $C000 reload value, $C001 bit 0 mode (0 scanline, 1 every 4 CPU
// cycles) and a reload request, $E000 disable + acknowledge, $E001 enable.
// Each clock reloads (forcing bit 0 on after a $C001 reload of a non-zero
// value) or decrements the counter; hitting 0 with IRQs on raises the IRQ
// 4 CPU cycles later.
class Mapper_064 : public BankedMapper {
public:
    Mapper_064(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId)
        : BankedMapper(prgBanks, chrBanks), tlsrom(mapperId == 158) {
        mirror_mode = Mirror::VERTICAL;
        Update();
    }

    int nametablePage(uint8_t quadrant) const override {
        return tlsrom ? chr_raw[quadrant & 3] >> 7 : -1;
    }

    bool irqState() const override { return irq_pending; }
    void ScanlineIRQ() override { if (!irq_cycle_mode) ClockCounter(); }
    void cpuClock() override;

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override;

private:
    bool tlsrom;
    uint8_t control = 0;
    uint8_t regs[16] = {0, 2, 4, 5, 6, 7, 0, 1, 1, 3, 0, 0, 0, 0, 0, 0};
    uint8_t chr_raw[8] = {0};

    uint8_t irq_latch = 0, irq_counter = 0;
    bool irq_reload = false, irq_enabled = false, irq_cycle_mode = false, irq_pending = false;
    uint8_t prescaler = 0, irq_delay = 0;

    void Update();
    void ClockCounter();
};
