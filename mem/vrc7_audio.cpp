#include "vrc7_audio.h"
#include "serialize.h"
#include <cmath>
#include <algorithm>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 49716.0; // 3.58MHz / 72; exactly CPU clock / 36

// Konami's fixed instruments 1-15, dumped from the chip (patch 0 is the
// custom one in registers $00-$07).
const uint8_t kPatches[16][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x03, 0x21, 0x05, 0x06, 0xE8, 0x81, 0x42, 0x27}, // Buzzy Bell
    {0x13, 0x41, 0x14, 0x0D, 0xD8, 0xF6, 0x23, 0x12}, // Guitar
    {0x11, 0x11, 0x08, 0x08, 0xFA, 0xB2, 0x20, 0x12}, // Wurly
    {0x31, 0x61, 0x0C, 0x07, 0xA8, 0x64, 0x61, 0x27}, // Flute
    {0x32, 0x21, 0x1E, 0x06, 0xE1, 0x76, 0x01, 0x28}, // Clarinet
    {0x02, 0x01, 0x06, 0x00, 0xA3, 0xE2, 0xF4, 0xF4}, // Synth
    {0x21, 0x61, 0x1D, 0x07, 0x82, 0x81, 0x11, 0x07}, // Trumpet
    {0x23, 0x21, 0x22, 0x17, 0xA2, 0x72, 0x01, 0x17}, // Organ
    {0x35, 0x11, 0x25, 0x00, 0x40, 0x73, 0x72, 0x01}, // Bells
    {0xB5, 0x01, 0x0F, 0x0F, 0xA8, 0xA5, 0x51, 0x02}, // Vibes
    {0x17, 0xC1, 0x24, 0x07, 0xF8, 0xF8, 0x22, 0x12}, // Vibraphone
    {0x71, 0x23, 0x11, 0x06, 0x65, 0x74, 0x18, 0x16}, // Tutti
    {0x01, 0x02, 0xD3, 0x05, 0xC9, 0x95, 0x03, 0x02}, // Fretless
    {0x61, 0x63, 0x0C, 0x00, 0x94, 0xC0, 0x33, 0xF6}, // Synth Bass
    {0x21, 0x72, 0x0D, 0x00, 0xC1, 0xD5, 0x56, 0x06}, // Sweep
};

// Frequency multiplier, doubled so 1/2 is representable.
const uint8_t kMult2[16] = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30};

// Feedback modulation index per FFF.
const double kFeedback[8] = {0, kPi / 16, kPi / 8, kPi / 4, kPi / 2, kPi, 2 * kPi, 4 * kPi};

// Modulator-to-carrier depth at full modulator output, as in the OPL line.
constexpr double kModIndex = 4 * kPi;

// Key level scaling base attenuation (dB) by the top four F-number bits,
// at octave 7; it drops 3dB per octave below that.
const double kKslTable[16] = {0.000, 9.000, 12.000, 13.875, 15.000, 16.125, 16.875, 17.625,
                              18.000, 18.750, 19.125, 19.500, 19.875, 20.250, 20.625, 21.000};
const double kKslScale[4] = {0.0, 0.5, 1.0, 2.0}; // 0, 1.5, 3, 6 dB per octave

constexpr double kChannelLevel = 0.12; // one channel at full volume, on the APU mixer's scale

double KeyScaleLevel(int ksl, int fnum, int block) {
    double base = kKslTable[fnum >> 5] - 3.0 * (7 - block);
    return base > 0 ? base * kKslScale[ksl] : 0.0;
}

} // namespace

void Vrc7Audio::Reset() {
    regs.fill(0);
    for (Channel& c : ch) c = Channel{};
    divider = 0;
    lfo_time = 0;
    output = 0.0f;
}

const uint8_t* Vrc7Audio::Patch(int channel) const {
    int instrument = regs[0x30 + channel] >> 4;
    return instrument ? kPatches[instrument] : &regs[0x00];
}

void Vrc7Audio::Write(uint8_t reg, uint8_t data) {
    if (reg > 0x35) return;
    if (reg >= 0x20 && reg <= 0x25) {
        Channel& c = ch[reg - 0x20];
        bool key = data & 0x10;
        if (key && !c.key) KeyOn(c);
        if (!key && c.key) KeyOff(c);
        c.key = key;
    }
    regs[reg] = data;
}

void Vrc7Audio::KeyOn(Channel& c) {
    for (Operator* op : {&c.mod, &c.car}) {
        op->state = ATTACK;
        op->phase = 0;
    }
}

void Vrc7Audio::KeyOff(Channel& c) {
    c.car.state = c.car.state == OFF ? OFF : RELEASE;
    // A modulator with its patch sustain bit keeps sustaining after key-off.
    const uint8_t* patch = Patch((int)(&c - ch.data()));
    if (!(patch[0] & 0x20) && c.mod.state != OFF) c.mod.state = RELEASE;
}

void Vrc7Audio::Clock() {
    if (++divider < 36) return;
    divider = 0;
    Sample();
}

