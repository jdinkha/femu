#include "apu.h"
#include "../mem/bus.h"
#include "../mem/serialize.h"

void APU::SerializeState(StateWriter& w) const {
    w.write(pulse1);
    w.write(pulse2);
    w.write(triangle);
    w.write(noise);
    w.write(dmc);
    w.write(five_step_mode);
    w.write(irq_inhibit);
    w.write(frame_irq);
    w.write(frame_cycle_counter);
    w.write(cpu_cycle_count);
    w.write(last_sample);
}

void APU::DeserializeState(StateReader& r) {
    r.read(pulse1);
    r.read(pulse2);
    r.read(triangle);
    r.read(noise);
    r.read(dmc);
    r.read(five_step_mode);
    r.read(irq_inhibit);
    r.read(frame_irq);
    r.read(frame_cycle_counter);
    r.read(cpu_cycle_count);
    r.read(last_sample);
}

const std::array<uint8_t, 32> APU::length_table = {
    10,254,20,2,40,4,80,6,160,8,60,10,14,12,26,14,
    12,16,24,18,48,20,96,22,192,24,72,26,16,28,32,30
};

const uint8_t APU::Triangle::sequence[32] = {
    15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
};

// ================================= Pulse ======================================

void APU::Pulse::WriteReg0(uint8_t data) {
    duty_cycle = (data >> 6) & 0x03;
    envelope_loop = (data & 0x20) != 0; // doubles as length-counter halt
    constant_volume = (data & 0x10) != 0;
    envelope_volume = data & 0x0F;
}

void APU::Pulse::WriteReg1(uint8_t data) {
    sweep_enabled = (data & 0x80) != 0;
    sweep_period = (data >> 4) & 0x07;
    sweep_negate = (data & 0x08) != 0;
    sweep_shift = data & 0x07;
    sweep_reload = true;
}

void APU::Pulse::WriteReg2(uint8_t data) {
    timer_period = (uint16_t)((timer_period & 0xFF00) | data);
}

void APU::Pulse::WriteReg3(uint8_t data) {
    timer_period = (uint16_t)((timer_period & 0x00FF) | ((data & 0x07) << 8));
    if (enabled) length_counter = APU::length_table[(data >> 3) & 0x1F];
    duty_pos = 0;
    envelope_start = true;
}

void APU::Pulse::ClockTimer() {
    if (timer_value == 0) {
        timer_value = timer_period;
        duty_pos = (duty_pos + 1) & 0x07;
    } else {
        timer_value--;
    }
}

void APU::Pulse::ClockEnvelope() {
    if (envelope_start) {
        envelope_start = false;
        envelope_decay = 15;
        envelope_divider = envelope_volume;
    } else if (envelope_divider == 0) {
        envelope_divider = envelope_volume;
        if (envelope_decay > 0) envelope_decay--;
        else if (envelope_loop) envelope_decay = 15;
    } else {
        envelope_divider--;
    }
}

uint16_t APU::Pulse::TargetPeriod() const {
    uint16_t change = timer_period >> sweep_shift;
    if (sweep_negate) {
        int32_t result = (int32_t)timer_period - (int32_t)change - (is_pulse1 ? 1 : 0);
        return (uint16_t)(result < 0 ? 0 : result);
    }
    return (uint16_t)(timer_period + change);
}

bool APU::Pulse::IsMuted() const {
    return timer_period < 8 || TargetPeriod() > 0x07FF;
}

void APU::Pulse::ClockSweep() {
    if (sweep_divider == 0 && sweep_enabled && sweep_shift > 0 && !IsMuted()) {
        timer_period = TargetPeriod();
    }
    if (sweep_divider == 0 || sweep_reload) {
        sweep_divider = sweep_period;
        sweep_reload = false;
    } else {
        sweep_divider--;
    }
}

void APU::Pulse::ClockLength() {
    if (!envelope_loop && length_counter > 0) length_counter--;
}

uint8_t APU::Pulse::Output() const {
    if (!enabled || length_counter == 0 || IsMuted()) return 0;
    static const bool duty_table[4][8] = {
        {0,1,0,0,0,0,0,0},
        {0,1,1,0,0,0,0,0},
        {0,1,1,1,1,0,0,0},
        {1,0,0,1,1,1,1,1},
    };
    if (!duty_table[duty_cycle][duty_pos]) return 0;
    return constant_volume ? envelope_volume : envelope_decay;
}

// ================================ Triangle ====================================

void APU::Triangle::WriteReg0(uint8_t data) {
    length_halt = (data & 0x80) != 0; // also the linear-counter "control" flag
    linear_reload_value = data & 0x7F;
}

