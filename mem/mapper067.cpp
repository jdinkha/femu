#include "mapper067.h"
#include "serialize.h"

void Mapper_067::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(irq_counter);
    w.write(irq_enabled);
    w.write(irq_pending);
    w.write(irq_toggle);
}

void Mapper_067::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(irq_counter);
    r.read(irq_enabled);
    r.read(irq_pending);
    r.read(irq_toggle);
}
