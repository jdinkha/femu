#include "mapper073.h"
#include "serialize.h"

void Mapper_073::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(irq_latch);
    w.write(irq_counter);
    w.write(irq_control);
    w.write(irq_pending);
}

void Mapper_073::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(irq_latch);
    r.read(irq_counter);
    r.read(irq_control);
    r.read(irq_pending);
}
