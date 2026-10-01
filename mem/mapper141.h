#pragma once
#include "banked_mapper.h"
#include <array>

// Sachen 8259A (mapper 141): The Great Wall, Super Cartridge, Chinese Kungfu.
// $4100 (mask $C101) selects an internal register, $4101 writes it:
//   0-3: low 3 bits of the CHR bank for PPU $0000/$0800/$1000/$1800
//   4:   high 3 bits for all four
//   5:   32KB PRG bank
//   7:   bit 0 "simple" mode (CHR reg 0 everywhere, vertical mirroring),
//        bits 1-2 mirroring (V, H, page 0 then 1/1/1, one-screen A)
// The A board shifts the 6-bit CHR bank up by one, so each 2KB window shows
// half of a 4KB bank, picked by PPU A11. Q-Boy uses the same number with
// unbanked CHR-RAM instead.
class Mapper_141 : public BankedMapper {
public:
    Mapper_141(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        prg_ram_size = 0;
        Update();
    }

    bool cpuWriteHook(uint16_t addr, uint8_t data) override {
        if ((addr & 0xC101) == 0x4100)      select = data & 0x07;
        else if ((addr & 0xC101) == 0x4101) { regs[select] = data & 0x07; Update(); }
        else return false;
        return true;
    }

    int nametablePage(uint8_t quadrant) const override {
        if (!(regs[7] & 0x01) && ((regs[7] >> 1) & 0x03) == 2) return quadrant == 0 ? 0 : 1;
        return -1;
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

private:
    uint8_t select = 0;
    std::array<uint8_t, 8> regs{};

    void Update() {
        SetPrg32k(regs[5]);
        bool simple = regs[7] & 0x01;
        if (nCHRBanks) {
            for (int w = 0; w < 4; w++) {
                int bank = (regs[4] << 3) | regs[simple ? 0 : w];
                SetChr2k(w, bank * 2 + (w & 1));
            }
        }
        switch (simple ? 0 : (regs[7] >> 1) & 0x03) {
            case 0: mirror_mode = Mirror::VERTICAL; break;
            case 1: mirror_mode = Mirror::HORIZONTAL; break;
            case 2: mirror_mode = Mirror::ONESCREEN_HI; break; // see nametablePage
            case 3: mirror_mode = Mirror::ONESCREEN_LO; break;
        }
    }
};
