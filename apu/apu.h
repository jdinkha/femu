#pragma once
#include <cstdint>
#include <array>

// 2A03 Audio Processing Unit.
//
// Implements the four "easy" channels - two pulse, triangle, noise - with
// duty cycles, envelopes, sweep (pulse only), length counters, and the
// frame sequencer that clocks all of that on schedule. Produces a mixed
// analog-ish sample on demand via GetOutputSample().
//
// NOT implemented: the DMC (delta modulation / sample-playback) channel.
// It needs its own DMA reads from PRG-ROM and is the least commonly load-
// bearing channel for a game's core audio - deliberately last on the list,
// same as the original build-order plan.
class APU {
public:
    APU();

    void cpuWrite(uint16_t addr, uint8_t data);
    uint8_t cpuRead(uint16_t addr); // only $4015 (status) is actually readable

    void clock(); // call once per CPU cycle
    void reset();

    // Mixed output in roughly [0.0, 1.0], using the standard NES non-linear
    // pulse/triangle-noise-dmc mixing approximation (with dmc term = 0).
    double GetOutputSample() const { return last_sample; }

private:
    struct Pulse {
        bool enabled = false;
        uint8_t duty_cycle = 0;
        uint8_t duty_pos = 0;

        uint16_t timer_period = 0;
        uint16_t timer_value = 0;

        // Envelope
        bool envelope_start = false;
        bool constant_volume = false;
        bool envelope_loop = false; // shared bit also used as length-counter halt
        uint8_t envelope_volume = 0; // constant volume, or envelope divider period
        uint8_t envelope_decay = 0;
        uint8_t envelope_divider = 0;

        // Sweep
        bool sweep_enabled = false;
        bool sweep_negate = false;
        bool sweep_reload = false;
        uint8_t sweep_period = 0;
        uint8_t sweep_divider = 0;
        uint8_t sweep_shift = 0;
        bool is_pulse1 = true; // pulse1's negate subtracts one extra (one's-complement quirk)

        uint8_t length_counter = 0;

        void WriteReg0(uint8_t data);
        void WriteReg1(uint8_t data);
        void WriteReg2(uint8_t data);
        void WriteReg3(uint8_t data);

        void ClockTimer();
        void ClockEnvelope();
        void ClockSweep();
        void ClockLength();

        uint16_t TargetPeriod() const;
        bool IsMuted() const;
        uint8_t Output() const;
    };

    struct Triangle {
        bool enabled = false;
        static const uint8_t sequence[32];
        uint8_t seq_pos = 0;

        uint16_t timer_period = 0;
        uint16_t timer_value = 0;

        bool length_halt = false; // also controls linear-counter control flag
        uint8_t length_counter = 0;

        bool linear_reload_flag = false;
        uint8_t linear_reload_value = 0;
        uint8_t linear_counter = 0;

        void WriteReg0(uint8_t data);
        void WriteReg2(uint8_t data);
        void WriteReg3(uint8_t data);

        void ClockTimer();
        void ClockLinear();
        void ClockLength();

        uint8_t Output() const;
    };

    struct Noise {
        bool enabled = false;

        uint16_t timer_period = 0;
        uint16_t timer_value = 0;
        uint16_t shift_register = 1;
        bool mode_flag = false; // "loop noise" - short (93-step) vs long (32767-step) sequence

        bool envelope_start = false;
        bool constant_volume = false;
        bool envelope_loop = false; // shared with length-counter halt
        uint8_t envelope_volume = 0;
        uint8_t envelope_decay = 0;
        uint8_t envelope_divider = 0;

        uint8_t length_counter = 0;

        void WriteReg0(uint8_t data);
        void WriteReg2(uint8_t data);
        void WriteReg3(uint8_t data);

        void ClockTimer();
        void ClockEnvelope();
        void ClockLength();

        uint8_t Output() const;
    };

    Pulse pulse1;
    Pulse pulse2;
    Triangle triangle;
    Noise noise;

    // Frame sequencer
    bool five_step_mode = false;
    bool irq_inhibit = false;
    bool frame_irq = false;
    uint32_t frame_cycle_counter = 0;

    uint32_t cpu_cycle_count = 0; // used to gate pulse/noise timers to half-CPU-rate
    double last_sample = 0.0;

    static const std::array<uint8_t, 32> length_table;
};
