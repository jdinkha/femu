#include "mapper065.h"
#include "serialize.h"

void Mapper_065::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.writeBytes(prg, sizeof(prg));
    w.write(prg_mode);
    w.write(irq_enabled);
    w.write(irq_pending);
    w.write(irq_counter);
    w.write(irq_reload);
}

void Mapper_065::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.readBytes(prg, sizeof(prg));
    r.read(prg_mode);
    r.read(irq_enabled);
    r.read(irq_pending);
    r.read(irq_counter);
    r.read(irq_reload);
}
