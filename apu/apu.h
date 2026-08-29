#pragma once
#include <cstdint>
#include <array>

class Bus;

// 2A03 Audio Processing Unit.
//
// Implements all five channels - two pulse, triangle, noise, and DMC - with
// duty cycles, envelopes, sweep (pulse only), length counters, the frame
// sequencer that clocks all of that on schedule, and DMC's own memory-reader
// state machine. Produces a mixed analog-ish sample on demand via
// GetOutputSample().
class APU {
public:
    APU();

    void ConnectBus(Bus* b) { bus = b; } // DMC needs this to fetch sample bytes

    void cpuWrite(uint16_t addr, uint8_t data);
    uint8_t cpuRead(uint16_t addr); // only $4015 (status) is actually readable

    void clock(); // call once per CPU cycle
    void reset();

    // True if either the frame sequencer or the DMC wants to raise an IRQ.
    // Bus checks this every clock and calls cpu.irq() - safe to call
    // unconditionally every cycle since CPU's own irq() no-ops once the I
    // flag is set, so this doesn't cause runaway re-triggering.
    bool IRQPending() const { return frame_irq || dmc.irq_flag; }

    // Mixed output in roughly [0.0, 1.0], using the standard NES non-linear
    // pulse/triangle-noise-dmc mixing approximation.
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

    struct DMC {
        bool irq_enable = false;
        bool loop = false;
        uint16_t timer_period = 428; // NTSC default (rate index 0)
        uint16_t timer_value = 0;

        uint16_t sample_address = 0xC000; // reload value for current_address
        uint16_t sample_length = 1;       // reload value for bytes_remaining

        uint16_t current_address = 0xC000;
        uint16_t bytes_remaining = 0;     // >0 means "actively playing a sample"

        uint8_t sample_buffer = 0;
        bool sample_buffer_filled = false;

        uint8_t shift_register = 0;
        uint8_t bits_remaining = 8;
        bool silence = true;

        uint8_t output_level = 0; // this IS the output - no length-counter-style mute gate
        bool irq_flag = false;

        void WriteReg0(uint8_t data); // $4010: IRQ enable, loop, rate
        void WriteReg1(uint8_t data); // $4011: direct output-level load
        void WriteReg2(uint8_t data); // $4012: sample address
        void WriteReg3(uint8_t data); // $4013: sample length
        void SetEnabled(bool en);     // from $4015 bit 4

        // Real hardware fetches sample bytes via DMA, which can briefly
        // stall the CPU (~4 cycles, more if it collides with OAMDMA). We
        // fetch instantly instead - same simplification already made for
        // OAMDMA, and for the same reason: correctness without needing to
        // model cycle-exact bus contention.
        void FillSampleBufferIfNeeded(Bus* bus);
        void ClockTimer(Bus* bus);

        uint8_t Output() const { return output_level; }
    };

    Pulse pulse1;
    Pulse pulse2;
    Triangle triangle;
    Noise noise;
    DMC dmc;

    Bus* bus = nullptr;

    // Frame sequencer
    bool five_step_mode = false;
    bool irq_inhibit = false;
    bool frame_irq = false;
    uint32_t frame_cycle_counter = 0;

    uint32_t cpu_cycle_count = 0; // used to gate pulse/noise timers to half-CPU-rate
    double last_sample = 0.0;

    static const std::array<uint8_t, 32> length_table;
};
