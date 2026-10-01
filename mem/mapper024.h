#pragma once
#include "banked_mapper.h"
#include "vrc_irq.h"

// Konami VRC6 (mappers 24 and 26, which swap address lines A0/A1):
// Akumajou Densetsu, Madara, Esper Dream 2. Registers (mapper 24 order):
//   $8000-$8003 16KB PRG bank at $8000
//   $C000-$C003 8KB PRG bank at $C000 ($E000 is the last bank)
//   $B003       banking style: bits 0-1 CHR layout, bits 2-3 mirroring,
//               bit 5 2KB-bank A10 passthrough, bit 7 PRG-RAM enable
//   $D000-$E003 CHR registers R0-R7
//   $F000-$F002 IRQ latch, control, acknowledge (shared VRC IRQ)
//   $9000-$B002 expansion audio: two pulse channels and a sawtooth
// The commercial games only use banking-style values $20-$2C (plus bit 7):
// CIRAM nametables with V/H/one-screen mirroring. ROM nametables and the
// other mirroring layouts aren't emulated.
class Mapper_024 : public BankedMapper {
public:
    Mapper_024(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId);

    bool prgRamEnabled() const override { return (style & 0x80) != 0; }
    bool irqState() const override { return irq.pending; }
    void cpuClock() override;
    float audioSample() const override { return audio_out; }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override;

private:
    bool swap_lines;
    uint8_t style = 0;
    uint8_t chr[8] = {0};
    VrcIrq irq;

    struct Pulse {
        uint8_t control = 0;   // MDDD VVVV
        uint16_t period = 0;
        bool enabled = false;
        uint16_t divider = 0;
        uint8_t step = 15;
    } pulse[2];
    struct Saw {
        uint8_t rate = 0;
        uint16_t period = 0;
        bool enabled = false;
        uint16_t divider = 0;
        uint8_t step = 0;
        uint8_t accumulator = 0;
    } saw;
    uint8_t freq_control = 0;  // bit 0 halt, bit 1 16x, bit 2 256x
    float audio_out = 0.0f;

    void UpdateChr();
    uint16_t Period(uint16_t period) const;
};
