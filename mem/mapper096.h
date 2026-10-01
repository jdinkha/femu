#pragma once
#include "banked_mapper.h"

// Bandai Oeka Kids (mapper 96): 32KB PRG bank and 16KB outer CHR-RAM bank
// in a $8000-$FFFF register (bits 0-1 / bit 2, bus conflicts). The inner
// 4KB CHR bank for PPU $0000 is latched from PPU address bits 8-9 whenever
// the PPU's address moves into $2000-$2FFF - i.e. on each nametable fetch
// - which turns the background into an addressable bitmap; $1000 is fixed
// to the outer bank's last 4KB. The games also need the Oeka Kids tablet,
// which femu doesn't emulate.
class Mapper_096 : public BankedMapper {
public:
    Mapper_096(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        chr_ram_size = 0x8000;
        SetPrg32k(0);
        Update();
    }

    bool ppuReadHook(uint16_t addr, uint8_t& data) override { (void)data; Snoop(addr); return false; }
    bool ppuWriteHook(uint16_t addr, uint8_t data) override { (void)data; Snoop(addr); return false; }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        data = BusConflict(addr, data);
        SetPrg32k(data & 0x03);
        outer = (data >> 2) & 0x01;
        Update();
    }

private:
    uint8_t outer = 0, inner = 0;
    bool in_nametables = false;

    void Snoop(uint16_t addr) {
        bool nt = (addr & 0x3000) == 0x2000;
        if (nt && !in_nametables) {
            inner = (addr >> 8) & 0x03;
            Update();
        }
        in_nametables = nt;
    }
    void Update() {
        SetChr4k(0, outer * 4 + inner);
        SetChr4k(1, outer * 4 + 3);
    }
};
