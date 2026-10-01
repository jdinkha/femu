#include "mapper032.h"
#include "serialize.h"

void Mapper_032::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.writeBytes(prg, sizeof(prg));
    w.write(prg_mode);
}

void Mapper_032::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.readBytes(prg, sizeof(prg));
    r.read(prg_mode);
}
