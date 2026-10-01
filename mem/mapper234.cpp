#include "mapper234.h"
#include "serialize.h"

void Mapper_234::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(outer);
    w.write(inner);
}

void Mapper_234::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(outer);
    r.read(inner);
}
