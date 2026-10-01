#pragma once
#include "banked_mapper.h"

// CNROM (mapper 3): fixed 16/32KB PRG, 8KB switchable CHR-ROM. The original
// board always has AND-type bus conflicts (Cybernoid depends on them), so
// they're on unless the NES 2.0 submapper says the board has none (1).
//
// Mapper 185 is CNROM wired as copy protection: the "bank" bits are really
// chip selects, and CHR-ROM only answers when they match - otherwise the
// game reads garbage, which it checks for. NES 2.0 submappers 4-7 give the
// matching value directly; without one, the usual rule is that CHR is
// enabled when either low bit is set, except for $13 (Spy vs. Spy).
class Mapper_003 : public BankedMapper {
public:
    Mapper_003(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId, uint8_t submapper);

    bool ppuReadHook(uint16_t addr, uint8_t& data) override;

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override;

private:
    bool bus_conflicts;
    bool protection;      // mapper 185
    int chip_select = -1; // mapper 185 submapper 4-7: required CS1/CS0 value, else -1
    bool chr_enabled = true;
};
