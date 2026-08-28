#include "ppu.h"

const std::array<uint8_t, 64 * 3> PPU::paletteRGB = { {
    84,84,84,    0,30,116,   8,16,144,   48,0,136,   68,0,100,   92,0,48,    84,4,0,     60,24,0,
    32,42,0,     8,58,0,     0,64,0,     0,60,0,     0,50,60,    0,0,0,      0,0,0,      0,0,0,
    152,150,152, 8,76,196,   48,50,236,  92,30,228,  136,20,176, 160,20,100, 152,34,32,  120,60,0,
    84,90,0,     40,114,0,   8,124,0,    0,118,40,   0,102,120,  0,0,0,      0,0,0,      0,0,0,
    236,238,236, 76,154,236, 120,124,236,176,98,236, 228,84,236, 236,88,180, 236,106,100,212,136,32,
    160,170,0,   116,196,0,  76,208,32,  56,204,108, 56,180,204, 60,60,60,   0,0,0,      0,0,0,
    236,238,236, 168,204,236,188,188,236,212,178,236,236,174,236,236,174,212,236,180,176,228,196,144,
    204,210,120, 180,222,120,168,226,144,152,226,180,160,214,228,160,162,160,0,0,0,      0,0,0
} };

PPU::PPU() {}

void PPU::ConnectCartridge(const std::shared_ptr<Cartridge>& c) { cart = c; }

void PPU::reset() {
    fine_x = 0;
    address_latch = false;
    ppu_data_buffer = 0x00;
    scanline = -1;
    cycle = 0;
    bg_next_tile_id = 0x00;
    bg_next_tile_attrib = 0x00;
    bg_next_tile_lsb = 0x00;
    bg_next_tile_msb = 0x00;
    bg_shifter_pattern_lo = 0x0000;
    bg_shifter_pattern_hi = 0x0000;
    bg_shifter_attrib_lo = 0x0000;
    bg_shifter_attrib_hi = 0x0000;
    status.reg = 0x00;
    mask.reg = 0x00;
    control.reg = 0x00;
    vram_addr.reg = 0x0000;
    tram_addr.reg = 0x0000;
    frame_complete = false;
    nmi = false;
}

// ============================ PPU-facing bus ==================================

uint8_t PPU::ppuRead(uint16_t addr) {
    addr &= 0x3FFF;
    uint8_t data = 0x00;

    if (cart && cart->ppuRead(addr, data)) {
        return data;
    } else if (addr <= 0x1FFF) {
        return 0x00; // no cartridge / no CHR data available
    } else if (addr <= 0x3EFF) {
        addr &= 0x0FFF;
        if (cart->mirror == Mirror::VERTICAL) {
            if (addr <= 0x03FF)      data = nameTable[0][addr & 0x03FF];
            else if (addr <= 0x07FF) data = nameTable[1][addr & 0x03FF];
            else if (addr <= 0x0BFF) data = nameTable[0][addr & 0x03FF];
            else                     data = nameTable[1][addr & 0x03FF];
        } else { // HORIZONTAL
            if (addr <= 0x03FF)      data = nameTable[0][addr & 0x03FF];
            else if (addr <= 0x07FF) data = nameTable[0][addr & 0x03FF];
            else if (addr <= 0x0BFF) data = nameTable[1][addr & 0x03FF];
            else                     data = nameTable[1][addr & 0x03FF];
        }
    } else { // addr <= 0x3FFF
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000;
        if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008;
        if (addr == 0x001C) addr = 0x000C;
        data = paletteTable[addr];
    }
    return data;
}

void PPU::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x3FFF;

    if (cart && cart->ppuWrite(addr, data)) {
        return;
    } else if (addr <= 0x1FFF) {
        // CHR-ROM: nothing to do (cart->ppuWrite already handled the CHR-RAM case above)
    } else if (addr <= 0x3EFF) {
        addr &= 0x0FFF;
        if (cart->mirror == Mirror::VERTICAL) {
            if (addr <= 0x03FF)      nameTable[0][addr & 0x03FF] = data;
            else if (addr <= 0x07FF) nameTable[1][addr & 0x03FF] = data;
            else if (addr <= 0x0BFF) nameTable[0][addr & 0x03FF] = data;
            else                     nameTable[1][addr & 0x03FF] = data;
        } else {
            if (addr <= 0x03FF)      nameTable[0][addr & 0x03FF] = data;
            else if (addr <= 0x07FF) nameTable[0][addr & 0x03FF] = data;
            else if (addr <= 0x0BFF) nameTable[1][addr & 0x03FF] = data;
            else                     nameTable[1][addr & 0x03FF] = data;
        }
    } else { // addr <= 0x3FFF
        addr &= 0x001F;
        if (addr == 0x0010) addr = 0x0000;
        if (addr == 0x0014) addr = 0x0004;
        if (addr == 0x0018) addr = 0x0008;
        if (addr == 0x001C) addr = 0x000C;
        paletteTable[addr] = data;
    }
}

