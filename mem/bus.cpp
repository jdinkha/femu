#include "bus.h"

// NES CPU memory map:
//   $0000-$1FFF  2KB internal RAM, mirrored every $0800
//   $2000-$3FFF  PPU registers (8 of them), mirrored every 8 bytes
//   $4000-$4013,$4015,$4017  APU registers
//   $4014        OAMDMA (sprite upload)
//   $4016,$4017  controller 1 / controller 2 (reads); $4016 also strobes both
//   $4020-$7FFF  unmapped on NROM (some mappers use this for PRG-RAM)
//   $8000-$FFFF  cartridge PRG-ROM, via the mapper

Bus::Bus() {
    cpu.ConnectBus(this);
    apu.ConnectBus(this);
    ram.fill(0x00);
}

void Bus::insertCartridge(const std::shared_ptr<Cartridge>& cart) {
    cartridge = cart;
    ppu.ConnectCartridge(cart);
}

void Bus::reset() {
    cpu.reset();
    ppu.reset();
    apu.reset();
    system_clock_counter = 0;
}

void Bus::clock() {
    // PPU runs 3x the CPU's rate - this is the actual master clock of the
    // system; the CPU is downstream of it, not the other way around.
    ppu.clock();
    if (system_clock_counter % 3 == 0) {
        cpu.clock();
        apu.clock(); // APU's internal timers are already specified in CPU cycles

        // Downsample the APU's per-CPU-cycle output to ~44.1kHz. Simple
        // sample-and-hold decimation rather than a proper resampling
        // filter - fine for a first pass, can introduce mild aliasing on
        // very high-pitched sound effects.
        audio_time_accumulator += 1.0 / CPU_CLOCK_HZ;
        if (audio_time_accumulator >= 1.0 / AUDIO_SAMPLE_RATE) {
            audio_time_accumulator -= 1.0 / AUDIO_SAMPLE_RATE;
            audio_samples.push_back((float)apu.GetOutputSample());
        }
    }

    if (ppu.nmi) {
        ppu.nmi = false;
        cpu.nmi();
    }

    // Level-triggered, unlike NMI: safe to call every cycle while pending,
    // since CPU::irq() itself no-ops once the I flag is set - it won't
    // retrigger until the interrupt handler clears I again (and by then
    // the game should have acknowledged the source via $4015).
    if (apu.IRQPending()) {
        cpu.irq();
    }

    system_clock_counter++;
}

void Bus::cpuWrite(uint16_t addr, uint8_t data) {
    if (cartridge && cartridge->cpuWrite(addr, data)) {
        // handled by cartridge/mapper (PRG-RAM, bank-select registers, etc.)
    } else if (addr <= 0x1FFF) {
        ram[addr & 0x07FF] = data;
    } else if (addr <= 0x3FFF) {
        ppu.cpuWrite(addr & 0x0007, data);
    } else if (addr == 0x4014) {
        // OAMDMA: copy 256 bytes from CPU page $data00-$dataFF into PPU OAM.
        // Real hardware stalls the CPU ~513-514 cycles during this; we do
        // the whole copy instantly instead, which is fine for correctness
        // but not cycle-accurate (a future refinement if timing bugs show up).
        uint16_t page = (uint16_t)(data << 8);
        for (int i = 0; i < 256; i++) {
            ppu.WriteOAMByte((uint8_t)i, cpuRead((uint16_t)(page + i)));
        }
    } else if (addr == 0x4016) {
        // Strobe: latch the current button state for both controllers so it
        // can be shifted out on subsequent $4016/$4017 reads. Real hardware
        // technically keeps re-latching continuously while the strobe bit
        // stays high and only starts shifting once it goes low, but every
        // real game just writes 1 then 0 before reading, so latching on any
        // write here produces identical behavior in practice.
        controller_state[0] = controller[0];
        controller_state[1] = controller[1];
    } else if (addr <= 0x4013 || addr == 0x4015 || addr == 0x4017) {
        apu.cpuWrite(addr, data);
    }
}

uint8_t Bus::cpuRead(uint16_t addr) {
    uint8_t data = 0x00;
    if (cartridge && cartridge->cpuRead(addr, data)) {
        return data;
    } else if (addr <= 0x1FFF) {
        return ram[addr & 0x07FF];
    } else if (addr <= 0x3FFF) {
        return ppu.cpuRead(addr & 0x0007);
    } else if (addr == 0x4016 || addr == 0x4017) {
        // Each read returns the next bit (A first), then shifts it out.
        // Shifting in a 1 (not 0) matches real hardware: reads past the 8th
        // button return 1 indefinitely (open-bus pull-up), not 0.
        uint8_t index = (uint8_t)(addr & 0x0001);
        uint8_t bit = (controller_state[index] & 0x80) > 0;
        controller_state[index] = (uint8_t)((controller_state[index] << 1) | 0x01);
        return bit;
    } else if (addr == 0x4015) {
        return apu.cpuRead(addr);
    }
    return 0x00;
}
