#include "mapper005.h"
#include "serialize.h"

namespace {
const uint8_t kLengthTable[32] = {10, 254, 20, 2,  40, 4,  80, 6,  160, 8,  60, 10, 14, 12, 26, 14,
                                  12, 16,  24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30};
const uint8_t kDuty[4][8] = {{0, 1, 0, 0, 0, 0, 0, 0},
                             {0, 1, 1, 0, 0, 0, 0, 0},
                             {0, 1, 1, 1, 1, 0, 0, 0},
                             {1, 0, 0, 1, 1, 1, 1, 1}};
} // namespace

Mapper_005::Mapper_005(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    prg_ram_size = 0x10000; // 64KB: a superset every ExROM board's RAM layout fits in
}

// ============================== PRG ==========================================

bool Mapper_005::PrgTarget(uint16_t addr, bool& is_ram, uint32_t& offset) const {
    if (addr < 0x6000) return false;
    if (addr < 0x8000) {
        is_ram = true;
        offset = (prg_regs[0] & 0x07) * 0x2000u + (addr & 0x1FFF);
        return true;
    }
    // Which register covers addr, and how big its window is, per PRG mode.
    int reg = 4, size = 8;
    switch (prg_mode) {
        case 0: reg = 4; size = 32; break;
        case 1: reg = addr < 0xC000 ? 2 : 4; size = 16; break;
        case 2:
            if (addr < 0xC000)      { reg = 2; size = 16; }
            else if (addr < 0xE000) { reg = 3; size = 8; }
            else                    { reg = 4; size = 8; }
            break;
        case 3: reg = 1 + ((addr - 0x8000) >> 13); size = 8; break;
    }
    const uint8_t value = prg_regs[reg];
    is_ram = reg != 4 && !(value & 0x80);
    if (is_ram) {
        uint32_t bank = value & (size == 16 ? 0x06 : 0x07);
        offset = bank * 0x2000 + (addr & (size == 16 ? 0x3FFF : 0x1FFF));
    } else {
        uint32_t bits = value & 0x7F;
        if (size == 32)      offset = (bits >> 2) * 0x8000 + (addr & 0x7FFF);
        else if (size == 16) offset = (bits >> 1) * 0x4000 + (addr & 0x3FFF);
        else                 offset = bits * 0x2000 + (addr & 0x1FFF);
    }
    return true;
}

bool Mapper_005::prgRamMap(uint16_t addr, uint32_t& offset) {
    bool is_ram = false;
    return PrgTarget(addr, is_ram, offset) && is_ram;
}

bool Mapper_005::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    bool is_ram = false;
    return addr >= 0x8000 && PrgTarget(addr, is_ram, mapped_addr) && !is_ram;
}

bool Mapper_005::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    (void)mapped_addr; (void)data;
    return addr >= 0x8000; // ROM: nothing to do
}

// ============================== CPU registers ================================

bool Mapper_005::cpuReadHook(uint16_t addr, uint8_t& data) {
    if (addr == 0xFFFA || addr == 0xFFFB) { // NMI vector fetch: the frame is over
        in_frame = false;
        irq_pending = false;
        irq_counter = 0;
        return false;
    }
    if ((pcm_mode & 0x01) && addr >= 0x8000 && addr <= 0xBFFF) { // PCM read mode
        uint32_t offset = 0;
        if (cpuMapRead(addr, offset) && prg_rom && !prg_rom->empty()) {
            uint8_t v = (*prg_rom)[offset % prg_rom->size()];
            if (v) pcm_value = v; else pcm_irq = true;
        }
        return false;
    }
    switch (addr) {
        case 0x5010:
            data = (uint8_t)((pcm_irq ? 0x80 : 0x00) | 0x01);
            pcm_irq = false;
            return true;
        case 0x5015:
            data = (uint8_t)((pulse[0].length ? 0x01 : 0) | (pulse[1].length ? 0x02 : 0));
            return true;
        case 0x5204:
            data = (uint8_t)((irq_pending ? 0x80 : 0) | (in_frame ? 0x40 : 0));
            irq_pending = false;
            return true;
        case 0x5205: data = (uint8_t)((mult_a * mult_b) & 0xFF); return true;
        case 0x5206: data = (uint8_t)((mult_a * mult_b) >> 8); return true;
    }
    if (addr >= 0x5C00 && addr <= 0x5FFF) {
        data = exram_mode >= 2 ? exram[addr & 0x3FF] : 0x00; // modes 0/1: open bus
        return true;
    }
    return false;
}

