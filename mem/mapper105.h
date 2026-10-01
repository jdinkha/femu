#pragma once
#include "banked_mapper.h"

// NES-EVENT (mapper 105, Nintendo World Championships 1990): an MMC1 - same
// serial register protocol - wired to two 128KB PRG chips and a 30-bit M2
// counter that ends the competition with an IRQ.
//   $8000 control: bits 0-1 mirroring, bits 2-3 PRG mode (as MMC1).
//   $A000: bit 4 = I (1 holds the timer at 0 and acknowledges its IRQ;
//          0 lets it run), bit 3 = chip (0: first chip, 32KB bank from
//          bits 1-2; 1: second chip, MMC1-style banking from $E000).
//   $E000: bits 0-3 PRG bank on the second chip, bit 4 PRG-RAM disable.
// Until I has gone 0 then 1 once, the first 32KB stays mapped. The IRQ
// fires when the counter reaches $20000000 plus the DIP switches' bits
// 25-28; femu uses the tournament setting (only switch C on, $28000000,
// about 6.25 minutes).
class Mapper_105 : public BankedMapper {
public:
    Mapper_105(uint8_t prgBanks, uint8_t chrBanks);

    bool prgRamEnabled() const override { return !(prg_b & 0x10); }
    bool irqState() const override { return irq_pending; }
    void cpuClock() override;

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override;

private:
    uint8_t shift = 0, shift_count = 0;
    uint8_t control = 0x0C, reg_a = 0x10, prg_b = 0;
    bool seen_zero = false, unlocked = false;
    uint32_t counter = 0;
    bool irq_pending = false;

    static constexpr uint32_t kTimerTarget = 0x28000000;

    void Update();
};
