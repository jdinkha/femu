#include "mapper004.h"

void Mapper_004::reset() {
    target_register = 0;
    prg_bank_mode = false;
    chr_a12_invert = false;
    reg.fill(0);
    mirror_mode = Mirror::HORIZONTAL;
    prg_ram_enabled = true;
    irq_enabled = false;
    irq_reload = false;
    irq_latch = 0;
    irq_counter = 0;
    irq_pending = false;
    UpdateBanks();
}

void Mapper_004::UpdateBanks() {
    uint16_t nPRG8k = (uint16_t)nPRGBanks * 2; // total 8KB PRG windows available
    uint8_t last = (uint8_t)(nPRG8k - 1);
    uint8_t second_last = (uint8_t)(nPRG8k - 2);

    if (!prg_bank_mode) {
        prg_bank_8k[0] = reg[6] % nPRG8k;
        prg_bank_8k[1] = reg[7] % nPRG8k;
        prg_bank_8k[2] = second_last;
        prg_bank_8k[3] = last;
    } else {
        prg_bank_8k[0] = second_last;
        prg_bank_8k[1] = reg[7] % nPRG8k;
        prg_bank_8k[2] = reg[6] % nPRG8k;
        prg_bank_8k[3] = last;
    }

    uint16_t nCHR1k = (uint16_t)nCHRBanks * 8;
    if (nCHR1k == 0) nCHR1k = 8; // CHR-RAM fallback; MMC3 games essentially always ship CHR-ROM

    if (!chr_a12_invert) {
        chr_bank_1k[0] = (uint8_t)((reg[0] & 0xFE) % nCHR1k);
        chr_bank_1k[1] = (uint8_t)((reg[0] | 0x01) % nCHR1k);
        chr_bank_1k[2] = (uint8_t)((reg[1] & 0xFE) % nCHR1k);
        chr_bank_1k[3] = (uint8_t)((reg[1] | 0x01) % nCHR1k);
        chr_bank_1k[4] = (uint8_t)(reg[2] % nCHR1k);
        chr_bank_1k[5] = (uint8_t)(reg[3] % nCHR1k);
        chr_bank_1k[6] = (uint8_t)(reg[4] % nCHR1k);
        chr_bank_1k[7] = (uint8_t)(reg[5] % nCHR1k);
    } else {
        chr_bank_1k[0] = (uint8_t)(reg[2] % nCHR1k);
        chr_bank_1k[1] = (uint8_t)(reg[3] % nCHR1k);
        chr_bank_1k[2] = (uint8_t)(reg[4] % nCHR1k);
        chr_bank_1k[3] = (uint8_t)(reg[5] % nCHR1k);
        chr_bank_1k[4] = (uint8_t)((reg[0] & 0xFE) % nCHR1k);
        chr_bank_1k[5] = (uint8_t)((reg[0] | 0x01) % nCHR1k);
        chr_bank_1k[6] = (uint8_t)((reg[1] & 0xFE) % nCHR1k);
        chr_bank_1k[7] = (uint8_t)((reg[1] | 0x01) % nCHR1k);
    }
}

bool Mapper_004::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x8000) return false;
    uint8_t window = (uint8_t)((addr - 0x8000) / 0x2000); // 0-3
    mapped_addr = (uint32_t)prg_bank_8k[window] * 0x2000 + (addr & 0x1FFF);
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
            // (write-protect nuance not modeled - rare in practice).
            prg_ram_enabled = (data & 0x80) != 0;
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
    if (nCHRBanks == 0) { mapped_addr = addr; return true; } // CHR-RAM (uncommon for MMC3, but handled)
    uint8_t window = (uint8_t)(addr / 0x400); // 0-7
    mapped_addr = (uint32_t)chr_bank_1k[window] * 0x400 + (addr & 0x3FF);
    return true;
}

bool Mapper_004::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr > 0x1FFF) return false;
    if (nCHRBanks != 0) return false; // real CHR-ROM, not writable
    mapped_addr = addr;
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
