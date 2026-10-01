#pragma once
#include "banked_mapper.h"
#include "i2c_eeprom.h"

// Bandai FCG boards (mappers 16 and 159): Dragon Ball Z, SD Gundam Gaiden,
// Rokudenashi Blues. Registers by the low address nibble:
//   0-7  1KB CHR banks        8  16KB PRG bank at $8000 (last bank fixed)
//   9    mirroring (V, H, one-screen A, B)
//   A    IRQ enable (bit 0); acknowledges, and on the LZ93D50 copies the
//        latch into the counter. Enabling with the counter at 0 fires at once.
//   B/C  IRQ counter low/high - the counter itself on FCG-1/2, a reload
//        latch on the LZ93D50
//   D    LZ93D50 EEPROM lines: bit 5 SCL, bit 6 SDA, bit 7 read direction
// The 16-bit counter counts down every CPU cycle while enabled and raises
// the IRQ when it is 0. The FCG-1/2 (16 submapper 4) decodes $6000-$600F;
// the LZ93D50 (16 submapper 5, and 159) $8000-$800F and returns the
// EEPROM's data line in bit 4 of $6000-$7FFF reads. With no submapper both
// ranges respond. Mapper 16 saves to a 24C02 and 159 to an X24C01 - only
// on boards with a battery (or an NES 2.0 PRG-NVRAM size).
class Mapper_016 : public BankedMapper {
public:
    Mapper_016(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId, uint8_t submapper, bool battery);

    void AttachMemory(const std::vector<uint8_t>* prg, std::vector<uint8_t>* prgram) override {
        BankedMapper::AttachMemory(prg, prgram);
        eeprom.Attach(prgram, eeprom_type);
    }

    bool cpuReadHook(uint16_t addr, uint8_t& data) override;
    bool cpuWriteHook(uint16_t addr, uint8_t data) override;
    bool prgRamMap(uint16_t addr, uint32_t& offset) override { (void)addr; (void)offset; return false; }

    bool irqState() const override { return irq_pending; }
    void cpuClock() override;

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override;

private:
    bool fcg_range, lz_range;   // which register window(s) respond
    bool latched_counter;       // LZ93D50: B/C write a latch, A copies it
    I2cEeprom::Type eeprom_type;
    I2cEeprom eeprom;

    bool irq_enabled = false, irq_pending = false;
    uint16_t irq_counter = 0, irq_latch = 0;

    void Register(uint8_t reg, uint8_t data);
    bool HasEeprom() const { return prg_ram && !prg_ram->empty(); }
};
