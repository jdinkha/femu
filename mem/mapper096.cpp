#include "mapper096.h"
#include "serialize.h"

void Mapper_096::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(outer);
    w.write(inner);
    w.write(in_nametables);
}

void Mapper_096::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(outer);
    r.read(inner);
    r.read(in_nametables);
}