// ============================ CPU-facing registers =============================

uint8_t PPU::cpuRead(uint16_t addr, bool readOnly) {
    uint8_t data = 0x00;

    if (readOnly) {
        // Peek-only path for debuggers - no side effects (no clearing vblank,
        // no advancing the address latch, no bumping vram_addr).
        switch (addr) {
            case 0x0000: data = control.reg; break;
            case 0x0001: data = mask.reg; break;
            case 0x0002: data = status.reg; break;
            case 0x0004: data = OAM[oam_addr]; break;
            case 0x0007: data = ppu_data_buffer; break;
            default: break;
        }
        return data;
    }

    switch (addr) {
        case 0x0000: break; // PPUCTRL - write only
        case 0x0001: break; // PPUMASK - write only
        case 0x0002: // PPUSTATUS
            data = (status.reg & 0xE0) | (ppu_data_buffer & 0x1F);
            status.vertical_blank = 0;
            address_latch = false;
            break;
        case 0x0003: break; // OAMADDR - write only
        case 0x0004: // OAMDATA
            data = OAM[oam_addr];
            break;
        case 0x0005: break; // PPUSCROLL - write only
        case 0x0006: break; // PPUADDR - write only
        case 0x0007: // PPUDATA
            data = ppu_data_buffer;
            ppu_data_buffer = ppuRead(vram_addr.reg);
            if (vram_addr.reg >= 0x3F00) data = ppu_data_buffer; // palette reads aren't delayed
            vram_addr.reg += (control.increment_mode ? 32 : 1);
            break;
        default: break;
    }
    return data;
}

void PPU::cpuWrite(uint16_t addr, uint8_t data) {
    switch (addr) {
        case 0x0000: // PPUCTRL
            control.reg = data;
            tram_addr.nametable_x = control.nametable_x;
            tram_addr.nametable_y = control.nametable_y;
            break;
        case 0x0001: // PPUMASK
            mask.reg = data;
            break;
        case 0x0002: break; // PPUSTATUS - read only
        case 0x0003: // OAMADDR
            oam_addr = data;
            break;
        case 0x0004: // OAMDATA
            OAM[oam_addr] = data;
            break;
        case 0x0005: // PPUSCROLL
            if (!address_latch) {
                fine_x = data & 0x07;
                tram_addr.coarse_x = data >> 3;
                address_latch = true;
            } else {
                tram_addr.fine_y = data & 0x07;
                tram_addr.coarse_y = data >> 3;
                address_latch = false;
            }
            break;
        case 0x0006: // PPUADDR
            if (!address_latch) {
                tram_addr.reg = (uint16_t)(((data & 0x3F) << 8) | (tram_addr.reg & 0x00FF));
                address_latch = true;
            } else {
                tram_addr.reg = (uint16_t)((tram_addr.reg & 0xFF00) | data);
                vram_addr = tram_addr;
                address_latch = false;
            }
            break;
        case 0x0007: // PPUDATA
            ppuWrite(vram_addr.reg, data);
            vram_addr.reg += (control.increment_mode ? 32 : 1);
            break;
        default: break;
    }
}

// ======================= Background rendering pipeline =========================

void PPU::IncrementScrollX() {
    if (mask.render_background || mask.render_sprites) {
        if (vram_addr.coarse_x == 31) {
            vram_addr.coarse_x = 0;
            vram_addr.nametable_x = ~vram_addr.nametable_x;
        } else {
            vram_addr.coarse_x++;
        }
    }
}

void PPU::IncrementScrollY() {
    if (mask.render_background || mask.render_sprites) {
        if (vram_addr.fine_y < 7) {
            vram_addr.fine_y++;
        } else {
            vram_addr.fine_y = 0;
            if (vram_addr.coarse_y == 29) {
                vram_addr.coarse_y = 0;
                vram_addr.nametable_y = ~vram_addr.nametable_y;
            } else if (vram_addr.coarse_y == 31) {
                vram_addr.coarse_y = 0; // attribute-table rows past 29 wrap without flipping nametable
            } else {
                vram_addr.coarse_y++;
            }
        }
    }
}

void PPU::TransferAddressX() {
    if (mask.render_background || mask.render_sprites) {
        vram_addr.nametable_x = tram_addr.nametable_x;
        vram_addr.coarse_x = tram_addr.coarse_x;
    }
}

void PPU::TransferAddressY() {
    if (mask.render_background || mask.render_sprites) {
        vram_addr.fine_y = tram_addr.fine_y;
        vram_addr.nametable_y = tram_addr.nametable_y;
        vram_addr.coarse_y = tram_addr.coarse_y;
    }
}

