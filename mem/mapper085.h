#pragma once
#include "banked_mapper.h"
#include "vrc_irq.h"
#include "vrc7_audio.h"

// Konami VRC7 (mapper 85): Lagrange Point, Tiny Toon Adventures 2 (J).
// Each register group has a second register on A4 (VRC7a, Lagrange Point,
// NES 2.0 submapper 2) or A3 (VRC7b, Tiny Toon 2, submapper 1); without a
// submapper both are accepted.
//   $8000 / $8010   8KB PRG banks at $8000 / $A000
//   $9000           8KB PRG bank at $C000 ($E000 is the last bank)
//   $9010 / $9030   audio register select / data (see Vrc7Audio)
//   $A000-$D010     eight 1KB CHR banks
//   $E000           bits 0-1 mirroring, bit 6 audio reset/silence,
//                   bit 7 PRG-RAM enable
//   $E010 / $F000 / $F010  IRQ latch / control / acknowledge (VRC IRQ)
class Mapper_085 : public BankedMapper {
public:
    Mapper_085(uint8_t prgBanks, uint8_t chrBanks, uint8_t submapper);

    bool prgRamEnabled() const override { return (control & 0x80) != 0; }
    bool irqState() const override { return irq.pending; }
    void cpuClock() override;
    float audioSample() const override { return (control & 0x40) ? 0.0f : audio.Output(); }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override;

private:
    uint16_t second_mask;   // address bit(s) that select a group's second register
    uint8_t control = 0;    // $E000
    uint8_t audio_select = 0;
    VrcIrq irq;
    Vrc7Audio audio;
};
