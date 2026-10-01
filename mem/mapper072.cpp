#include "mapper072.h"
#include "serialize.h"

void Mapper_072::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(last);
}

void Mapper_072::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(last);
}
