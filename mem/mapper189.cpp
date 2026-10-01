#include "mapper189.h"
#include "serialize.h"

void Mapper_189::SerializeState(StateWriter& w) const {
    Mapper_004::SerializeState(w);
    w.write(prg_bank);
}

void Mapper_189::DeserializeState(StateReader& r) {
    Mapper_004::DeserializeState(r);
    r.read(prg_bank);
}
