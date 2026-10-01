#pragma once
#include <cstdint>
#include "serialize.h"

// The IRQ counter shared by Konami's VRC4, VRC6 and VRC7. An 8-bit counter
// counts up from a reload latch and fires when it overflows past $FF. In
// cycle mode it's clocked every CPU cycle; in scanline mode a prescaler
// divides CPU cycles by 113 2/3 (114, 114, 113...) to approximate NTSC
// scanlines.
struct VrcIrq {
    uint8_t latch = 0;
    uint8_t control = 0;     // bit 0 A (enable after ack), bit 1 E (enable), bit 2 M (cycle mode)
    uint8_t counter = 0;
    int16_t prescaler = 341;
    bool pending = false;

    void WriteLatchLow(uint8_t v)  { latch = (uint8_t)((latch & 0xF0) | (v & 0x0F)); }
    void WriteLatchHigh(uint8_t v) { latch = (uint8_t)((latch & 0x0F) | (v << 4)); }

    void WriteControl(uint8_t v) {
        control = v & 0x07;
        pending = false;
        prescaler = 341;
        if (control & 0x02) counter = latch;
    }

    void Acknowledge() {
        pending = false;
        control = (uint8_t)((control & ~0x02) | ((control & 0x01) << 1)); // E = A
    }

    // Once per CPU cycle.
    void Clock() {
        if (!(control & 0x02)) return;
        if (control & 0x04) {
            Tick();
        } else {
            prescaler -= 3;
            if (prescaler <= 0) {
                prescaler += 341;
                Tick();
            }
        }
    }

    void Serialize(StateWriter& w) const {
        w.write(latch); w.write(control); w.write(counter); w.write(prescaler); w.write(pending);
    }
    void Deserialize(StateReader& r) {
        r.read(latch); r.read(control); r.read(counter); r.read(prescaler); r.read(pending);
    }

private:
    void Tick() {
        if (counter == 0xFF) {
            counter = latch;
            pending = true;
        } else {
            counter++;
        }
    }
};
