#pragma once
#include "banked_mapper.h"

// Irem H3001 (mapper 65): Daiku no Gen San 2, Kaiketsu Yanchamaru 3,
// Spartan X 2. Mask $F007:
//   $8000  8KB PRG bank at $8000 (or $C000 when $9000 bit 7 is set; the
//          other slot holds the second-last bank, $E000 the last)
//   $9000  bit 7 PRG layout       $9001  bits 6-7 mirroring: 0 V, 2 H, else 1scA
//   $9003  bit 7 IRQ enable (acknowledges)
//   $9004  reload the counter from the reload value (acknowledges)
//   $9005 / $9006  reload value high / low byte
//   $A000  8KB PRG bank at $A000  $B000-$B007  1KB CHR banks
// The 16-bit counter counts down every CPU cycle while enabled and fires
// when it reaches 0, where it stays. The games rely on PRG banks 0 and 1
// at power-on.
class Mapper_065 : public BankedMapper {
public:
    Mapper_065(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        mirror_mode = Mirror::VERTICAL;
        Update();
    }

    bool irqState() const override { return irq_pending; }
    void cpuClock() override {
        if (irq_enabled && irq_counter && --irq_counter == 0) irq_pending = true;
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        switch (addr & 0xF007) {
            case 0x8000: prg[0] = data; Update(); break;
            case 0x9000: prg_mode = data & 0x80; Update(); break;
            case 0x9001:
                mirror_mode = (data >> 6) == 0 ? Mirror::VERTICAL
                            : (data >> 6) == 2 ? Mirror::HORIZONTAL : Mirror::ONESCREEN_LO;
                break;
            case 0x9003: irq_enabled = data & 0x80; irq_pending = false; break;
            case 0x9004: irq_counter = irq_reload; irq_pending = false; break;
            case 0x9005: irq_reload = (uint16_t)((irq_reload & 0x00FF) | (data << 8)); break;
            case 0x9006: irq_reload = (uint16_t)((irq_reload & 0xFF00) | data); break;
            case 0xA000: prg[1] = data; Update(); break;
            default:
                if ((addr & 0xF000) == 0xB000) SetChr1k(addr & 0x07, data);
                break;
        }
    }

private:
    uint8_t prg[2] = {0, 1};
    bool prg_mode = false;
    bool irq_enabled = false, irq_pending = false;
    uint16_t irq_counter = 0, irq_reload = 0;

    void Update() {
        SetPrg8k(prg_mode ? 2 : 0, prg[0]);
        SetPrg8k(prg_mode ? 0 : 2, -2);
        SetPrg8k(1, prg[1]);
        SetPrg8k(3, -1);
    }
};
