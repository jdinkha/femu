#pragma once
#include "banked_mapper.h"

// Konami VRC1 (mapper 75): Ganbare Goemon, King Kong 2, Exciting Boxing.
//   $8000/$A000/$C000  8KB PRG banks ($E000 is the last bank)
//   $9000  bit 0 mirroring (0 = V), bits 1/2 high bits of the two CHR banks
//   $E000/$F000  low four bits of the 4KB CHR banks at $0000/$1000
class Mapper_075 : public BankedMapper {
public:
    Mapper_075(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        SetPrg8k(0, 0);
        SetPrg8k(1, 0);
        SetPrg8k(2, 0);
        SetPrg8k(3, -1);
        Update();
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        switch (addr & 0xF000) {
            case 0x8000: SetPrg8k(0, data & 0x0F); break;
            case 0xA000: SetPrg8k(1, data & 0x0F); break;
            case 0xC000: SetPrg8k(2, data & 0x0F); break;
            case 0x9000:
                mirror_mode = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
                chr[0] = (uint8_t)((chr[0] & 0x0F) | ((data & 0x02) << 3));
                chr[1] = (uint8_t)((chr[1] & 0x0F) | ((data & 0x04) << 2));
                Update();
                break;
            case 0xE000: chr[0] = (uint8_t)((chr[0] & 0x10) | (data & 0x0F)); Update(); break;
            case 0xF000: chr[1] = (uint8_t)((chr[1] & 0x10) | (data & 0x0F)); Update(); break;
        }
    }

private:
    uint8_t chr[2] = {0, 0};

    void Update() {
        SetChr4k(0, chr[0]);
        SetChr4k(1, chr[1]);
    }
};
