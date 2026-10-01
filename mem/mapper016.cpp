#include "mapper016.h"

Mapper_016::Mapper_016(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId, uint8_t submapper, bool battery)
    : BankedMapper(prgBanks, chrBanks) {
    const bool is159 = mapperId == 159;
    fcg_range = !is159 && submapper != 5;
    lz_range = is159 || submapper != 4;
    latched_counter = is159 || submapper == 5;
    eeprom_type = is159 ? I2cEeprom::Type::X24C01 : I2cEeprom::Type::C24C02;
    prg_ram_size = battery ? (is159 ? 128 : 256) : 0; // the EEPROM, when there is one
}

void Mapper_016::Register(uint8_t reg, uint8_t data) {
    switch (reg) {
        case 0x0: case 0x1: case 0x2: case 0x3:
        case 0x4: case 0x5: case 0x6: case 0x7:
            SetChr1k(reg, data);
            break;
        case 0x8: SetPrg16k(0, data & 0x0F); break;
        case 0x9: {
            static const Mirror modes[4] = {Mirror::VERTICAL, Mirror::HORIZONTAL,
                                            Mirror::ONESCREEN_LO, Mirror::ONESCREEN_HI};
            mirror_mode = modes[data & 0x03];
            break;
        }
        case 0xA:
            irq_enabled = data & 0x01;
            irq_pending = false;
            if (lz_range) irq_counter = irq_latch; // FCG-1/2 has no latch to copy
            if (irq_enabled && irq_counter == 0) irq_pending = true;
            break;
        case 0xB:
            irq_latch = (uint16_t)((irq_latch & 0xFF00) | data);
            if (!latched_counter) irq_counter = (uint16_t)((irq_counter & 0xFF00) | data);
            break;
        case 0xC:
            irq_latch = (uint16_t)((irq_latch & 0x00FF) | (data << 8));
            if (!latched_counter) irq_counter = (uint16_t)((irq_counter & 0x00FF) | (data << 8));
            break;
        case 0xD:
            if (HasEeprom()) eeprom.Write(data & 0x20, data & 0x40);
            break;
    }
}

void Mapper_016::WriteRegister(uint16_t addr, uint8_t data) {
    if (lz_range) Register(addr & 0x0F, data);
}

bool Mapper_016::cpuWriteHook(uint16_t addr, uint8_t data) {
    if (addr < 0x6000 || addr > 0x7FFF) return false;
    if (fcg_range) Register(addr & 0x0F, data);
    return true;
}

bool Mapper_016::cpuReadHook(uint16_t addr, uint8_t& data) {
    if (addr < 0x6000 || addr > 0x7FFF) return false;
    data = (lz_range && HasEeprom() && eeprom.Read()) ? 0x10 : 0x00;
    return true;
}

void Mapper_016::cpuClock() {
    if (!irq_enabled) return;
    if (irq_counter == 0) irq_pending = true;
    irq_counter--;
}

void Mapper_016::SerializeState(StateWriter& w) const {
    BankedMapper::SerializeState(w);
    eeprom.Serialize(w);
    w.write(irq_enabled);
    w.write(irq_pending);
    w.write(irq_counter);
    w.write(irq_latch);
}

void Mapper_016::DeserializeState(StateReader& r) {
    BankedMapper::DeserializeState(r);
    eeprom.Deserialize(r);
    r.read(irq_enabled);
    r.read(irq_pending);
    r.read(irq_counter);
    r.read(irq_latch);
}
