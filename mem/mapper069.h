#pragma once
#include "banked_mapper.h"
#include <array>

// Sunsoft FME-7 / 5A / 5B (mapper 69): Batman: Return of the Joker,
// Gimmick!, Hebereke. Write a command number to $8000-$9FFF, then its
// parameter to $A000-$BFFF:
//   $0-$7  1KB CHR banks
//   $8     $6000-$7FFF: bits 0-5 bank, bit 6 RAM (else ROM), bit 7 RAM
//          enable (RAM selected but disabled reads open bus)
//   $9-$B  8KB PRG banks at $8000/$A000/$C000 ($E000 is the last bank)
//   $C     mirroring: 0 vertical, 1 horizontal, 2/3 one-screen A/B
//   $D     IRQ control: bit 0 IRQ enable, bit 7 counter enable; any write
//          acknowledges the IRQ
//   $E/$F  IRQ counter low/high
// The 16-bit counter decrements every CPU cycle while enabled and raises the
// IRQ when it wraps from $0000 to $FFFF.
//
// The 5B (only Gimmick! uses it) adds a YM2149F-style sound generator:
// register select at $C000-$DFFF, data at $E000-$FFFF.
class Mapper_069 : public BankedMapper {
public:
    Mapper_069(uint8_t prgBanks, uint8_t chrBanks);

    bool prgRamMap(uint16_t addr, uint32_t& offset) override;
    bool irqState() const override { return irq_pending; }
    void cpuClock() override;
    float audioSample() const override { return audio_out; }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override;

private:
    uint8_t command = 0;
    uint8_t prg6000 = 0;
    uint8_t irq_control = 0;
    uint16_t irq_counter = 0;
    bool irq_pending = false;

    // ---- 5B audio ----
    uint8_t audio_select = 0;
    std::array<uint8_t, 16> audio_reg{};
    uint8_t prescaler = 0;               // CPU cycles toward the next 16-cycle tick
    bool noise_phase = false;            // noise runs at half the tone rate
    uint16_t tone_count[3] = {0, 0, 0};
    bool tone_out[3] = {false, false, false};
    uint8_t noise_count = 0;
    uint32_t lfsr = 1;
    uint16_t env_count = 0;
    int8_t env_step = 0;
    uint8_t env_attack = 0, env_hold = 0, env_alternate = 0;
    bool env_holding = false;
    float audio_out = 0.0f;

    void Command(uint8_t data);
    void AudioWrite(uint8_t reg, uint8_t data);
    void AudioTick();
};
