#include "mapper004.h"
#include "serialize.h"

void Mapper_004::SerializeState(StateWriter& w) const {
    w.write(target_register);
    w.write(prg_bank_mode);
    w.write(chr_a12_invert);
    w.writeBytes(reg.data(), sizeof(reg));
    w.write(mirror_mode);
    w.write(prg_ram_enabled);
    w.write(prg_ram_writable);
    w.write(irq_enabled);
    w.write(irq_reload);
    w.write(irq_latch);
    w.write(irq_counter);
    w.write(irq_pending);
}

void Mapper_004::DeserializeState(StateReader& r) {
    r.read(target_register);
    r.read(prg_bank_mode);
    r.read(chr_a12_invert);
    r.readBytes(reg.data(), sizeof(reg));
    r.read(mirror_mode);
    r.read(prg_ram_enabled);
    r.read(prg_ram_writable);
    r.read(irq_enabled);
    r.read(irq_reload);
    r.read(irq_latch);
    r.read(irq_counter);
    r.read(irq_pending);
    UpdateBanks(); // the bank maps are derived from the registers
}

void Mapper_004::reset() {
    target_register = 0;
    prg_bank_mode = false;
    chr_a12_invert = false;
    reg.fill(0);
    mirror_mode = Mirror::HORIZONTAL;
    prg_ram_enabled = true;
    prg_ram_writable = true;
    irq_enabled = false;
    irq_reload = false;
    irq_latch = 0;
    irq_counter = 0;
    irq_pending = false;
    UpdateBanks();
}

uint32_t Mapper_004::PrgBank(uint8_t raw) const {
    uint32_t nPRG8k = (uint32_t)nPRGBanks * 2; // total 8KB PRG banks available
    // $FE/$FF from the fixed windows mean "second-last"/"last", which a plain
    // modulo gets wrong for the odd non-power-of-two dump.
    if (raw >= 0xFE && (nPRG8k & (nPRG8k - 1)) != 0 && nPRG8k >= 2) return (nPRG8k - (0x100u - raw)) * 0x2000;
    return (raw % nPRG8k) * 0x2000;
}

uint32_t Mapper_004::ChrBank(uint8_t raw) const {
    // The MMC3 banks CHR-RAM (TGROM/TNROM) exactly like CHR-ROM.
    uint32_t nCHR1k = nCHRBanks ? (uint32_t)nCHRBanks * 8 : chr_ram_size / 0x400;
    if (nCHR1k == 0) nCHR1k = 8;
    return (raw % nCHR1k) * 0x400;
}

void Mapper_004::UpdateBanks() {
    if (!prg_bank_mode) {
        prg_raw[0] = reg[6];
        prg_raw[1] = reg[7];
        prg_raw[2] = 0xFE;
        prg_raw[3] = 0xFF;
    } else {
        prg_raw[0] = 0xFE;
        prg_raw[1] = reg[7];
        prg_raw[2] = reg[6];
        prg_raw[3] = 0xFF;
    }

    const int lo = chr_a12_invert ? 4 : 0; // where the two 2KB banks go
    const int hi = chr_a12_invert ? 0 : 4; // where the four 1KB banks go
    chr_raw[lo + 0] = reg[0] & 0xFE;
    chr_raw[lo + 1] = reg[0] | 0x01;
    chr_raw[lo + 2] = reg[1] & 0xFE;
    chr_raw[lo + 3] = reg[1] | 0x01;
    for (int i = 0; i < 4; i++) chr_raw[hi + i] = reg[2 + i];

    for (int i = 0; i < 4; i++) prg_offset[i] = PrgBank(prg_raw[i]);
    for (int i = 0; i < 8; i++) {
        chr_offset[i] = ChrBank(chr_raw[i]);
        chr_writable[i] = ChrWritable(chr_raw[i]);
    }
}

bool Mapper_004::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x8000) return false;
    uint8_t window = (uint8_t)((addr - 0x8000) / 0x2000); // 0-3
    mapped_addr = prg_offset[window] + (addr & 0x1FFF);
    return true;
}

bool Mapper_004::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    (void)mapped_addr; // MMC3 registers, never a direct memory write
    if (addr < 0x8000) return false;

    if (addr <= 0x9FFF) {
        if ((addr & 0x01) == 0) {
            // $8000: bank select
            target_register = data & 0x07;
            prg_bank_mode = (data & 0x40) != 0;
            chr_a12_invert = (data & 0x80) != 0;
        } else {
            // $8001: bank data
            reg[target_register] = data;
        }
        UpdateBanks();
    } else if (addr <= 0xBFFF) {
        if ((addr & 0x01) == 0) {
            // $A000: mirroring (ignored on four-screen carts, which we don't support)
            mirror_mode = (data & 0x01) ? Mirror::HORIZONTAL : Mirror::VERTICAL;
        } else {
            // $A001: PRG-RAM protect. bit7 = chip enable, bit6 = write-protect
            // (write-protect isn't enforced on PRG-RAM itself - doing so breaks
            // the MMC6 games, which share this mapper number - but boards that
            // hang a register off the PRG-RAM interface do honor it).
            prg_ram_enabled = (data & 0x80) != 0;
            prg_ram_writable = (data & 0x40) == 0;
        }
    } else if (addr <= 0xDFFF) {
        if ((addr & 0x01) == 0) {
            irq_latch = data; // $C000: reload value for the scanline counter
        } else {
            irq_counter = 0; // $C001: force a reload on the next scanline clock
            irq_reload = true;
        }
    } else {
        if ((addr & 0x01) == 0) {
            irq_enabled = false; // $E000: disable AND acknowledge any pending IRQ
            irq_pending = false;
        } else {
            irq_enabled = true; // $E001: enable
        }
    }
    return true;
}

bool Mapper_004::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr > 0x1FFF) return false;
    uint8_t window = (uint8_t)(addr / 0x400); // 0-7
    mapped_addr = chr_offset[window] + (addr & 0x3FF);
    return true;
}

bool Mapper_004::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr > 0x1FFF) return false;
    uint8_t window = (uint8_t)(addr / 0x400);
    if (!chr_writable[window]) return false; // real CHR-ROM, not writable
    mapped_addr = chr_offset[window] + (addr & 0x3FF);
    return true;
}

void Mapper_004::ScanlineIRQ() {
    if (irq_counter == 0 || irq_reload) {
        irq_counter = irq_latch;
        irq_reload = false;
    } else {
        irq_counter--;
    }
    if (irq_counter == 0 && irq_enabled) {
        irq_pending = true;
    }
}
