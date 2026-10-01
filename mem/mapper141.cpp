#include "mapper141.h"
#include "serialize.h"

void Mapper_141::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(select);
    w.writeBytes(regs.data(), sizeof(regs));
}

void Mapper_141::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(select);
    r.readBytes(regs.data(), sizeof(regs));
}
