#include "mapper085.h"

Mapper_085::Mapper_085(uint8_t prgBanks, uint8_t chrBanks, uint8_t submapper)
    : BankedMapper(prgBanks, chrBanks),
      second_mask(submapper == 1 ? 0x08 : submapper == 2 ? 0x10 : 0x18) {
    SetPrg8k(0, 0);
    SetPrg8k(1, 0);
    SetPrg8k(2, 0);
    SetPrg8k(3, -1);
    mirror_mode = Mirror::VERTICAL;
}

void Mapper_085::WriteRegister(uint16_t addr, uint8_t data) {
    const bool second = (addr & second_mask) != 0;
    switch (addr & 0xF000) {
        case 0x8000: SetPrg8k(second ? 1 : 0, data & 0x3F); break;
        case 0x9000:
            if (!second)             SetPrg8k(2, data & 0x3F);
            else if (control & 0x40) break; // audio held in reset ignores writes
            else if (addr & 0x20)    audio.Write(audio_select, data);
            else                     audio_select = data;
            break;
        case 0xA000: case 0xB000: case 0xC000: case 0xD000:
            SetChr1k(((addr >> 12) - 0xA) * 2 + (second ? 1 : 0), data);
            break;
        case 0xE000:
            if (second) { irq.latch = data; break; }
            control = data;
            switch (data & 0x03) {
                case 0: mirror_mode = Mirror::VERTICAL; break;
                case 1: mirror_mode = Mirror::HORIZONTAL; break;
                case 2: mirror_mode = Mirror::ONESCREEN_LO; break;
                case 3: mirror_mode = Mirror::ONESCREEN_HI; break;
            }
            if (data & 0x40) audio.Reset(); // silences the chip and clears its registers
            break;
        case 0xF000:
            if (second) irq.Acknowledge();
            else        irq.WriteControl(data);
            break;
    }
}

void Mapper_085::cpuClock() {
    irq.Clock();
    if (!(control & 0x40)) audio.Clock();
}

void Mapper_085::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(control);
    w.write(audio_select);
    irq.Serialize(w);
    audio.Serialize(w);
}

void Mapper_085::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(control);
    r.read(audio_select);
    irq.Deserialize(r);
    audio.Deserialize(r);
}
