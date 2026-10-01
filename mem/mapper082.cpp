#include "mapper082.h"
#include "serialize.h"

void Mapper_082::Update() {
    const int lo = chr_invert ? 2 : 0; // 2KB-slot index of the two 2KB banks
    const int hi = chr_invert ? 0 : 4; // 1KB-slot index of the four 1KB banks
    SetChr2k(lo, chr[0] >> 1);
    SetChr2k(lo + 1, chr[1] >> 1);
    for (int i = 0; i < 4; i++) SetChr1k(hi + i, chr[2 + i]);
}

bool Mapper_082::cpuWriteHook(uint16_t addr, uint8_t data) {
    if (addr < 0x7EF0 || addr > 0x7EFF) return false;
    switch (addr & 0x0F) {
        case 0x0: case 0x1: case 0x2: case 0x3: case 0x4: case 0x5:
            chr[addr & 0x0F] = data;
            Update();
            break;
        case 0x6:
            mirror_mode = (data & 0x01) ? Mirror::VERTICAL : Mirror::HORIZONTAL;
            chr_invert = data & 0x02;
            Update();
            break;
        case 0x7: unlocked[0] = data == 0xCA; break;
        case 0x8: unlocked[1] = data == 0x69; break;
        case 0x9: unlocked[2] = data == 0x84; break;
        case 0xA: SetPrg8k(0, (data >> 2) & 0x0F); break;
        case 0xB: SetPrg8k(1, (data >> 2) & 0x0F); break;
        case 0xC: SetPrg8k(2, (data >> 2) & 0x0F); break;
    }
    return true;
}

void Mapper_082::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.writeBytes(chr, sizeof(chr));
    w.write(chr_invert);
    w.writeBytes(unlocked, sizeof(unlocked));
}

void Mapper_082::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.readBytes(chr, sizeof(chr));
    r.read(chr_invert);
    r.readBytes(unlocked, sizeof(unlocked));
}
