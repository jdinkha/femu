#include "mapper068.h"
#include "serialize.h"

void Mapper_068::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.writeBytes(nt, sizeof(nt));
    w.write(rom_nametables);
    w.write(ram_enabled);
}

void Mapper_068::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.readBytes(nt, sizeof(nt));
    r.read(rom_nametables);
    r.read(ram_enabled);
}