bool Mapper_005::cpuWriteHook(uint16_t addr, uint8_t data) {
    if (addr == 0x2000) { sprite_8x16 = data & 0x20; return false; } // the PPU still gets it
    if (addr == 0x2001) {
        bool on = (data & 0x18) != 0;
        if (on && !substitutions) { in_frame = false; irq_pending = false; irq_counter = 0; }
        substitutions = on;
        return false;
    }
    if (addr >= 0x5000 && addr <= 0x5015) { AudioWrite(addr, data); return true; }
    if (addr >= 0x5C00 && addr <= 0x5FFF) {
        if (exram_mode != 3) exram[addr & 0x3FF] = data;
        return true;
    }
    if (addr >= 0x5100 && addr <= 0x5206) {
        switch (addr) {
            case 0x5100: prg_mode = data & 0x03; break;
            case 0x5101: chr_mode = data & 0x03; break;
            case 0x5102: ram_protect[0] = data & 0x03; break;
            case 0x5103: ram_protect[1] = data & 0x03; break;
            case 0x5104: exram_mode = data & 0x03; break;
            case 0x5105: nt_mapping = data; break;
            case 0x5106: fill_tile = data; break;
            case 0x5107: fill_attr = data & 0x03; break;
            case 0x5113: case 0x5114: case 0x5115: case 0x5116: case 0x5117:
                prg_regs[addr - 0x5113] = data;
                break;
            case 0x5130: chr_upper = data & 0x03; break;
            case 0x5200: split_control = data; break;
            case 0x5201: split_scroll = data; break;
            case 0x5202: split_bank = data; break;
            case 0x5203: irq_compare = data; break;
            case 0x5204: irq_enabled = data & 0x80; break;
            case 0x5205: mult_a = data; break;
            case 0x5206: mult_b = data; break;
            default:
                if (addr >= 0x5120 && addr <= 0x512B) WriteChr(addr - 0x5120, data);
                break;
        }
        return true;
    }
    if (addr >= 0x6000) { // PRG-RAM writes need $5102=2 and $5103=1
        bool is_ram = false;
        uint32_t offset = 0;
        bool unlocked = ram_protect[0] == 0x02 && ram_protect[1] == 0x01;
        if (PrgTarget(addr, is_ram, offset) && is_ram && !unlocked) return true; // swallowed
    }
    return false;
}

// ============================== CHR ==========================================

// The bank registers are 10 bits; which data/$5130 bits land where depends
// on the CHR mode at write time, and how they're read out on the mode at
// fetch time - exactly as on the chip, so switching modes mid-game works.
void Mapper_005::WriteChr(int index, uint8_t data) {
    uint16_t value = 0;
    switch (chr_mode) {
        case 3: value = (uint16_t)((chr_upper << 8) | data); break;
        case 2: value = (uint16_t)(((chr_upper & 1) << 9) | (data << 1) | (data & 1)); break;
        case 1: value = (uint16_t)((data << 2) | (data & 1)); break;
        case 0: value = (uint16_t)(((data << 3) & 0x3F8) | (data & 1)); break;
    }
    chr_regs[index] = value;
    last_set_b = index >= 8;
}

uint32_t Mapper_005::ChrOffset(uint16_t addr) const {
    const bool bg_render = Rendering() && !sprite_fetch;
    if (bg_render && in_split) return split_bank * 0x1000u + ((addr & 0x0FF8) | (split_y & 0x07));
    if (bg_render && ExtendedAttributes()) {
        uint32_t bank = (uint32_t)(chr_upper << 6) | (exattr & 0x3F);
        return bank * 0x1000 + (addr & 0x0FFF);
    }

    bool set_b = false;
    if (sprite_8x16 && substitutions) set_b = Rendering() ? !sprite_fetch : last_set_b;

    uint32_t reg;
    switch (chr_mode) {
        case 0:
            reg = chr_regs[set_b ? 11 : 7];
            return (reg >> 3) * 0x2000 + (addr & 0x1FFF);
        case 1:
            reg = chr_regs[set_b ? 11 : ((addr & 0x1000) ? 7 : 3)];
            return (reg >> 2) * 0x1000 + (addr & 0x0FFF);
        case 2: {
            static const int a[4] = {1, 3, 5, 7};
            int q = addr >> 11;
            reg = chr_regs[set_b ? ((q & 1) ? 11 : 9) : a[q]];
            return (reg >> 1) * 0x800 + (addr & 0x07FF);
        }
        default: {
            int w = addr >> 10;
            reg = chr_regs[set_b ? 8 + (w & 3) : w];
            return reg * 0x400 + (addr & 0x03FF);
        }
    }
}

