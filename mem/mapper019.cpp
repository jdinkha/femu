#include "mapper019.h"
#include "serialize.h"

Mapper_019::Mapper_019(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {
    prg_ram_size = 0x2000 + 0x80; // 8KB external + the chip's 128 bytes
    SetPrg8k(0, 0);
    SetPrg8k(1, 0);
    SetPrg8k(2, 0);
    SetPrg8k(3, -1);
}

bool Mapper_019::cpuReadHook(uint16_t addr, uint8_t& data) {
    switch (addr & 0xF800) {
        case 0x4800:
            data = InternalRam(ram_address);
            if ((ram_address & 0x80) && (ram_address & 0x7F) < 0x7F) ram_address++;
            return true;
        case 0x5000: data = (uint8_t)(irq_counter & 0xFF); return true;
        case 0x5800: data = (uint8_t)(irq_counter >> 8); return true;
        default: return false;
    }
}

bool Mapper_019::cpuWriteHook(uint16_t addr, uint8_t data) {
    switch (addr & 0xF800) {
        case 0x4800:
            InternalRam(ram_address) = data;
            if ((ram_address & 0x80) && (ram_address & 0x7F) < 0x7F) ram_address++;
            return true;
        case 0x5000:
            irq_counter = (uint16_t)((irq_counter & 0xFF00) | data);
            irq_pending = false;
            return true;
        case 0x5800:
            irq_counter = (uint16_t)((irq_counter & 0x00FF) | (data << 8));
            irq_pending = false;
            return true;
    }
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        // $F800 holds the write protection; anything outside $40-$4E locks
        // all of PRG-RAM, and bits 0-3 lock 2KB quarters.
        bool unlocked = (ram_address & 0xF0) == 0x40 && (ram_address & 0x0F) != 0x0F
                        && !(ram_address & (1 << ((addr - 0x6000) >> 11)));
        return !unlocked; // swallow protected writes
    }
    return false;
}

bool Mapper_019::prgRamMap(uint16_t addr, uint32_t& offset) {
    uint32_t external = prg_ram_size > 0x80 ? prg_ram_size - 0x80 : 0;
    if (addr < 0x6000 || addr > 0x7FFF || external == 0) return false;
    offset = (addr - 0x6000u) % external;
    return true;
}

void Mapper_019::WriteRegister(uint16_t addr, uint8_t data) {
    const int reg = (addr - 0x8000) >> 11; // 0-15, one per $800
    if (reg < 8) {
        chr[reg] = data;
        SetChr1k(reg, data);
    } else if (reg < 12) {
        nt[reg - 8] = data;
    } else if (reg == 12) {
        prg_e000 = data;
        SetPrg8k(0, data & 0x3F);
    } else if (reg == 13) {
        prg_e800 = data;
        SetPrg8k(1, data & 0x3F);
    } else if (reg == 14) {
        SetPrg8k(2, data & 0x3F);
    } else {
        ram_address = data;
    }
}

int Mapper_019::CiramPage(uint16_t addr) const {
    if (addr < 0x2000) {
        uint8_t bank = chr[addr >> 10];
        bool ram_allowed = !(prg_e800 & (addr < 0x1000 ? 0x40 : 0x80));
        return (bank >= 0xE0 && ram_allowed) ? (bank & 1) : -1;
    }
    uint8_t bank = nt[(addr >> 10) & 3];
    return bank >= 0xE0 ? (bank & 1) : -1;
}

bool Mapper_019::ppuReadHook(uint16_t addr, uint8_t& data) {
    if (addr > 0x3EFF) return false;
    int page = CiramPage(addr);
    if (page < 0) return false;
    data = ciram[page * 0x400 + (addr & 0x3FF)];
    return true;
}

bool Mapper_019::ppuWriteHook(uint16_t addr, uint8_t data) {
    if (addr > 0x3EFF) return false;
    int page = CiramPage(addr);
    if (page >= 0) ciram[page * 0x400 + (addr & 0x3FF)] = data;
    return true; // CHR-ROM pages (pattern or nametable) just ignore writes
}

bool Mapper_019::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr >= 0x2000 && addr <= 0x3EFF) { // a nametable served from CHR-ROM
        mapped_addr = (uint32_t)nt[(addr >> 10) & 3] * 0x400 + (addr & 0x3FF);
        return true;
    }
    return BankedMapper::ppuMapRead(addr, mapped_addr);
}

void Mapper_019::cpuClock() {
    if ((irq_counter & 0x8000) && (irq_counter & 0x7FFF) != 0x7FFF) {
        if ((++irq_counter & 0x7FFF) == 0x7FFF) irq_pending = true;
    }

    if (prg_e000 & 0x40) { audio_out = 0.0f; return; } // sound disabled
    if (++channel_timer < 15) return;
    channel_timer = 0;
    UpdateChannel();
}

// One channel's wavetable step, per the documented update sequence.
void Mapper_019::UpdateChannel() {
    const int enabled = ((InternalRam(0x7F) >> 4) & 0x07) + 1; // channels 8 down to 8-(n-1)
    const uint8_t base = (uint8_t)(0x40 + channel * 8);
    uint32_t phase = (uint32_t)(InternalRam(base + 5) << 16 | InternalRam(base + 3) << 8 | InternalRam(base + 1));
    uint32_t freq = (uint32_t)((InternalRam(base + 4) & 0x03) << 16 | InternalRam(base + 2) << 8 | InternalRam(base + 0));
    uint32_t length = 256 - (InternalRam(base + 4) & 0xFC);
    phase = (phase + freq) % (length << 16);
    InternalRam(base + 5) = (uint8_t)(phase >> 16);
    InternalRam(base + 3) = (uint8_t)(phase >> 8);
    InternalRam(base + 1) = (uint8_t)phase;

    uint8_t index = (uint8_t)((phase >> 16) + InternalRam(base + 6));
    int sample = (InternalRam(index >> 1) >> ((index & 1) * 4)) & 0x0F;
    channel_out[channel] = (int16_t)((sample - 8) * (InternalRam(base + 7) & 0x0F));

    // Real hardware outputs the channels one at a time; averaging them is
    // the usual way to emulate that without the switching whine.
    int sum = 0;
    for (int c = 8 - enabled; c < 8; c++) sum += channel_out[c];
    // A lone full-volume channel comes out roughly 13-19dB above an APU
    // pulse depending on the board; this sits in that range.
    audio_out = (float)sum / enabled * 0.0035f;

    channel = channel == 8 - enabled ? 7 : channel - 1;
    if (channel < 8 - enabled) channel = 7;
}

void Mapper_019::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    w.writeBytes(ciram.data(), sizeof(ciram));
    w.writeBytes(chr, sizeof(chr));
    w.writeBytes(nt, sizeof(nt));
    w.write(prg_e000);
    w.write(prg_e800);
    w.write(ram_address);
    w.write(irq_counter);
    w.write(irq_pending);
    w.write(channel_timer);
    w.write(channel);
    w.writeBytes(channel_out, sizeof(channel_out));
    w.write(audio_out);
}

void Mapper_019::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    r.readBytes(ciram.data(), sizeof(ciram));
    r.readBytes(chr, sizeof(chr));
    r.readBytes(nt, sizeof(nt));
    r.read(prg_e000);
    r.read(prg_e800);
    r.read(ram_address);
    r.read(irq_counter);
    r.read(irq_pending);
    r.read(channel_timer);
    r.read(channel);
    r.readBytes(channel_out, sizeof(channel_out));
    r.read(audio_out);
}
