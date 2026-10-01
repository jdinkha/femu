#include "mapper069.h"
#include "serialize.h"
#include <cmath>

namespace {
// 5B DAC: 5-bit level, 1.5dB per step, levels 0 and 1 silent. Scaled so a
// channel at full volume is ~6dB above an APU pulse at full volume (0.15
// on the APU's mixer scale) - the 5B is documented as "very loud" next to
// the 2A03, but no exact figure is published.
float Level(int level) {
    if (level < 2) return 0.0f;
    return 0.30f * std::pow(10.0f, (level - 31) * 1.5f / 20.0f);
}
} // namespace

Mapper_069::Mapper_069(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
    SetPrg8k(0, 0);
    SetPrg8k(1, 0);
    SetPrg8k(2, 0);
    SetPrg8k(3, -1);
    SetPrg6000(0);
    mirror_mode = Mirror::VERTICAL;
}

void Mapper_069::WriteRegister(uint16_t addr, uint8_t data) {
    switch (addr & 0xE000) {
        case 0x8000: command = data & 0x0F; break;
        case 0xA000: Command(data); break;
        case 0xC000: audio_select = data; break;
        case 0xE000: if (!(audio_select & 0xF0)) AudioWrite(audio_select & 0x0F, data); break;
    }
}

void Mapper_069::Command(uint8_t data) {
    switch (command) {
        case 0x0: case 0x1: case 0x2: case 0x3:
        case 0x4: case 0x5: case 0x6: case 0x7:
            SetChr1k(command, data);
            break;
        case 0x8:
            prg6000 = data;
            if (data & 0x40) ClearPrg6000();
            else             SetPrg6000(data & 0x3F);
            break;
        case 0x9: case 0xA: case 0xB:
            SetPrg8k(command - 0x9, data & 0x3F);
            break;
        case 0xC:
            switch (data & 0x03) {
                case 0: mirror_mode = Mirror::VERTICAL; break;
                case 1: mirror_mode = Mirror::HORIZONTAL; break;
                case 2: mirror_mode = Mirror::ONESCREEN_LO; break;
                case 3: mirror_mode = Mirror::ONESCREEN_HI; break;
            }
            break;
        case 0xD:
            irq_control = data;
            irq_pending = false;
            break;
        case 0xE: irq_counter = (uint16_t)((irq_counter & 0xFF00) | data); break;
        case 0xF: irq_counter = (uint16_t)((irq_counter & 0x00FF) | (data << 8)); break;
    }
}

bool Mapper_069::prgRamMap(uint16_t addr, uint32_t& offset) {
    if (addr < 0x6000 || addr > 0x7FFF || !(prg6000 & 0x40)) return false;
    if (!(prg6000 & 0x80)) return false; // RAM selected but disabled: open bus
    offset = (uint32_t)(prg6000 & 0x3F) * 0x2000 + (addr & 0x1FFF);
    return true;
}

void Mapper_069::cpuClock() {
    if (irq_control & 0x80) {
        if (irq_counter-- == 0 && (irq_control & 0x01)) irq_pending = true;
    }
    if (++prescaler == 16) {
        prescaler = 0;
        AudioTick();
    }
}

void Mapper_069::AudioWrite(uint8_t reg, uint8_t data) {
    audio_reg[reg] = data;
    if (reg == 0x0D) { // envelope shape, restarting it
        env_attack = (data & 0x04) ? 0x1F : 0x00;
        if (!(data & 0x08)) { env_hold = 1; env_alternate = env_attack; }
        else                { env_hold = data & 0x01; env_alternate = (data & 0x02) ? 1 : 0; }
        env_step = 0x1F;
        env_holding = false;
        env_count = 0;
    }
}

// Runs every 16 CPU cycles: tones flip when their counter reaches the
// period; noise and the envelope step on their own periods.
void Mapper_069::AudioTick() {
    for (int ch = 0; ch < 3; ch++) {
        uint16_t period = (uint16_t)(audio_reg[ch * 2] | ((audio_reg[ch * 2 + 1] & 0x0F) << 8));
        if (++tone_count[ch] >= (period ? period : 1)) {
            tone_count[ch] = 0;
            tone_out[ch] = !tone_out[ch];
        }
    }

    noise_phase = !noise_phase;
    if (noise_phase) {
        uint8_t period = audio_reg[6] & 0x1F;
        if (++noise_count >= (period ? period : 1)) {
            noise_count = 0;
            uint32_t bit = (lfsr ^ (lfsr >> 3)) & 1; // taps at bits 0 and 3 of the 17-bit register
            lfsr = (lfsr >> 1) | (bit << 16);
        }
    }

    uint16_t env_period = (uint16_t)(audio_reg[0x0B] | (audio_reg[0x0C] << 8));
    if (++env_count >= (env_period ? env_period : 1)) {
        env_count = 0;
        if (!env_holding && --env_step < 0) {
            if (env_hold) {
                if (env_alternate) env_attack ^= 0x1F;
                env_holding = true;
                env_step = 0;
            } else {
                if (env_alternate) env_attack ^= 0x1F;
                env_step = 0x1F;
            }
        }
    }

    const uint8_t mix = audio_reg[7];
    const int env_level = env_step ^ env_attack;
    float out = 0.0f;
    for (int ch = 0; ch < 3; ch++) {
        bool tone_on = tone_out[ch] || (mix & (1 << ch));
        bool noise_on = (lfsr & 1) || (mix & (8 << ch));
        if (!tone_on || !noise_on) continue;
        uint8_t v = audio_reg[8 + ch];
        int level = (v & 0x10) ? env_level : ((v & 0x0F) ? (v & 0x0F) * 2 + 1 : 0);
        out += Level(level);
    }
    audio_out = out;
}

void Mapper_069::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(command);
    w.write(prg6000);
    w.write(irq_control);
    w.write(irq_counter);
    w.write(irq_pending);
    w.write(audio_select);
    w.writeBytes(audio_reg.data(), sizeof(audio_reg));
    w.write(prescaler);
    w.write(noise_phase);
    w.writeBytes(tone_count, sizeof(tone_count));
    w.writeBytes(tone_out, sizeof(tone_out));
    w.write(noise_count);
    w.write(lfsr);
    w.write(env_count);
    w.write(env_step);
    w.write(env_attack);
    w.write(env_hold);
    w.write(env_alternate);
    w.write(env_holding);
    w.write(audio_out);
}

void Mapper_069::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(command);
    r.read(prg6000);
    r.read(irq_control);
    r.read(irq_counter);
    r.read(irq_pending);
    r.read(audio_select);
    r.readBytes(audio_reg.data(), sizeof(audio_reg));
    r.read(prescaler);
    r.read(noise_phase);
    r.readBytes(tone_count, sizeof(tone_count));
    r.readBytes(tone_out, sizeof(tone_out));
    r.read(noise_count);
    r.read(lfsr);
    r.read(env_count);
    r.read(env_step);
    r.read(env_attack);
    r.read(env_hold);
    r.read(env_alternate);
    r.read(env_holding);
    r.read(audio_out);
}
