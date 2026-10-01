#include "mapper075.h"
#include "serialize.h"

void Mapper_075::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.writeBytes(chr, sizeof(chr));
}

void Mapper_075::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.readBytes(chr, sizeof(chr));
}
