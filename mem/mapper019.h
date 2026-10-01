#pragma once
#include "banked_mapper.h"
#include <array>

// Namco 129/163 (mapper 19): Rolling Thunder (J), Megami Tensei II, Final
// Lap, King of Kings. Each register spans $800 bytes:
//   $4800  data port into the chip's 128 bytes of internal RAM (r/w)
//   $5000 / $5800  IRQ counter low / high 7 bits + enable (r/w; writes
//          acknowledge). The 15-bit counter counts CPU cycles up and fires
//          and stops at $7FFF.
//   $8000-$B800  1KB CHR banks; values $E0+ select a page of nametable RAM
//          instead, unless $E800 bit 6 (low half) / 7 (high half) is set
//   $C000-$D800  nametable banks for $2000/$2400/$2800/$2C00: $E0+ is
//          nametable RAM page A/B (even/odd), anything else a CHR-ROM page
//   $E000  8KB PRG at $8000, bit 6 disables sound
//   $E800  8KB PRG at $A000, bits 6/7 as above
//   $F000  8KB PRG at $C000 ($E000 is the last bank)
//   $F800  internal RAM address (bits 0-6) + auto-increment (bit 7); also
//          PRG-RAM write protection: only $40-$4E allow writes, with bits
//          0-3 protecting each 2KB quarter
// Because the chip routes every nametable fetch, the 2KB of nametable RAM
// it hands out lives here. The internal RAM is battery-backed along with
// PRG-RAM on boards that have a battery, so it's kept as the last 128 bytes
// of PRG-RAM. Expansion audio: up to 8 wavetable channels read from that
// same internal RAM, updated one at a time every 15 CPU cycles.
class Mapper_019 : public BankedMapper {
public:
    Mapper_019(uint8_t prgBanks, uint8_t chrBanks);

    void SetRamSizes(uint32_t prg, uint32_t chr) override { BankedMapper::SetRamSizes(prg + 0x80, chr); }

    bool cpuReadHook(uint16_t addr, uint8_t& data) override;
    bool cpuWriteHook(uint16_t addr, uint8_t data) override;
    bool prgRamMap(uint16_t addr, uint32_t& offset) override;

    bool ppuReadHook(uint16_t addr, uint8_t& data) override;
    bool ppuWriteHook(uint16_t addr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mapped_addr) override;

    bool irqState() const override { return irq_pending; }
    void cpuClock() override;
    float audioSample() const override { return audio_out; }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override;

private:
    std::array<uint8_t, 0x800> ciram{}; // the console's nametable RAM, routed through the chip
    uint8_t chr[8] = {0};
    uint8_t nt[4] = {0xE0, 0xE1, 0xE0, 0xE1};
    uint8_t prg_e000 = 0, prg_e800 = 0;
    uint8_t ram_address = 0;           // $F800
    uint16_t irq_counter = 0;          // bit 15 = enable
    bool irq_pending = false;

    uint8_t channel_timer = 0;         // CPU cycles toward the next channel update
    uint8_t channel = 7;               // the channel updated next (7 = "channel 8")
    int16_t channel_out[8] = {0};
    float audio_out = 0.0f;

    uint8_t& InternalRam(uint8_t addr) { return (*prg_ram)[prg_ram->size() - 0x80 + (addr & 0x7F)]; }
    int CiramPage(uint16_t addr) const; // which nametable-RAM page an access hits, or -1
    void UpdateChannel();
};
