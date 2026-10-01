#include "mapper093.h"
#include "serialize.h"

void Mapper_093::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(chr_enabled);
}

void Mapper_093::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(chr_enabled);
}
