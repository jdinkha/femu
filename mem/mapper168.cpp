#include "mapper168.h"
#include "serialize.h"

void Mapper_168::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(chr_page);
    w.write(irq_hold);
    w.write(ram_protected);
    w.write(counter);
}

void Mapper_168::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(chr_page);
    r.read(irq_hold);
    r.read(ram_protected);
    r.read(counter);
}