void APU::Triangle::WriteReg2(uint8_t data) {
    timer_period = (uint16_t)((timer_period & 0xFF00) | data);
}

void APU::Triangle::WriteReg3(uint8_t data) {
    timer_period = (uint16_t)((timer_period & 0x00FF) | ((data & 0x07) << 8));
    if (enabled) length_counter = APU::length_table[(data >> 3) & 0x1F];
    linear_reload_flag = true;
}

void APU::Triangle::ClockTimer() {
    if (timer_value == 0) {
        timer_value = timer_period;
        if (length_counter > 0 && linear_counter > 0) {
            seq_pos = (seq_pos + 1) & 0x1F;
        }
    } else {
        timer_value--;
    }
}

void APU::Triangle::ClockLinear() {
    if (linear_reload_flag) {
        linear_counter = linear_reload_value;
    } else if (linear_counter > 0) {
        linear_counter--;
    }
    if (!length_halt) linear_reload_flag = false;
}

void APU::Triangle::ClockLength() {
    if (!length_halt && length_counter > 0) length_counter--;
}

uint8_t APU::Triangle::Output() const {
    // Real hardware freezes the sequencer (rather than silencing it) when
    // muted, which can leave it parked on a nonzero step; after the high-pass
    // filtering real audio hardware does, that's inaudible anyway, so we
    // just simplify straight to silence here.
    if (!enabled || length_counter == 0 || linear_counter == 0) return 0;
    return sequence[seq_pos];
}

// ================================== Noise =====================================

void APU::Noise::WriteReg0(uint8_t data) {
    envelope_loop = (data & 0x20) != 0; // doubles as length-counter halt
    constant_volume = (data & 0x10) != 0;
    envelope_volume = data & 0x0F;
}

void APU::Noise::WriteReg2(uint8_t data) {
    static const uint16_t period_table[16] = {
        4,8,16,32,64,96,128,160,202,254,380,508,762,1016,2034,4068
    };
    mode_flag = (data & 0x80) != 0;
    timer_period = period_table[data & 0x0F];
}

void APU::Noise::WriteReg3(uint8_t data) {
    if (enabled) length_counter = APU::length_table[(data >> 3) & 0x1F];
    envelope_start = true;
}

void APU::Noise::ClockTimer() {
    if (timer_value == 0) {
        timer_value = timer_period;
        uint16_t feedback_bit = mode_flag ? ((shift_register >> 6) & 1) : ((shift_register >> 1) & 1);
        uint16_t feedback = (uint16_t)((shift_register & 1) ^ feedback_bit);
        shift_register >>= 1;
        shift_register |= (uint16_t)(feedback << 14);
    } else {
        timer_value--;
    }
}

void APU::Noise::ClockEnvelope() {
    if (envelope_start) {
        envelope_start = false;
        envelope_decay = 15;
        envelope_divider = envelope_volume;
    } else if (envelope_divider == 0) {
        envelope_divider = envelope_volume;
        if (envelope_decay > 0) envelope_decay--;
        else if (envelope_loop) envelope_decay = 15;
    } else {
        envelope_divider--;
    }
}

void APU::Noise::ClockLength() {
    if (!envelope_loop && length_counter > 0) length_counter--;
}

uint8_t APU::Noise::Output() const {
    if (!enabled || length_counter == 0) return 0;
    if (shift_register & 1) return 0;
    return constant_volume ? envelope_volume : envelope_decay;
}

// =================================== DMC ======================================

static const uint16_t dmc_rate_table[16] = {
    428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
};

void APU::DMC::WriteReg0(uint8_t data) {
    irq_enable = (data & 0x80) != 0;
    loop = (data & 0x40) != 0;
    timer_period = dmc_rate_table[data & 0x0F];
    if (!irq_enable) irq_flag = false; // clearing the enable bit also clears any pending flag
}

void APU::DMC::WriteReg1(uint8_t data) {
    output_level = data & 0x7F; // top bit ignored - this is a 7-bit DAC
}

void APU::DMC::WriteReg2(uint8_t data) {
    sample_address = (uint16_t)(0xC000 + ((uint16_t)data << 6));
}

void APU::DMC::WriteReg3(uint8_t data) {
    sample_length = (uint16_t)(((uint16_t)data << 4) + 1);
}

void APU::DMC::SetEnabled(bool en) {
    if (!en) {
        bytes_remaining = 0; // stops the memory reader; already-loaded bits still play out
    } else if (bytes_remaining == 0) {
        current_address = sample_address;
        bytes_remaining = sample_length;
    }
}

