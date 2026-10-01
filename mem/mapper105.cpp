#include "mapper105.h"
#include "serialize.h"

Mapper_105::Mapper_105(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
    Update();
}

void Mapper_105::WriteRegister(uint16_t addr, uint8_t data) {
    if (data & 0x80) { // MMC1 reset: clear the shift register, force PRG mode 3
        shift = 0;
        shift_count = 0;
        control |= 0x0C;
        Update();
        return;
    }
    shift = (uint8_t)((shift >> 1) | ((data & 0x01) << 4));
    if (++shift_count < 5) return;

    switch ((addr >> 13) & 0x03) {
        case 0: control = shift; break;
        case 1:
            reg_a = shift;
            if (!(reg_a & 0x10)) seen_zero = true;
            else if (seen_zero) unlocked = true;
            break;
        case 2: break; // CHR bank 1: unused, the 8KB of CHR-RAM isn't banked
        case 3: prg_b = shift; break;
    }
    shift = 0;
    shift_count = 0;
    Update();
}

void Mapper_105::Update() {
    switch (control & 0x03) {
        case 0: mirror_mode = Mirror::ONESCREEN_LO; break;
        case 1: mirror_mode = Mirror::ONESCREEN_HI; break;
        case 2: mirror_mode = Mirror::VERTICAL; break;
        case 3: mirror_mode = Mirror::HORIZONTAL; break;
    }

    if (!unlocked) {
        SetPrg32k(0);
    } else if (!(reg_a & 0x08)) {
        SetPrg32k((reg_a >> 1) & 0x03);              // first chip, 32KB banks
    } else {
        int bank = 8 + (prg_b & 0x07);               // second chip, in 16KB banks
        switch ((control >> 2) & 0x03) {
            case 0: case 1: SetPrg32k(bank >> 1); break;
            case 2: SetPrg16k(0, 8);    SetPrg16k(1, bank); break;
            case 3: SetPrg16k(0, bank); SetPrg16k(1, 15);   break;
        }
    }

    if (reg_a & 0x10) { // timer held in reset, IRQ acknowledged
        counter = 0;
        irq_pending = false;
    }
}

void Mapper_105::cpuClock() {
    if (reg_a & 0x10) return;
    if (++counter == kTimerTarget) irq_pending = true;
}

void Mapper_105::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.write(shift);
    w.write(shift_count);
    w.write(control);
    w.write(reg_a);
    w.write(prg_b);
    w.write(seen_zero);
    w.write(unlocked);
    w.write(counter);
    w.write(irq_pending);
}

void Mapper_105::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.read(shift);
    r.read(shift_count);
    r.read(control);
    r.read(reg_a);
    r.read(prg_b);
    r.read(seen_zero);
    r.read(unlocked);
    r.read(counter);
    r.read(irq_pending);
}