bool Mapper_005::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr > 0x1FFF) return false;
    mapped_addr = ChrOffset(addr);
    return true;
}

bool Mapper_005::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr > 0x1FFF || nCHRBanks != 0) return false;
    mapped_addr = ChrOffset(addr);
    return true;
}

// ============================== Nametables ===================================

int Mapper_005::nametablePage(uint8_t quadrant) const {
    return (nt_mapping >> (quadrant * 2)) & 0x01; // only consulted for CIRAM quadrants
}

bool Mapper_005::ppuReadHook(uint16_t addr, uint8_t& data) {
    if (addr < 0x2000 || addr > 0x3EFF) return false;
    const uint16_t off = addr & 0x3FF;
    const bool attribute = off >= 0x3C0;
    const int source = (nt_mapping >> (((addr >> 10) & 3) * 2)) & 0x03;

    if (Rendering() && !sprite_fetch) {
        if (!attribute) {
            // A background nametable fetch. Tiles 2-33 of a scanline come
            // after the sprite fetch; tiles 0-1 are the next line's prefetch;
            // fetches 34-35 are the PPU's two dummy reads.
            int fetch = nt_fetches++;
            int column = fetch < 32 ? fetch + 2 : fetch - 32;
            in_split = false;
            if (fetch < 34 && (split_control & 0x80) && exram_mode <= 1) {
                int threshold = split_control & 0x1F;
                in_split = (split_control & 0x40) ? column >= threshold : column < threshold;
            }
            if (in_split) {
                split_column = (uint8_t)(column & 31);
                split_y = (uint8_t)((split_scroll + scanline + (fetch >= 32 ? 1 : 0)) % 240);
                data = exram[(split_y / 8) * 32 + split_column];
                return true;
            }
            if (ExtendedAttributes()) exattr = exram[off];
        } else if (in_split) {
            uint8_t attr = exram[0x3C0 + (split_y / 32) * 8 + split_column / 4];
            int shift = ((split_y / 16) & 1) * 4 + ((split_column / 2) & 1) * 2;
            data = (uint8_t)(((attr >> shift) & 0x03) * 0x55);
            return true;
        } else if (ExtendedAttributes()) {
            data = (uint8_t)((exattr >> 6) * 0x55);
            return true;
        }
    }

    switch (source) {
        case 2: data = exram_mode <= 1 ? exram[off] : 0x00; return true;
        case 3: data = attribute ? (uint8_t)(fill_attr * 0x55) : fill_tile; return true;
        default: return false; // CIRAM, via nametablePage
    }
}

bool Mapper_005::ppuWriteHook(uint16_t addr, uint8_t data) {
    if (addr < 0x2000 || addr > 0x3EFF) return false;
    const int source = (nt_mapping >> (((addr >> 10) & 3) * 2)) & 0x03;
    if (source == 2) {
        if (exram_mode <= 1) exram[addr & 0x3FF] = data;
        return true;
    }
    return source == 3; // fill mode ignores writes; CIRAM takes them normally
}

// ============================== Scanline IRQ =================================

void Mapper_005::ppuScanline(int16_t line, bool rendering) {
    scanline = line;
    if (rendering && line >= 0 && line < 240) {
        if (!in_frame) {
            in_frame = true;
            irq_counter = 0;
            irq_pending = false;
        } else if (++irq_counter == irq_compare && irq_compare != 0) {
            irq_pending = true;
        }
    } else {
        in_frame = false;
        if (line == 241) { irq_pending = false; irq_counter = 0; }
    }
}

// ============================== Audio ========================================

void Mapper_005::AudioWrite(uint16_t addr, uint8_t data) {
    if (addr <= 0x5007) {
        Pulse& p = pulse[(addr >> 2) & 1];
        switch (addr & 3) {
            case 0: p.control = data; break;
            case 1: break; // no sweep unit
            case 2: p.period = (uint16_t)((p.period & 0x0700) | data); break;
            case 3:
                p.period = (uint16_t)((p.period & 0x00FF) | ((data & 0x07) << 8));
                if (p.enabled) p.length = kLengthTable[data >> 3];
                p.env_start = true;
                p.step = 0;
                break;
        }
    } else if (addr == 0x5010) {
        pcm_mode = data;
    } else if (addr == 0x5011) {
        if (!(pcm_mode & 0x01)) {
            if (data) pcm_value = data; else pcm_irq = true;
        }
    } else if (addr == 0x5015) {
        for (int i = 0; i < 2; i++) {
            pulse[i].enabled = data & (1 << i);
            if (!pulse[i].enabled) pulse[i].length = 0;
        }
    }
}

