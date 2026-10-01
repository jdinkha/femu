#pragma once
#include "mapper004.h"

// TxSROM (mapper 118): MMC3 boards that wire CHR A17 to the nametable RAM's
// A10 instead of CHR-ROM. The MMC3 ignores PPU A13 when banking, so a
// nametable fetch from $2000-$2FFF gets the CHR bank for $0000-$0FFF -
// whose bit 7 then picks the CIRAM page for that quadrant. $A000 mirroring
// does nothing. Armadillo, Pro Sport Hockey, NES Play Action Football.
class Mapper_118 : public Mapper_004 {
public:
    Mapper_118(uint8_t prgBanks, uint8_t chrBanks) : Mapper_004(prgBanks, chrBanks) {
        UpdateBanks();
    }

    int nametablePage(uint8_t quadrant) const override { return chr_raw[quadrant & 3] >> 7; }

protected:
    uint32_t ChrBank(uint8_t raw) const override { return Mapper_004::ChrBank(raw & 0x7F); }
};
