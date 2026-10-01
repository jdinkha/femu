#include "mapper009.h"
#include "serialize.h"

Mapper_009::Mapper_009(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId)
    : BankedMapper(prgBanks, chrBanks), mmc4(mapperId == 10) {
    if (mmc4) {
        SetPrg16k(0, 0);
        SetPrg16k(1, -1);
    } else {
        SetPrg8k(0, 0);
        SetPrg8k(1, -3);
        SetPrg8k(2, -2);
        SetPrg8k(3, -1);
    }
    mirror_mode = Mirror::VERTICAL;
    UpdateChr();
}

void Mapper_009::WriteRegister(uint16_t addr, uint8_t data) {
    switch (addr & 0xF000) {
        case 0xA000:
            if (mmc4) SetPrg16k(0, data & 0x0F);
            else      SetPrg8k(0, data & 0x0F);
            break;
        case 0xB000: chr[0] = data & 0x1F; UpdateChr(); break;
        case 0xC000: chr[1] = data & 0x1F; UpdateChr(); break;
        case 0xD000: chr[2] = data & 0x1F; UpdateChr(); break;
        case 0xE000: chr[3] = data & 0x1F; UpdateChr(); break;
        case 0xF000: mirror_mode = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL; break;
        default: break;
    }
}

void Mapper_009::UpdateChr() {
    SetChr4k(0, latch[0] == 0xFD ? chr[0] : chr[1]);
    SetChr4k(1, latch[1] == 0xFD ? chr[2] : chr[3]);
}

bool Mapper_009::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (!BankedMapper::ppuMapRead(addr, mapped_addr)) return false;

    // Update the latches after the fetch they're triggered by.
    uint16_t a = addr & 0x1FFF;
    bool exact = !mmc4 && a < 0x1000; // MMC2 latch 0 watches one address only
    uint16_t row = exact ? a : (uint16_t)(a & 0x1FF8);
    int which = a >> 12;
    uint16_t base = which ? 0x1000 : 0x0000;
    if (row == base + 0x0FD8)      { latch[which] = 0xFD; UpdateChr(); }
    else if (row == base + 0x0FE8) { latch[which] = 0xFE; UpdateChr(); }
    return true;
}

void Mapper_009::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.writeBytes(chr, sizeof(chr));
    w.writeBytes(latch, sizeof(latch));
}

void Mapper_009::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.readBytes(chr, sizeof(chr));
    r.readBytes(latch, sizeof(latch));
}
