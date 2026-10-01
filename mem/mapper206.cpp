#include "mapper206.h"
#include "serialize.h"

Mapper_206::Mapper_206(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId, uint8_t submapper)
    : BankedMapper(prgBanks, chrBanks), variant(mapperId), fixed_prg(mapperId == 206 && submapper == 1) {
    if (variant == 154) mirror_mode = Mirror::ONESCREEN_LO;
    Update();
}

void Mapper_206::WriteRegister(uint16_t addr, uint8_t data) {
    if (variant == 154) mirror_mode = (data & 0x40) ? Mirror::ONESCREEN_HI : Mirror::ONESCREEN_LO;
    if (addr >= 0xA000) return;
    if (!(addr & 1)) {
        select = data & 0x07;
    } else {
        regs[select] = data & 0x3F;
        Update();
    }
}

void Mapper_206::Update() {
    if (fixed_prg) {
        SetPrg32k(0);
    } else {
        SetPrg8k(0, regs[6] & 0x0F);
        SetPrg8k(1, regs[7] & 0x0F);
        SetPrg8k(2, -2);
        SetPrg8k(3, -1);
    }

    if (variant == 76) {
        for (int i = 0; i < 4; i++) SetChr2k(i, regs[2 + i]);
        return;
    }
    // 1KB bank numbers for $0000-$0FFF (two 2KB banks) and $1000-$1FFF.
    int banks[8];
    for (int i = 0; i < 2; i++) {
        int b = regs[i] & (variant == 95 ? 0x1E : 0x3E);
        banks[i * 2] = b;
        banks[i * 2 + 1] = b | 1;
    }
    for (int i = 0; i < 4; i++) banks[4 + i] = regs[2 + i] & (variant == 95 ? 0x1F : 0x3F);
    if (variant == 88 || variant == 154) {
        for (int i = 0; i < 8; i++) banks[i] = (banks[i] & 0x3F) | (i >= 4 ? 0x40 : 0x00);
    }
    for (int i = 0; i < 8; i++) SetChr1k(i, banks[i]);
}

void Mapper_206::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(select);
    w.writeBytes(regs, sizeof(regs));
}

void Mapper_206::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(select);
    r.readBytes(regs, sizeof(regs));
}
