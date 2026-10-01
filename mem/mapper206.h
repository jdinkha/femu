#pragma once
#include "banked_mapper.h"

// Namco 108 / Tengen MIMIC-1 (mapper 206, the MMC3's simpler predecessor)
// and its board variants. $8000 (even) selects a register, $8001 (odd)
// writes it (mask $E001, so only $8000-$9FFF):
//   R0, R1  2KB CHR banks at $0000, $0800
//   R2-R5   1KB CHR banks at $1000-$1C00
//   R6, R7  8KB PRG banks at $8000, $A000 ($C000/$E000 are the last two)
// No IRQ and hardwired mirroring. Variants:
//   76  (NAMCOT-3446) R2-R5 become 2KB banks covering all of $0000-$1FFF
//   88  (NAMCOT-34xx) PPU A12 drives CHR A16: $1000-$1FFF banks come from
//       the second 64KB
//   95  (NAMCOT-3425) bit 5 of R0/R1 picks the CIRAM page for $2000/$2800
//   154 (NAMCOT-3453) mapper 88 plus one-screen mirroring from bit 6 of
//       any write to $8000-$FFFF
//   206 submapper 1: 32KB PRG wired straight through, unbanked
class Mapper_206 : public BankedMapper {
public:
    Mapper_206(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId, uint8_t submapper);

    int nametablePage(uint8_t quadrant) const override {
        if (variant != 95) return -1;
        return (regs[quadrant < 2 ? 0 : 1] >> 5) & 1;
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override;

private:
    uint8_t variant;
    bool fixed_prg;
    uint8_t select = 0;
    uint8_t regs[8] = {0, 2, 4, 5, 6, 7, 0, 1};

    void Update();
};