// One envelope step, in the OPL style: rate 4R + key scale, where every
// four rate steps doubles the speed. Attack approaches 0 exponentially;
// the other phases add attenuation linearly.
void Vrc7Audio::Envelope(Operator& op, const uint8_t* patch, bool carrier, int channel, int rks) {
    const int n = carrier ? 1 : 0;
    int rate = 0;
    switch (op.state) {
        case ATTACK:  rate = patch[4 + n] >> 4; break;
        case DECAY:   rate = patch[4 + n] & 0x0F; break;
        case SUSTAIN: rate = (patch[n] & 0x20) ? 0 : (patch[6 + n] & 0x0F); break;
        case RELEASE: rate = (regs[0x20 + channel] & 0x20) ? 5 : (patch[6 + n] & 0x0F); break;
        case OFF:     op.env = 127.0; return;
    }
    if (rate == 0) return;
    int r = std::min(63, rate * 4 + rks);
    double step = (4 + (r & 3)) / 4.0 * std::ldexp(1.0, (r >> 2) - 13);

    switch (op.state) {
        case ATTACK:
            if (r >= 60) op.env = 0.0;
            else         op.env -= step * (op.env / 8.0 + 1.0);
            if (op.env <= 0.0) { op.env = 0.0; op.state = DECAY; }
            break;
        case DECAY: {
            double sustain_level = (patch[6 + n] >> 4) * 8.0; // 3dB steps
            op.env += step;
            if (op.env >= sustain_level) { op.env = sustain_level; op.state = SUSTAIN; }
            break;
        }
        case SUSTAIN:
        case RELEASE:
            op.env += step;
            if (op.env >= 127.0) { op.env = 127.0; op.state = OFF; }
            break;
        case OFF: break;
    }
}

void Vrc7Audio::Sample() {
    lfo_time++;
    const double t = lfo_time / kSampleRate;
    const double tremolo_db = 4.8 * (1.0 + std::sin(2 * kPi * 3.7 * t)) / 2.0;
    const double vibrato = 1.0 + 0.0081 * std::sin(2 * kPi * 6.4 * t); // about +-14 cents

    double sum = 0.0;
    for (int c = 0; c < 6; c++) {
        Channel& chn = ch[c];
        const uint8_t* patch = Patch(c);
        const int fnum = regs[0x10 + c] | ((regs[0x20 + c] & 0x01) << 8);
        const int block = (regs[0x20 + c] >> 1) & 0x07;
        const int ks = block * 2 + (fnum >> 8);

        Operator* ops[2] = {&chn.mod, &chn.car};
        for (int n = 0; n < 2; n++) {
            Operator& op = *ops[n];
            const uint8_t p = patch[n];
            Envelope(op, patch, n == 1, c, (p & 0x10) ? ks : ks >> 2);
            double inc = (double)((fnum << block) * kMult2[p & 0x0F]) / 2.0;
            if (p & 0x40) inc *= vibrato;
            op.phase = (uint32_t)(op.phase + (uint32_t)inc) & 0x7FFFF;
        }

        // Modulator, with self-feedback.
        Operator& mod = chn.mod;
        double angle = 2 * kPi * mod.phase / 524288.0;
        if (patch[3] & 0x07) angle += kFeedback[patch[3] & 0x07] * (mod.out + mod.prev) / 2.0;
        double wave = std::sin(angle);
        if ((patch[3] & 0x08) && wave < 0) wave = 0; // half-wave rectified
        double att = mod.env * 0.375 + (patch[2] & 0x3F) * 0.75 + KeyScaleLevel(patch[2] >> 6, fnum, block);
        if (patch[0] & 0x80) att += tremolo_db;
        mod.prev = mod.out;
        mod.out = mod.state == OFF ? 0.0 : wave * std::pow(10.0, -att / 20.0);

        // Carrier, phase-modulated by the modulator.
        Operator& car = chn.car;
        angle = 2 * kPi * car.phase / 524288.0 + mod.out * kModIndex;
        wave = std::sin(angle);
        if ((patch[3] & 0x10) && wave < 0) wave = 0;
        att = car.env * 0.375 + (regs[0x30 + c] & 0x0F) * 3.0 + KeyScaleLevel(patch[3] >> 6, fnum, block);
        if (patch[1] & 0x80) att += tremolo_db;
        car.out = car.state == OFF ? 0.0 : wave * std::pow(10.0, -att / 20.0);
        sum += car.out;
    }
    output = (float)(sum * kChannelLevel);
}

void Vrc7Audio::Serialize(StateWriter& w) const {
    w.writeBytes(regs.data(), sizeof(regs));
    w.writeBytes(ch.data(), sizeof(ch));
    w.write(divider);
    w.write(lfo_time);
    w.write(output);
}

void Vrc7Audio::Deserialize(StateReader& r) {
    r.readBytes(regs.data(), sizeof(regs));
    r.readBytes(ch.data(), sizeof(ch));
    r.read(divider);
    r.read(lfo_time);
    r.read(output);
}
