#include "mapper080.h"
#include "serialize.h"

bool Mapper_080::cpuWriteHook(uint16_t addr, uint8_t data) {
    if ((addr & 0xFF70) != 0x7E70) return false;
    switch (addr & 0x0F) {
        case 0x0: case 0x1: {
            int which = addr & 1;
            chr_regs[which] = data;
            SetChr2k(which, (single_screen_bits ? data & 0x7F : data) >> 1);
            break;
        }
        case 0x2: case 0x3: case 0x4: case 0x5:
            SetChr1k(4 + (addr & 0x0F) - 2, data);
            break;
        case 0x6:
            if (!single_screen_bits) mirror_mode = (data & 0x01) ? Mirror::VERTICAL : Mirror::HORIZONTAL;
            break;
        case 0x8: case 0x9: ram_unlocked = data == 0xA3; break;
        case 0xA: case 0xB: SetPrg8k(0, data & 0x3F); break;
        case 0xC: case 0xD: SetPrg8k(1, data & 0x3F); break;
        case 0xE: case 0xF: SetPrg8k(2, data & 0x3F); break;
    }
    return true;
}

void Mapper_080::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.writeBytes(chr_regs, sizeof(chr_regs));
    w.write(ram_unlocked);
}

void Mapper_080::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.readBytes(chr_regs, sizeof(chr_regs));
    r.read(ram_unlocked);
}
