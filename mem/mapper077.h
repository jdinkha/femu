#pragma once
#include "banked_mapper.h"

// Irem (mapper 77, Napoleon Senki): one register at $8000-$FFFF (bus
// conflicts) selecting a 32KB PRG bank (bits 0-3) and a 2KB CHR-ROM bank
// for PPU $0000 (bits 4-7). The board's 8KB of RAM fills the rest of the
// pattern tables ($0800-$1FFF) and the first two nametables ($2000-$27FF);
// the console's own CIRAM backs $2800-$2FFF - four nametables in all.
class Mapper_077 : public BankedMapper {
public:
    Mapper_077(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
        chr_ram_size = 0x2000;
        SetPrg32k(0);
        SetChr2k(0, 0);
        for (int i = 2; i < 8; i++) SetChrRam1k(i, i - 2);
    }

    // RAM 1KB pages 6-7 hold nametables 0-1; CIRAM pages 0-1 hold 2-3.
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override {
        if (NametableRam(addr, mapped_addr)) return true;
        return BankedMapper::ppuMapRead(addr, mapped_addr);
    }
    bool ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) override {
        if (NametableRam(addr, mapped_addr)) return true;
        return BankedMapper::ppuMapWrite(addr, mapped_addr);
    }
    int nametablePage(uint8_t quadrant) const override { return quadrant & 1; }

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        data = BusConflict(addr, data);
        SetPrg32k(data & 0x0F);
        SetChr2k(0, data >> 4);
    }

private:
    bool NametableRam(uint16_t addr, uint32_t& mapped_addr) const {
        if (addr < 0x2000 || addr > 0x3EFF || (addr & 0x0800)) return false;
        mapped_addr = (uint32_t)nCHRBanks * 0x2000 + 0x1800 + (addr & 0x07FF);
        return true;
    }
};
