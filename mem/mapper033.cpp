#include "mapper033.h"
#include "serialize.h"

void Mapper_033::WriteRegister(uint16_t addr, uint8_t data) {
    switch (addr & (tc0690 ? 0xE003 : 0xA003)) {
        case 0x8000:
            SetPrg8k(0, data & 0x3F);
            if (!tc0690) mirror_mode = (data & 0x40) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            break;
        case 0x8001: SetPrg8k(1, data & 0x3F); break;
        case 0x8002: SetChr2k(0, data); break;
        case 0x8003: SetChr2k(1, data); break;
        case 0xA000: case 0xA001: case 0xA002: case 0xA003:
            SetChr1k(4 + (addr & 3), data);
            break;
        case 0xC000: irq_latch = data ^ 0xFF; break;
        case 0xC001: irq_counter = 0; irq_reload = true; break;
        case 0xC002: irq_enabled = true; break;
        case 0xC003: irq_enabled = false; irq_pending = false; irq_delay = 0; break;
        case 0xE000: mirror_mode = (data & 0x40) ? Mirror::HORIZONTAL : Mirror::VERTICAL; break;
    }
}

// Same counter as the MMC3 (see Mapper_004::ScanlineIRQ).
void Mapper_033::ScanlineIRQ() {
    if (!tc0690) return;
    if (irq_counter == 0 || irq_reload) {
        irq_counter = irq_latch;
        irq_reload = false;
    } else {
        irq_counter--;
    }
    if (irq_counter == 0 && irq_enabled) irq_delay = 4;
}

void Mapper_033::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(irq_latch);
    w.write(irq_counter);
    w.write(irq_reload);
    w.write(irq_enabled);
    w.write(irq_pending);
    w.write(irq_delay);
}

void Mapper_033::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(irq_latch);
    r.read(irq_counter);
    r.read(irq_reload);
    r.read(irq_enabled);
    r.read(irq_pending);
    r.read(irq_delay);
}