void APU::DMC::FillSampleBufferIfNeeded(Bus* b) {
    if (sample_buffer_filled || bytes_remaining == 0 || !b) return;

    sample_buffer = b->cpuRead(current_address);
    sample_buffer_filled = true;
    current_address = (current_address == 0xFFFF) ? (uint16_t)0x8000 : (uint16_t)(current_address + 1);
    bytes_remaining--;

    if (bytes_remaining == 0) {
        if (loop) {
            current_address = sample_address;
            bytes_remaining = sample_length;
        } else if (irq_enable) {
            irq_flag = true;
        }
    }
}

void APU::DMC::ClockTimer(Bus* b) {
    // The memory reader is logically independent of the output-unit timer
    // below - it just keeps the buffer topped up whenever it can.
    FillSampleBufferIfNeeded(b);

    if (timer_value == 0) {
        timer_value = timer_period;

        if (!silence) {
            if (shift_register & 0x01) {
                if (output_level <= 125) output_level += 2;
            } else {
                if (output_level >= 2) output_level -= 2;
            }
        }
        shift_register >>= 1;
        bits_remaining--;

        if (bits_remaining == 0) {
            bits_remaining = 8;
            if (!sample_buffer_filled) {
                silence = true;
            } else {
                silence = false;
                shift_register = sample_buffer;
                sample_buffer_filled = false;
            }
        }
    } else {
        timer_value--;
    }
}

// =================================== APU ======================================

APU::APU() {
    pulse1.is_pulse1 = true;
    pulse2.is_pulse1 = false;
    noise.shift_register = 1;
}

void APU::reset() {
    pulse1 = Pulse{};
    pulse1.is_pulse1 = true;
    pulse2 = Pulse{};
    pulse2.is_pulse1 = false;
    triangle = Triangle{};
    noise = Noise{};
    noise.shift_register = 1;
    dmc = DMC{};
    five_step_mode = false;
    irq_inhibit = false;
    frame_irq = false;
    frame_cycle_counter = 0;
    cpu_cycle_count = 0;
    last_sample = 0.0;
}

void APU::cpuWrite(uint16_t addr, uint8_t data) {
    switch (addr) {
        case 0x4000: pulse1.WriteReg0(data); break;
        case 0x4001: pulse1.WriteReg1(data); break;
        case 0x4002: pulse1.WriteReg2(data); break;
        case 0x4003: pulse1.WriteReg3(data); break;
        case 0x4004: pulse2.WriteReg0(data); break;
        case 0x4005: pulse2.WriteReg1(data); break;
        case 0x4006: pulse2.WriteReg2(data); break;
        case 0x4007: pulse2.WriteReg3(data); break;
        case 0x4008: triangle.WriteReg0(data); break;
        case 0x400A: triangle.WriteReg2(data); break;
        case 0x400B: triangle.WriteReg3(data); break;
        case 0x400C: noise.WriteReg0(data); break;
        case 0x400E: noise.WriteReg2(data); break;
        case 0x400F: noise.WriteReg3(data); break;
        case 0x4010: dmc.WriteReg0(data); break;
        case 0x4011: dmc.WriteReg1(data); break;
        case 0x4012: dmc.WriteReg2(data); break;
        case 0x4013: dmc.WriteReg3(data); break;
        case 0x4015:
            pulse1.enabled = (data & 0x01) != 0;
            pulse2.enabled = (data & 0x02) != 0;
            triangle.enabled = (data & 0x04) != 0;
            noise.enabled = (data & 0x08) != 0;
            if (!pulse1.enabled) pulse1.length_counter = 0;
            if (!pulse2.enabled) pulse2.length_counter = 0;
            if (!triangle.enabled) triangle.length_counter = 0;
            if (!noise.enabled) noise.length_counter = 0;
            dmc.SetEnabled((data & 0x10) != 0);
            break;
        case 0x4017:
            five_step_mode = (data & 0x80) != 0;
            irq_inhibit = (data & 0x40) != 0;
            if (irq_inhibit) frame_irq = false;
            frame_cycle_counter = 0;
            if (five_step_mode) {
                // Writing this immediately clocks quarter+half frame units once
                pulse1.ClockEnvelope(); pulse2.ClockEnvelope(); noise.ClockEnvelope(); triangle.ClockLinear();
                pulse1.ClockLength();   pulse2.ClockLength();   noise.ClockLength();   triangle.ClockLength();
                pulse1.ClockSweep();    pulse2.ClockSweep();
            }
            break;
        default: break;
    }
}

