#pragma once
#include <cstdint>
#include <array>

struct StateWriter;
struct StateReader;

// The VRC7's sound generator: a cut-down Yamaha YM2413 (OPLL) with six
// 2-operator FM channels and Konami's own fixed instrument set. Only
// Lagrange Point uses it.
//
// Registers (selected through $9010, written through $9030):
//   $00-$07  the custom instrument (patch 0)
//   $10-$15  channel frequency, low 8 bits
//   $20-$25  bit 5 sustain, bit 4 key on, bits 1-3 octave, bit 0 freq bit 8
//   $30-$35  bits 4-7 instrument, bits 0-3 volume (3dB steps, 0 loudest)
//
// The chip updates each channel 49716 times a second; this runs the same
// rate off the CPU clock. Waveforms, the FM/feedback structure, sustain/
// release rules, key scaling and the LFOs follow the documented behavior;
// the envelope rate timings follow the OPL family's rate structure rather
// than measured VRC7 tables, so envelope speeds are approximate.
class Vrc7Audio {
public:
    Vrc7Audio() { Reset(); }

    void Reset();
    void Write(uint8_t reg, uint8_t data);
    void Clock();                        // once per CPU cycle
    float Output() const { return output; }

    void Serialize(StateWriter& w) const;
    void Deserialize(StateReader& r);

private:
    enum EnvState : uint8_t { ATTACK, DECAY, SUSTAIN, RELEASE, OFF };

    struct Operator {
        uint32_t phase = 0;              // 19-bit phase accumulator
        double env = 127.0;              // attenuation in 0.375dB units, 0 = loudest
        EnvState state = OFF;
        double out = 0.0, prev = 0.0;    // last two outputs (feedback)
    };
    struct Channel {
        Operator mod, car;
        bool key = false;
    };

    std::array<uint8_t, 0x40> regs{};
    std::array<Channel, 6> ch{};
    uint8_t divider = 0;                 // CPU cycles toward the next 49716Hz sample
    uint32_t lfo_time = 0;               // samples since reset, for the LFOs
    float output = 0.0f;

    void Sample();
    void KeyOn(Channel& c);
    void KeyOff(Channel& c);
    const uint8_t* Patch(int channel) const;
    void Envelope(Operator& op, const uint8_t* patch, bool carrier, int channel, int rks);
};
