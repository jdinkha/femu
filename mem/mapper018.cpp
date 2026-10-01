#include "mapper018.h"
#include "serialize.h"

void Mapper_018::WriteRegister(uint16_t addr, uint8_t data) {
    const uint16_t reg = addr & 0xF003;
    const bool high = reg & 1;
    const uint8_t nibble = data & 0x0F;

    if (reg <= 0x9001) { // PRG: $8000/1, $8002/3, $9000/1
        int slot = ((reg >> 12) - 0x8) * 2 + ((reg >> 1) & 1);
        if (high) prg[slot] = (uint8_t)((prg[slot] & 0x0F) | ((nibble & 0x03) << 4));
        else      prg[slot] = (uint8_t)((prg[slot] & 0x30) | nibble);
        SetPrg8k(slot, prg[slot]);
    } else if (reg == 0x9002) {
        ram_control = data & 0x03;
    } else if (reg >= 0xA000 && reg <= 0xD003) {
        int slot = ((reg >> 12) - 0xA) * 2 + ((reg >> 1) & 1);
        if (high) chr[slot] = (uint8_t)((chr[slot] & 0x0F) | (nibble << 4));
        else      chr[slot] = (uint8_t)((chr[slot] & 0xF0) | nibble);
        SetChr1k(slot, chr[slot]);
    } else if ((reg & 0xF000) == 0xE000) {
        int shift = (reg & 3) * 4;
        irq_reload = (uint16_t)((irq_reload & ~(0x0F << shift)) | (nibble << shift));
    } else if (reg == 0xF000) {
        irq_counter = irq_reload;
        irq_pending = false;
    } else if (reg == 0xF001) {
        irq_control = data & 0x0F;
        irq_pending = false;
    } else if (reg == 0xF002) {
        static const Mirror modes[4] = {Mirror::HORIZONTAL, Mirror::VERTICAL,
                                        Mirror::ONESCREEN_LO, Mirror::ONESCREEN_HI};
        mirror_mode = modes[data & 0x03];
    }
}

void Mapper_018::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.writeBytes(prg, sizeof(prg));
    w.writeBytes(chr, sizeof(chr));
    w.write(ram_control);
    w.write(irq_reload);
    w.write(irq_counter);
    w.write(irq_control);
    w.write(irq_pending);
}

void Mapper_018::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.readBytes(prg, sizeof(prg));
    r.readBytes(chr, sizeof(chr));
    r.read(ram_control);
    r.read(irq_reload);
    r.read(irq_counter);
    r.read(irq_control);
    r.read(irq_pending);
}
