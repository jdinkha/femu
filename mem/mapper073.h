#pragma once
#include "banked_mapper.h"

// Konami VRC3 (mapper 73, Salamander): 16KB switchable PRG at $8000 via
// $F000 (last bank fixed), 8KB CHR-RAM, and a 16-bit CPU-cycle IRQ counter:
//   $8000/$9000/$A000/$B000  IRQ latch, four bits each (low to high)
//   $C000  control: bit 0 A (enable after ack), bit 1 E (enable), bit 2 M
//          (8-bit mode); acknowledges, and reloads the counter if E is set
//   $D000  acknowledge, copying A into E
// The counter counts up every cycle while enabled; on overflow past $FFFF
// it reloads from the latch and raises the IRQ. In 8-bit mode only the low
// byte counts, overflows and reloads.
class Mapper_073 : public BankedMapper {
public:
    Mapper_073(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {}

    bool irqState() const override { return irq_pending; }

    void cpuClock() override {
        if (!(irq_control & 0x02)) return;
        if (irq_control & 0x04) {
            uint8_t low = (uint8_t)(irq_counter & 0xFF);
            if (low == 0xFF) {
                irq_counter = (uint16_t)((irq_counter & 0xFF00) | (irq_latch & 0x00FF));
                irq_pending = true;
            } else {
                irq_counter++;
            }
        } else if (irq_counter == 0xFFFF) {
            irq_counter = irq_latch;
            irq_pending = true;
        } else {
            irq_counter++;
        }
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        switch (addr & 0xF000) {
            case 0x8000: case 0x9000: case 0xA000: case 0xB000: {
                int shift = ((addr >> 12) - 0x8) * 4;
                irq_latch = (uint16_t)((irq_latch & ~(0x0F << shift)) | ((data & 0x0F) << shift));
                break;
            }
            case 0xC000:
                irq_control = data & 0x07;
                irq_pending = false;
                if (irq_control & 0x02) irq_counter = irq_latch;
                break;
            case 0xD000:
                irq_pending = false;
                irq_control = (uint8_t)((irq_control & ~0x02) | ((irq_control & 0x01) << 1));
                break;
            case 0xF000:
                SetPrg16k(0, data & 0x07);
                break;
        }
    }

private:
    uint16_t irq_latch = 0, irq_counter = 0;
    uint8_t irq_control = 0;
    bool irq_pending = false;
};
