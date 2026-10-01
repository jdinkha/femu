#include "mapper210.h"
#include "serialize.h"

void Mapper_210::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(ram_enabled);
}

void Mapper_210::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(ram_enabled);
}
