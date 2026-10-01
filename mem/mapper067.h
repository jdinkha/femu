#pragma once
#include "banked_mapper.h"

// Sunsoft-3 (mapper 67): Fantasy Zone II (J), Mito Koumon II. Mask $F800:
//   $8000 (any A11-clear write)  acknowledge the IRQ
//   $8800/$9800/$A800/$B800  2KB CHR banks
//   $C800  IRQ counter, written twice: high byte, then low byte
//   $D800  bit 4 counter enable; also resets the $C800 write toggle
//   $E800  mirroring: V, H, one-screen A, one-screen B
//   $F800  16KB PRG bank at $8000 (last bank fixed)
// The counter counts down every CPU cycle while enabled; wrapping from
// $0000 to $FFFF raises the IRQ and stops it.
class Mapper_067 : public BankedMapper {
public:
    Mapper_067(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {}

    bool irqState() const override { return irq_pending; }
    void cpuClock() override {
        if (irq_enabled && irq_counter-- == 0) {
            irq_pending = true;
            irq_enabled = false;
        }
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        if (!(addr & 0x0800)) { irq_pending = false; return; }
        switch (addr & 0xF800) {
            case 0x8800: case 0x9800: case 0xA800: case 0xB800:
                SetChr2k((addr >> 12) & 0x3, data & 0x3F); // $8800 -> 0 ... $B800 -> 3
                break;
            case 0xC800:
                if (!irq_toggle) irq_counter = (uint16_t)((irq_counter & 0x00FF) | (data << 8));
                else             irq_counter = (uint16_t)((irq_counter & 0xFF00) | data);
                irq_toggle = !irq_toggle;
                break;
            case 0xD800:
                irq_enabled = data & 0x10;
                irq_toggle = false;
                break;
            case 0xE800: {
                static const Mirror modes[4] = {Mirror::VERTICAL, Mirror::HORIZONTAL,
                                                Mirror::ONESCREEN_LO, Mirror::ONESCREEN_HI};
                mirror_mode = modes[data & 0x03];
                break;
            }
            case 0xF800: SetPrg16k(0, data & 0x0F); break;
        }
    }

private:
    uint16_t irq_counter = 0;
    bool irq_enabled = false, irq_pending = false, irq_toggle = false;
};
