#include "mapper003.h"
#include "serialize.h"

Mapper_003::Mapper_003(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId, uint8_t submapper)
    : BankedMapper(prgBanks, chrBanks),
      bus_conflicts(submapper != 1),
      protection(mapperId == 185) {
    if (protection && submapper >= 4 && submapper <= 7) chip_select = submapper - 4;
    if (protection) chr_enabled = false; // nothing selected until the game writes
}

void Mapper_003::WriteRegister(uint16_t addr, uint8_t data) {
    if (bus_conflicts) data = BusConflict(addr, data);
    if (!protection) {
        SetChr8k(data);
        return;
    }
    if (chip_select >= 0) chr_enabled = (data & 0x03) == chip_select;
    else                  chr_enabled = (data & 0x03) != 0 && data != 0x13;
}

bool Mapper_003::ppuReadHook(uint16_t addr, uint8_t& data) {
    if (addr > 0x1FFF || chr_enabled) return false;
    data = 0xFF; // CHR-ROM deselected: the pattern bus floats high
    return true;
}

void Mapper_003::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(chr_enabled);
}

void Mapper_003::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(chr_enabled);
}
