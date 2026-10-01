#include "mapper064.h"
#include "serialize.h"

void Mapper_064::WriteRegister(uint16_t addr, uint8_t data) {
    const bool odd = addr & 1;
    switch (addr & 0xE000) {
        case 0x8000:
            if (!odd) control = data;
            else      regs[control & 0x0F] = data;
            Update();
            break;
        case 0xA000:
            if (!odd && !tlsrom) mirror_mode = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
            break;
        case 0xC000:
            if (!odd) {
                irq_latch = data;
            } else {
                irq_cycle_mode = data & 0x01;
                irq_reload = true;
                prescaler = 0;
            }
            break;
        case 0xE000:
            if (!odd) { irq_enabled = false; irq_pending = false; irq_delay = 0; }
            else      irq_enabled = true;
            break;
    }
}

void Mapper_064::Update() {
    const bool prg_mode = control & 0x40;
    SetPrg8k(0, prg_mode ? regs[15] : regs[6]);
    SetPrg8k(1, regs[7]);
    SetPrg8k(2, prg_mode ? regs[6] : regs[15]);
    SetPrg8k(3, -1);

    const bool k = control & 0x20;
    uint8_t left[4]; // the half controlled by R0/R1 (and R8/R9)
    left[0] = k ? regs[0] : (regs[0] & 0xFE);
    left[1] = k ? regs[8] : (regs[0] | 0x01);
    left[2] = k ? regs[1] : (regs[1] & 0xFE);
    left[3] = k ? regs[9] : (regs[1] | 0x01);
    const int lo = (control & 0x80) ? 4 : 0;
    const int hi = (control & 0x80) ? 0 : 4;
    for (int i = 0; i < 4; i++) {
        chr_raw[lo + i] = left[i];
        chr_raw[hi + i] = regs[2 + i];
    }
    for (int i = 0; i < 8; i++) SetChr1k(i, tlsrom ? (chr_raw[i] & 0x7F) : chr_raw[i]);
}

void Mapper_064::ClockCounter() {
    if (irq_reload) {
        irq_counter = irq_latch ? (irq_latch | 0x01) : 0;
        irq_reload = false;
    } else if (irq_counter == 0) {
        irq_counter = irq_latch;
    } else {
        irq_counter--;
    }
    if (irq_counter == 0 && irq_enabled) irq_delay = 4;
}

void Mapper_064::cpuClock() {
    if (irq_delay && --irq_delay == 0) irq_pending = true;
    if (irq_cycle_mode && ++prescaler == 4) {
        prescaler = 0;
        ClockCounter();
    }
}

void Mapper_064::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(control);
    w.writeBytes(regs, sizeof(regs));
    w.writeBytes(chr_raw, sizeof(chr_raw));
    w.write(irq_latch);
    w.write(irq_counter);
    w.write(irq_reload);
    w.write(irq_enabled);
    w.write(irq_cycle_mode);
    w.write(irq_pending);
    w.write(prescaler);
    w.write(irq_delay);
}

void Mapper_064::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(control);
    r.readBytes(regs, sizeof(regs));
    r.readBytes(chr_raw, sizeof(chr_raw));
    r.read(irq_latch);
    r.read(irq_counter);
    r.read(irq_reload);
    r.read(irq_enabled);
    r.read(irq_cycle_mode);
    r.read(irq_pending);
    r.read(prescaler);
    r.read(irq_delay);
}
