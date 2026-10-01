#include "mapper232.h"
#include "serialize.h"

void Mapper_232::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(block);
    w.write(page);
}

void Mapper_232::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(block);
    r.read(page);
}
