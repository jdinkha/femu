#include "mapper024.h"

Mapper_024::Mapper_024(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId)
    : BankedMapper(prgBanks, chrBanks), swap_lines(mapperId == 26) {
    SetPrg16k(0, 0);
    SetPrg8k(2, 0);
    SetPrg8k(3, -1);
    style = 0x20;
    UpdateChr();
}

void Mapper_024::UpdateChr() {
    // Which register feeds each 1KB pattern window, per the low two style
    // bits; 2KB windows use the register for both halves.
    static const uint8_t layout[3][8] = {
        {0, 1, 2, 3, 4, 5, 6, 7}, // mode 0: eight 1KB banks
        {0, 0, 1, 1, 2, 2, 3, 3}, // mode 1: four 2KB banks
        {0, 1, 2, 3, 4, 4, 5, 5}, // modes 2/3: 1KB, 1KB, 1KB, 1KB, 2KB, 2KB
    };
    const uint8_t mode = style & 0x03;
    const uint8_t* map = layout[mode >= 2 ? 2 : mode];
    for (int w = 0; w < 8; w++) {
        bool two_k = (w > 0 && map[w] == map[w - 1]) || (w < 7 && map[w] == map[w + 1]);
        if (two_k && (style & 0x20)) SetChr1k(w, (chr[map[w]] & ~1) | (w & 1)); // A10 passes through
        else                          SetChr1k(w, chr[map[w]]);
    }
    switch ((style >> 2) & 0x03) {
        case 0: mirror_mode = Mirror::VERTICAL; break;
        case 1: mirror_mode = Mirror::HORIZONTAL; break;
        case 2: mirror_mode = Mirror::ONESCREEN_LO; break;
        case 3: mirror_mode = Mirror::ONESCREEN_HI; break;
    }
}

void Mapper_024::WriteRegister(uint16_t addr, uint8_t data) {
    uint16_t reg = addr & 0xF003;
    if (swap_lines) reg = (uint16_t)((reg & 0xF000) | ((reg & 1) << 1) | ((reg >> 1) & 1));

    switch (reg & 0xF000) {
        case 0x8000: SetPrg16k(0, data & 0x0F); return;
        case 0xC000: SetPrg8k(2, data & 0x1F); return;
        case 0xD000: chr[reg & 3] = data; UpdateChr(); return;
        case 0xE000: chr[4 + (reg & 3)] = data; UpdateChr(); return;
        case 0xF000:
            switch (reg & 3) {
                case 0: irq.latch = data; break;
                case 1: irq.WriteControl(data); break;
                case 2: irq.Acknowledge(); break;
            }
            return;
    }

    // $9000-$B003: audio, and the banking style at $B003.
    if (reg == 0xB003) { style = data; UpdateChr(); return; }
    if (reg == 0x9003) { freq_control = data & 0x07; return; }
    int ch = ((reg & 0xF000) - 0x9000) >> 12; // 0, 1 pulses; 2 saw
    switch (reg & 3) {
        case 0:
            if (ch < 2) pulse[ch].control = data;
            else        saw.rate = data & 0x3F;
            break;
        case 1:
            if (ch < 2) pulse[ch].period = (uint16_t)((pulse[ch].period & 0x0F00) | data);
            else        saw.period = (uint16_t)((saw.period & 0x0F00) | data);
            break;
        case 2: {
            bool enable = data & 0x80;
            uint16_t high = (uint16_t)((data & 0x0F) << 8);
            if (ch < 2) {
                pulse[ch].period = (uint16_t)((pulse[ch].period & 0x00FF) | high);
                pulse[ch].enabled = enable;
                if (!enable) pulse[ch].step = 15; // duty restarts when re-enabled
            } else {
                saw.period = (uint16_t)((saw.period & 0x00FF) | high);
                saw.enabled = enable;
                if (!enable) saw.accumulator = 0;
            }
            break;
        }
    }
}

uint16_t Mapper_024::Period(uint16_t period) const {
    if (freq_control & 0x04) return period >> 8;
    if (freq_control & 0x02) return period >> 4;
    return period;
}

void Mapper_024::cpuClock() {
    irq.Clock();

    if (!(freq_control & 0x01)) { // not halted
        for (Pulse& p : pulse) {
            if (!p.enabled) continue;
            if (p.divider == 0) {
                p.divider = Period(p.period);
                p.step = (p.step - 1) & 0x0F;
            } else {
                p.divider--;
            }
        }
        if (saw.enabled) {
            if (saw.divider == 0) {
                saw.divider = Period(saw.period);
                if (++saw.step == 14) {
                    saw.step = 0;
                    saw.accumulator = 0;
                } else if (!(saw.step & 1)) {
                    saw.accumulator = (uint8_t)(saw.accumulator + saw.rate);
                }
            } else {
                saw.divider--;
            }
        }
    }

    int sum = 0;
    for (const Pulse& p : pulse) {
        if (!p.enabled) continue;
        bool high = (p.control & 0x80) || p.step <= ((p.control >> 4) & 0x07);
        if (high) sum += p.control & 0x0F;
    }
    if (saw.enabled) sum += saw.accumulator >> 3;
    // Linear DAC; a pulse at volume 15 matches an APU pulse at full volume
    // (0.149 on the APU mixer's scale).
    audio_out = sum * (0.1494f / 15.0f);
}

void Mapper_024::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(style);
    w.writeBytes(chr, sizeof(chr));
    irq.Serialize(w);
    for (const Pulse& p : pulse) w.write(p);
    w.write(saw);
    w.write(freq_control);
    w.write(audio_out);
}

void Mapper_024::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(style);
    r.readBytes(chr, sizeof(chr));
    irq.Deserialize(r);
    for (Pulse& p : pulse) r.read(p);
    r.read(saw);
    r.read(freq_control);
    r.read(audio_out);
}