uint8_t APU::cpuRead(uint16_t addr) {
    if (addr == 0x4015) {
        uint8_t result = 0;
        if (pulse1.length_counter > 0)   result |= 0x01;
        if (pulse2.length_counter > 0)   result |= 0x02;
        if (triangle.length_counter > 0) result |= 0x04;
        if (noise.length_counter > 0)    result |= 0x08;
        if (dmc.bytes_remaining > 0)     result |= 0x10;
        if (dmc.irq_flag)                result |= 0x80;
        if (frame_irq)                   result |= 0x40;
        frame_irq = false;    // reading $4015 acknowledges the frame IRQ
        dmc.irq_flag = false; // ...and the DMC IRQ
        return result;
    }
    return 0x00;
}

void APU::clock() {
    // Triangle's timer runs at the full CPU rate; pulse/noise run at half
    // that (real hardware ties them to a separately-divided clock). DMC also
    // runs at full CPU rate - its rate table is specified directly in CPU
    // cycles, unlike pulse/noise's tables.
    triangle.ClockTimer();
    dmc.ClockTimer(bus);
    if ((cpu_cycle_count & 1) == 1) {
        pulse1.ClockTimer();
        pulse2.ClockTimer();
        noise.ClockTimer();
    }

    // Frame sequencer - standard NTSC CPU-cycle thresholds for each step.
    frame_cycle_counter++;
    if (!five_step_mode) {
        switch (frame_cycle_counter) {
            case 7457:
                pulse1.ClockEnvelope(); pulse2.ClockEnvelope(); noise.ClockEnvelope(); triangle.ClockLinear();
                break;
            case 14913:
                pulse1.ClockEnvelope(); pulse2.ClockEnvelope(); noise.ClockEnvelope(); triangle.ClockLinear();
                pulse1.ClockLength();   pulse2.ClockLength();   noise.ClockLength();   triangle.ClockLength();
                pulse1.ClockSweep();    pulse2.ClockSweep();
                break;
            case 22371:
                pulse1.ClockEnvelope(); pulse2.ClockEnvelope(); noise.ClockEnvelope(); triangle.ClockLinear();
                break;
            case 29829:
                pulse1.ClockEnvelope(); pulse2.ClockEnvelope(); noise.ClockEnvelope(); triangle.ClockLinear();
                pulse1.ClockLength();   pulse2.ClockLength();   noise.ClockLength();   triangle.ClockLength();
                pulse1.ClockSweep();    pulse2.ClockSweep();
                if (!irq_inhibit) frame_irq = true;
                frame_cycle_counter = 0;
                break;
            default: break;
        }
    } else {
        switch (frame_cycle_counter) {
            case 7457:
                pulse1.ClockEnvelope(); pulse2.ClockEnvelope(); noise.ClockEnvelope(); triangle.ClockLinear();
                break;
            case 14913:
                pulse1.ClockEnvelope(); pulse2.ClockEnvelope(); noise.ClockEnvelope(); triangle.ClockLinear();
                pulse1.ClockLength();   pulse2.ClockLength();   noise.ClockLength();   triangle.ClockLength();
                pulse1.ClockSweep();    pulse2.ClockSweep();
                break;
            case 22371:
                pulse1.ClockEnvelope(); pulse2.ClockEnvelope(); noise.ClockEnvelope(); triangle.ClockLinear();
                break;
            case 37281:
                pulse1.ClockEnvelope(); pulse2.ClockEnvelope(); noise.ClockEnvelope(); triangle.ClockLinear();
                pulse1.ClockLength();   pulse2.ClockLength();   noise.ClockLength();   triangle.ClockLength();
                pulse1.ClockSweep();    pulse2.ClockSweep();
                frame_cycle_counter = 0;
                break;
            default: break;
        }
    }

    cpu_cycle_count++;

    uint8_t p1 = pulse1.Output();
    uint8_t p2 = pulse2.Output();
    uint8_t tr = triangle.Output();
    uint8_t ns = noise.Output();
    uint8_t dm = dmc.Output();

    // Standard NES non-linear mixing approximation, now with all five channels.
    double pulse_out = (p1 + p2) == 0 ? 0.0 : 95.88 / ((8128.0 / (p1 + p2)) + 100.0);
    double tnd_out = (tr == 0 && ns == 0 && dm == 0) ? 0.0
        : 159.79 / (1.0 / ((tr / 8227.0) + (ns / 12241.0) + (dm / 22638.0)) + 100.0);

    last_sample = pulse_out + tnd_out;
}
