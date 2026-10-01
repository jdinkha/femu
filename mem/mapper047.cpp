#include "mapper047.h"
#include "serialize.h"

void Mapper_047::SerializeState(StateWriter& w) const {
    Mapper_004::SerializeState(w);
    w.write(block);
}

void Mapper_047::DeserializeState(StateReader& r) {
    Mapper_004::DeserializeState(r);
    r.read(block);
    UpdateBanks();
}