void PPU::LoadBackgroundShifters() {
    bg_shifter_pattern_lo = (uint16_t)((bg_shifter_pattern_lo & 0xFF00) | bg_next_tile_lsb);
    bg_shifter_pattern_hi = (uint16_t)((bg_shifter_pattern_hi & 0xFF00) | bg_next_tile_msb);
    bg_shifter_attrib_lo = (uint16_t)((bg_shifter_attrib_lo & 0xFF00) | ((bg_next_tile_attrib & 0b01) ? 0xFF : 0x00));
    bg_shifter_attrib_hi = (uint16_t)((bg_shifter_attrib_hi & 0xFF00) | ((bg_next_tile_attrib & 0b10) ? 0xFF : 0x00));
}

void PPU::UpdateShifters() {
    if (mask.render_background) {
        bg_shifter_pattern_lo <<= 1;
        bg_shifter_pattern_hi <<= 1;
        bg_shifter_attrib_lo <<= 1;
        bg_shifter_attrib_hi <<= 1;
    }
}

uint8_t PPU::GetColorFromPaletteRam(uint8_t palette, uint8_t pixel) {
    return ppuRead((uint16_t)(0x3F00 + (palette << 2) + pixel)) & 0x3F;
}

void PPU::SetPixel(int x, int y, uint8_t colorIndex) {
    if (x < 0 || x >= 256 || y < 0 || y >= 240) return;
    size_t base = (size_t)(y * 256 + x) * 3;
    framebuffer[base + 0] = paletteRGB[(size_t)colorIndex * 3 + 0];
    framebuffer[base + 1] = paletteRGB[(size_t)colorIndex * 3 + 1];
    framebuffer[base + 2] = paletteRGB[(size_t)colorIndex * 3 + 2];
}

void PPU::clock() {
    bool renderLine = (scanline >= -1 && scanline < 240);

    if (scanline == -1 && cycle == 1) {
        status.vertical_blank = 0;
        status.sprite_zero_hit = 0;
        status.sprite_overflow = 0;
    }

    if (renderLine) {
        if ((cycle >= 2 && cycle < 258) || (cycle >= 321 && cycle < 338)) {
            UpdateShifters();

            switch ((cycle - 1) % 8) {
                case 0:
                    LoadBackgroundShifters();
                    bg_next_tile_id = ppuRead((uint16_t)(0x2000 | (vram_addr.reg & 0x0FFF)));
                    break;
                case 2:
                    bg_next_tile_attrib = ppuRead((uint16_t)(0x23C0
                        | (vram_addr.nametable_y << 11)
                        | (vram_addr.nametable_x << 10)
                        | ((vram_addr.coarse_y >> 2) << 3)
                        | (vram_addr.coarse_x >> 2)));
                    if (vram_addr.coarse_y & 0x02) bg_next_tile_attrib >>= 4;
                    if (vram_addr.coarse_x & 0x02) bg_next_tile_attrib >>= 2;
                    bg_next_tile_attrib &= 0x03;
                    break;
                case 4:
                    bg_next_tile_lsb = ppuRead((uint16_t)(((uint16_t)control.pattern_background << 12)
                        + ((uint16_t)bg_next_tile_id << 4)
                        + vram_addr.fine_y));
                    break;
                case 6:
                    bg_next_tile_msb = ppuRead((uint16_t)(((uint16_t)control.pattern_background << 12)
                        + ((uint16_t)bg_next_tile_id << 4)
                        + vram_addr.fine_y + 8));
                    break;
                case 7:
                    IncrementScrollX();
                    break;
                default: break;
            }
        }

        if (cycle == 256) {
            IncrementScrollY();
        }
        if (cycle == 257) {
            LoadBackgroundShifters();
            TransferAddressX();
        }
        if (cycle == 338 || cycle == 340) {
            bg_next_tile_id = ppuRead((uint16_t)(0x2000 | (vram_addr.reg & 0x0FFF)));
        }
        if (scanline == -1 && cycle >= 280 && cycle < 305) {
            TransferAddressY();
        }
    }

    if (scanline == 241 && cycle == 1) {
        status.vertical_blank = 1;
        if (control.enable_nmi) nmi = true;
    }

    // Composite this cycle's pixel (visible area only)
    if (scanline >= 0 && scanline < 240 && cycle >= 1 && cycle <= 256) {
        uint8_t bg_pixel = 0x00, bg_palette = 0x00;
        if (mask.render_background) {
            uint16_t bit_mux = (uint16_t)(0x8000 >> fine_x);
            uint8_t p0 = (bg_shifter_pattern_lo & bit_mux) > 0;
            uint8_t p1 = (bg_shifter_pattern_hi & bit_mux) > 0;
            bg_pixel = (uint8_t)((p1 << 1) | p0);

            uint8_t pal0 = (bg_shifter_attrib_lo & bit_mux) > 0;
            uint8_t pal1 = (bg_shifter_attrib_hi & bit_mux) > 0;
            bg_palette = (uint8_t)((pal1 << 1) | pal0);
        }
        SetPixel(cycle - 1, scanline, GetColorFromPaletteRam(bg_palette, bg_pixel));
    }

    cycle++;
    if (cycle >= 341) {
        cycle = 0;
        scanline++;
        if (scanline >= 261) {
            scanline = -1;
            frame_complete = true;
        }
    }
}
