#include "mapper001.h"

void Mapper_001::reset() {
    load_register = 0x00;
    load_register_count = 0;
    control_register = 0x1C;
    chr_bank_select_4lo = 0;
    chr_bank_select_4hi = 0;
    chr_bank_select_8 = 0;
    prg_bank_select_16 = 0;
    prg_bank_select_32 = 0;
    mirror_mode = Mirror::HORIZONTAL;
}

bool Mapper_001::cpuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr < 0x8000) return false;

    uint8_t prg_mode = (control_register >> 2) & 0x03;
    if (prg_mode <= 1) {
        // 32KB mode: low bit of the bank number is ignored
        mapped_addr = (uint32_t)prg_bank_select_32 * 0x8000 + (addr & 0x7FFF);
    } else if (prg_mode == 2) {
        // $8000-$BFFF fixed to bank 0; $C000-$FFFF is the switchable window
        if (addr <= 0xBFFF) mapped_addr = (addr & 0x3FFF);
        else                mapped_addr = (uint32_t)prg_bank_select_16 * 0x4000 + (addr & 0x3FFF);
    } else {
        // mode 3: $8000-$BFFF is the switchable window; $C000-$FFFF fixed to
        // the LAST bank - computed directly here, not stored, so it can
        // never go stale relative to nPRGBanks (real hardware doesn't store
        // it either; it's just wired to "last bank" whenever mode 3 is active).
        if (addr <= 0xBFFF) mapped_addr = (uint32_t)prg_bank_select_16 * 0x4000 + (addr & 0x3FFF);
        else                mapped_addr = (uint32_t)(nPRGBanks - 1) * 0x4000 + (addr & 0x3FFF);
    }
    return true;
}

bool Mapper_001::cpuMapWrite(uint16_t addr, uint32_t& mapped_addr, uint8_t data) {
    (void)mapped_addr; // MMC1 writes to $8000+ are always register writes, never direct memory
    if (addr < 0x8000) return false;

    if (data & 0x80) {
        // Reset: bit 7 set on any write clears the shift register and
        // forces PRG mode back to 3, regardless of what was being shifted in.
        load_register = 0x00;
        load_register_count = 0;
        control_register |= 0x0C;
        return true;
    }

    load_register >>= 1;
    load_register |= (uint8_t)((data & 0x01) << 4);
    load_register_count++;

    if (load_register_count == 5) {
        uint8_t target = (uint8_t)((addr >> 13) & 0x03);

        if (target == 0) {
            control_register = load_register & 0x1F;
            switch (control_register & 0x03) {
                case 0: mirror_mode = Mirror::ONESCREEN_LO; break;
                case 1: mirror_mode = Mirror::ONESCREEN_HI; break;
                case 2: mirror_mode = Mirror::VERTICAL; break;
                case 3: mirror_mode = Mirror::HORIZONTAL; break;
            }
        } else if (target == 1) { // CHR bank 0
            if (control_register & 0x10) chr_bank_select_4lo = load_register & 0x1F;
            else                          chr_bank_select_8   = load_register & 0x1E;
        } else if (target == 2) { // CHR bank 1
            if (control_register & 0x10) chr_bank_select_4hi = load_register & 0x1F;
            // in 8KB CHR mode this register is simply unused
        } else { // target == 3: PRG bank
            uint8_t prg_mode = (control_register >> 2) & 0x03;
            if (prg_mode <= 1) {
                prg_bank_select_32 = (load_register & 0x0E) >> 1;
            } else {
                prg_bank_select_16 = load_register & 0x0F;
            }
        }

        load_register = 0x00;
        load_register_count = 0;
    }

    return true;
}

bool Mapper_001::ppuMapRead(uint16_t addr, uint32_t& mapped_addr) {
    if (addr > 0x1FFF) return false;

    if (nCHRBanks == 0) {
        mapped_addr = addr; // CHR-RAM: direct, no banking
        return true;
    }

    if (control_register & 0x10) {
        // 4KB CHR banks
        mapped_addr = (addr <= 0x0FFF)
            ? (uint32_t)chr_bank_select_4lo * 0x1000 + (addr & 0x0FFF)
            : (uint32_t)chr_bank_select_4hi * 0x1000 + (addr & 0x0FFF);
    } else {
        // 8KB CHR bank (low bit of the selector ignored)
        mapped_addr = (uint32_t)chr_bank_select_8 * 0x1000 + (addr & 0x1FFF);
    }
    return true;
}

bool Mapper_001::ppuMapWrite(uint16_t addr, uint32_t& mapped_addr) {
    if (addr > 0x1FFF) return false;
    if (nCHRBanks != 0) return false; // real CHR-ROM, not writable
    mapped_addr = addr;
    return true;
}
