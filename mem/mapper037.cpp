#include "mapper037.h"
#include "serialize.h"

void Mapper_037::SerializeState(StateWriter& w) const {
    Mapper_004::SerializeState(w);
    w.write(outer);
}

void Mapper_037::DeserializeState(StateReader& r) {
    Mapper_004::DeserializeState(r);
    r.read(outer);
    UpdateBanks();
}
