#pragma once
#include "banked_mapper.h"
#include "vrc_irq.h"

// Konami VRC2 and VRC4 (mappers 21, 22, 23, 25): Gradius II (J), Wai Wai
// World, TMNT (J), Ganbare Goemon Gaiden. The chips are the same across
// mapper numbers; the boards differ in which two CPU address lines feed the
// chip's register select, so each mapper number accepts every wiring it
// covers at once (they never collide), unless an NES 2.0 submapper names one:
//   21: VRC4a (A1,A2) / VRC4c (A6,A7)
//   22: VRC2a (A1,A0), CHR bank numbers in half-KB units
//   23: VRC2b/VRC4f (A0,A1) / VRC4e (A2,A3)
//   25: VRC2c/VRC4b (A1,A0) / VRC4d (A3,A2)
// Registers (in register-select order 0-3):
//   $8000      8KB PRG bank at $8000 (or $C000 in VRC4 swap mode)
//   $9000      mirroring (VRC2: bit 0 only; VRC4: V/H/1scA/1scB)
//   $9002      VRC4: bit 1 PRG swap mode
//   $A000      8KB PRG bank at $A000
//   $B000-$E003 eight 1KB CHR banks, each split low nibble / high bits
//   $F000-$F003 VRC4 IRQ: latch low, latch high, control, acknowledge
// $C000 (or $8000 when swapped) holds the second-last bank, $E000 the last.
// VRC2's EEPROM-port latch at $6000 reads back what was written, which the
// PRG-RAM here reproduces.
class Mapper_021 : public BankedMapper {
public:
    Mapper_021(uint8_t prgBanks, uint8_t chrBanks, uint8_t mapperId, uint8_t submapper);

    bool irqState() const override { return irq.pending; }
    void cpuClock() override { if (vrc4) irq.Clock(); }

    void SerializeState(StateWriter& w) const override;
    void DeserializeState(StateReader& r) override;

protected:
    void WriteRegister(uint16_t addr, uint8_t data) override;

private:
    bool vrc4 = true;
    bool chr_half = false;    // VRC2a: CHR bank numbers count 512-byte units
    uint8_t wiring[2][2];     // up to two (A0, A1) pairs this board responds to
    int wirings = 1;

    uint8_t prg[2] = {0, 0};
    bool swap_mode = false;
    uint16_t chr[8] = {0};
    VrcIrq irq;

    uint8_t RegisterSelect(uint16_t addr) const;
    void UpdatePrg();
};
