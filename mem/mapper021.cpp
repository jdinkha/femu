#include "mapper021.h"

Mapper_021::Mapper_021(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId, uint8_t submapper)
    : BankedMapper(prgBanks, chrBanks) {
    // Address lines that drive the chip's A0/A1, per board.
    struct Pair { uint8_t a0, a1; };
    Pair first{0, 1}, second{0, 1};
    switch (mapperId) {
        case 21: first = {1, 2}; second = {6, 7}; // VRC4a, VRC4c
            if (submapper == 1) second = first;
            if (submapper == 2) first = second;
            break;
        case 22: first = second = {1, 0};         // VRC2a
            vrc4 = false;
            chr_half = true;
            break;
        case 23: first = {0, 1}; second = {2, 3}; // VRC2b/VRC4f, VRC4e
            if (submapper == 1 || submapper == 3) second = first;
            if (submapper == 2) first = second;
            vrc4 = submapper != 3;
            break;
        case 25: first = {1, 0}; second = {3, 2}; // VRC2c/VRC4b, VRC4d
            if (submapper == 1 || submapper == 3) second = first;
            if (submapper == 2) first = second;
            vrc4 = submapper != 3;
            break;
    }
    wiring[0][0] = first.a0;  wiring[0][1] = first.a1;
    wiring[1][0] = second.a0; wiring[1][1] = second.a1;
    wirings = (first.a0 == second.a0 && first.a1 == second.a1) ? 1 : 2;

    mirror_mode = Mirror::VERTICAL;
    UpdatePrg();
    for (int i = 0; i < 8; i++) SetChr1k(i, 0);
}

uint8_t Mapper_021::RegisterSelect(uint16_t addr) const {
    uint8_t sel = 0;
    for (int i = 0; i < wirings; i++) {
        sel |= (uint8_t)(((addr >> wiring[i][0]) & 1) | (((addr >> wiring[i][1]) & 1) << 1));
    }
    return sel;
}

void Mapper_021::UpdatePrg() {
    SetPrg8k(swap_mode ? 2 : 0, prg[0]);
    SetPrg8k(swap_mode ? 0 : 2, -2);
    SetPrg8k(1, prg[1]);
    SetPrg8k(3, -1);
}

void Mapper_021::WriteRegister(uint16_t addr, uint8_t data) {
    const uint8_t sel = RegisterSelect(addr);
    switch (addr & 0xF000) {
        case 0x8000:
            prg[0] = data & 0x1F;
            UpdatePrg();
            break;
        case 0x9000:
            if (!vrc4) {
                mirror_mode = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            } else if (sel == 0) {
                static const Mirror modes[4] = {Mirror::VERTICAL, Mirror::HORIZONTAL,
                                                Mirror::ONESCREEN_LO, Mirror::ONESCREEN_HI};
                mirror_mode = modes[data & 0x03];
            } else if (sel == 2) {
                swap_mode = data & 0x02;
                UpdatePrg();
            }
            break;
        case 0xA000:
            prg[1] = data & 0x1F;
            UpdatePrg();
            break;
        case 0xB000: case 0xC000: case 0xD000: case 0xE000: {
            int slot = ((addr >> 12) - 0xB) * 2 + (sel >> 1);
            if (sel & 1) chr[slot] = (uint16_t)((chr[slot] & 0x000F) | ((data & (vrc4 ? 0x1F : 0x0F)) << 4));
            else         chr[slot] = (uint16_t)((chr[slot] & 0x01F0) | (data & 0x0F));
            SetChr1k(slot, chr_half ? chr[slot] >> 1 : chr[slot]);
            break;
        }
        case 0xF000:
            if (!vrc4) break;
            switch (sel) {
                case 0: irq.WriteLatchLow(data); break;
                case 1: irq.WriteLatchHigh(data); break;
                case 2: irq.WriteControl(data); break;
                case 3: irq.Acknowledge(); break;
            }
            break;
    }
}

void Mapper_021::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.writeBytes(prg, sizeof(prg));
    w.write(swap_mode);
    w.writeBytes(chr, sizeof(chr));
    irq.Serialize(w);
}

void Mapper_021::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.readBytes(prg, sizeof(prg));
    r.read(swap_mode);
    r.readBytes(chr, sizeof(chr));
    irq.Deserialize(r);
}
