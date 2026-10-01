#include "mapper228.h"
#include "serialize.h"

void Mapper_228::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(chip_missing);
}

void Mapper_228::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(chip_missing);
}