void Mapper_005::AudioClock() {
    apu_phase = !apu_phase;
    if (apu_phase) { // pulse timers tick at the APU rate, every other CPU cycle
        for (Pulse& p : pulse) {
            if (p.timer == 0) { p.timer = p.period; p.step = (p.step + 1) & 7; }
            else              p.timer--;
        }
    }

    // Envelopes and length counters run off a fixed ~240Hz clock.
    if (++frame_timer >= 7424) {
        frame_timer = 0;
        for (Pulse& p : pulse) {
            if (p.env_start) {
                p.env_start = false;
                p.env_decay = 15;
                p.env_divider = p.control & 0x0F;
            } else if (p.env_divider == 0) {
                p.env_divider = p.control & 0x0F;
                if (p.env_decay) p.env_decay--;
                else if (p.control & 0x20) p.env_decay = 15;
            } else {
                p.env_divider--;
            }
            if (p.length && !(p.control & 0x20)) p.length--;
        }
    }

    // Same mixing curves as the APU: the pulses match the APU pulses, and
    // the 8-bit PCM matches the DMC at half its value.
    int sum = 0;
    for (const Pulse& p : pulse) {
        if (!p.length || !kDuty[p.control >> 6][p.step]) continue;
        sum += (p.control & 0x10) ? (p.control & 0x0F) : p.env_decay;
    }
    float out = sum ? 95.88f / (8128.0f / sum + 100.0f) : 0.0f;
    if (pcm_value) out += 159.79f / (22638.0f / (pcm_value / 2.0f) + 100.0f);
    audio_out = out;
}

// ============================== Savestates ===================================

void Mapper_005::SerializeState(StateWriter& w) const {
    w.write(prg_mode); w.write(chr_mode);
    w.writeBytes(ram_protect, sizeof(ram_protect));
    w.write(exram_mode); w.write(nt_mapping); w.write(fill_tile); w.write(fill_attr);
    w.writeBytes(prg_regs, sizeof(prg_regs));
    w.writeBytes(chr_regs, sizeof(chr_regs));
    w.write(chr_upper); w.write(last_set_b);
    w.writeBytes(exram.data(), sizeof(exram));
    w.write(sprite_8x16); w.write(substitutions); w.write(sprite_fetch); w.write(scanline);
    w.write(exattr); w.write(nt_fetches); w.write(in_split); w.write(split_column); w.write(split_y);
    w.write(split_control); w.write(split_scroll); w.write(split_bank);
    w.write(irq_compare); w.write(irq_enabled); w.write(irq_pending); w.write(in_frame);
    w.write(irq_counter); w.write(mult_a); w.write(mult_b);
    w.write(pulse[0]); w.write(pulse[1]);
    w.write(pcm_mode); w.write(pcm_value); w.write(pcm_irq);
    w.write(frame_timer); w.write(apu_phase); w.write(audio_out);
}

void Mapper_005::DeserializeState(StateReader& r) {
    r.read(prg_mode); r.read(chr_mode);
    r.readBytes(ram_protect, sizeof(ram_protect));
    r.read(exram_mode); r.read(nt_mapping); r.read(fill_tile); r.read(fill_attr);
    r.readBytes(prg_regs, sizeof(prg_regs));
    r.readBytes(chr_regs, sizeof(chr_regs));
    r.read(chr_upper); r.read(last_set_b);
    r.readBytes(exram.data(), sizeof(exram));
    r.read(sprite_8x16); r.read(substitutions); r.read(sprite_fetch); r.read(scanline);
    r.read(exattr); r.read(nt_fetches); r.read(in_split); r.read(split_column); r.read(split_y);
    r.read(split_control); r.read(split_scroll); r.read(split_bank);
    r.read(irq_compare); r.read(irq_enabled); r.read(irq_pending); r.read(in_frame);
    r.read(irq_counter); r.read(mult_a); r.read(mult_b);
    r.read(pulse[0]); r.read(pulse[1]);
    r.read(pcm_mode); r.read(pcm_value); r.read(pcm_irq);
    r.read(frame_timer); r.read(apu_phase); r.read(audio_out);
}
