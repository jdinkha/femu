#pragma once
#include "banked_mapper.h"

// Sunsoft-2 on the Sunsoft-3R board (mapper 93): Shanghai, Fantasy Zone
// (J). $8000-$FFFF register (bus conflicts) with a 16KB PRG bank at $8000
// (bits 4-6, last bank fixed) and a CHR-RAM enable (bit 0) - while clear,
// CHR-RAM ignores writes and reads float.
class Mapper_093 : public BankedMapper {
public:
    Mapper_093(uint8_t prgBanks, uint8_t chrBanks) : BankedMapper(prgBanks, chrBanks) {}

    bool ppuReadHook(uint16_t addr, uint8_t& data) override {
        if (addr > 0x1FFF || chr_enabled) return false;
        data = 0xFF;
        return true;
    }
    bool ppuWriteHook(uint16_t addr, uint8_t data) override {
        (void)data;
        return addr <= 0x1FFF && !chr_enabled; // swallow the write
    }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override {
        data = BusConflict(addr, data);
        SetPrg16k(0, (data >> 4) & 0x07);
        chr_enabled = data & 0x01;
    }

private:
    bool chr_enabled = true;
};
