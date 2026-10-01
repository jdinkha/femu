#include "mapper041.h"
#include "serialize.h"

void Mapper_041::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(outer);
    w.write(inner);
}

void Mapper_041::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(outer);
    r.read(inner);
}
